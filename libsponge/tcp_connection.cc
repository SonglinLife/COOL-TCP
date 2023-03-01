#include "tcp_connection.hh"

#include "wrapping_integers.hh"

#include <cstddef>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <utility>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _current_time - _last_rev_seg_time; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    const auto &header = seg.header();
    DebugPrint("receive seg seqno:",
               header.seqno.raw_value(),
               "rst",
               header.rst,
               "syn",
               header.syn,
               "fin",
               header.fin,
               "ack",
               header.ack,
               "ackno",
               header.ackno.raw_value());
    _last_rev_seg_time = _current_time;
    if (header.rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _lived = false;
        return;
    }
    // ********************** receiver ******************
    _receiver.segment_received(seg);
    if (_receiver.ackno().has_value()) {
        // sender may in listening mode, so it was better to knock it.
        _sender.fill_window();
    }

    if (!_sender.stream_in().eof() && _receiver.eof()) {
        // if sender not send the fin seg, and receiver has close, there was no need to wait
        _linger_after_streams_finish = false;
    }

    // **********************  sender *************************
    if (header.ack) {
        // give ackno and win to sender
        _sender.ack_received(header.ackno, header.win);
    }

    // for specally case
    if (seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 &&
        seg.header().seqno.raw_value() == _receiver.ackno().value().raw_value() - 1 && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    pull_sender_push_segment();

    if (_receiver.eof() && _sender.eof() && !_linger_after_streams_finish) {
        DebugPrint("it was close pressive");
        _lived = false;
    }
}

bool TCPConnection::active() const {
    if (!_lived) {
        return false;
    }
    if (_sender.eof() && _receiver.eof() && !_linger_after_streams_finish) {
        return false;
    }
    return true;
}

size_t TCPConnection::write(const string &data) {
    auto &stream = _sender.stream_in();
    auto write_sz = stream.write(data);
    pull_sender_push_segment();
    return write_sz;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _current_time += ms_since_last_tick;
    DebugPrint("tick before time", _current_time - ms_since_last_tick, "tick after time", _current_time);
    _sender.tick(ms_since_last_tick);
    pull_sender_push_segment();
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        DebugPrint("end _sender since over MAX_RETX_ATTEMPTS");
        std::remove_reference_t<decltype(_segments_out)> tmp{};
        _segments_out.swap(tmp);

        auto &sender_q = _sender.segments_out();
        std::remove_reference_t<decltype(_segments_out)> tmp1{};
        sender_q.swap(tmp1);
        _sender.send_empty_segment();
        auto seg = sender_q.front();
        sender_q.pop();
        seg.header().rst = true;
        sender_q.push(seg);
        pull_sender_push_segment();
        // shut down itself
        _lived = false;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    if (_receiver.eof() && _sender.eof()) {
        if (!_linger_after_streams_finish) {
            DebugPrint("close lived with no linger");
            _lived = false;
        } else if (_current_time - _last_rev_seg_time >= _cfg.rt_timeout * 10) {
            DebugPrint("close lived with no linger");
            _lived = false;
            _linger_after_streams_finish = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    auto &stream = _sender.stream_in();
    stream.end_input();
    _sender.fill_window();
    _end_input = true;
    pull_sender_push_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();
    DebugPrint("in connect");
    DebugPrint("tcp sender rto: ", _cfg.rt_timeout, "recv_capacity", _cfg.recv_capacity);
    pull_sender_push_segment();
    DebugPrint("connect done");
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            auto &sender_q = _sender.segments_out();
            std::remove_reference_t<decltype(sender_q)> tmp{};
            sender_q.swap(tmp);
            // clean sender segments_out queue
            _sender.send_empty_segment();
            auto seg = sender_q.front();
            sender_q.pop();
            seg.header().rst = true;
            sender_q.push(seg);
            pull_sender_push_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

size_t TCPConnection::pull_sender_push_segment() {
    auto &sender_q = _sender.segments_out();
    auto win_sz = _receiver.window_size();
    auto ackno_opt = _receiver.ackno();
    auto ack{false};
    auto ackno = WrappingInt32(0);
    if (ackno_opt.has_value()) {
        ack = true;
        ackno = ackno_opt.value();
    }
    auto sz = sender_q.size();
    while (!sender_q.empty()) {
        auto seg = sender_q.front();
        sender_q.pop();
        auto &header = seg.header();
        header.ack = ack;
        header.ackno = ackno;
        header.win = win_sz;
        _segments_out.push(seg);
        DebugPrint("send seg seqno:",
                   header.seqno.raw_value(),
                   "rst",
                   header.rst,
                   "syn",
                   header.syn,
                   "fin",
                   header.fin,
                   "ack",
                   header.ack,
                   "ackno",
                   header.ackno.raw_value());
    }
    return sz;
}
