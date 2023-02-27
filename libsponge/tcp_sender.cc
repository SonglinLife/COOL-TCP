#include "tcp_sender.hh"

#include "buffer.hh"
#include "tcp_config.hh"
#include "tcp_header.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <iostream>
#include <new>
#include <random>
#include <sstream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(_current_time, _initial_retransmission_timeout)
    , _last_ackno(_isn) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - unwrap(_last_ackno, _isn, _next_seqno); }

void TCPSender::fill_window() {
    while (_left_win_sz > 0) {
        TCPHeader header;
        if (_left_win_sz && _next_seqno == 0) {
            header.syn = true;
            _left_win_sz--;
        }
        size_t expect_read_sz = std::min(TCPConfig::MAX_PAYLOAD_SIZE, _left_win_sz);
        auto read_str = _stream.read(expect_read_sz);
        Buffer payload(std::move(read_str));
        _left_win_sz -= payload.size();
        if (_stream.eof() && _left_win_sz && !_fin_sent) {
            header.fin = true;
            _fin_sent = true;
            _left_win_sz--;
        }
        header.seqno = wrap(_next_seqno, _isn);
        TCPSegment seg;
        seg.header() = header;
        seg.payload() = payload;
        if (seg.length_in_sequence_space() > 0) {
            // not send a empty seg
            _segments_out.push(seg);
            _store.push_back(seg);
            _next_seqno += seg.length_in_sequence_space();
        }
        if (_stream.buffer_empty()) {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (unwrap(ackno, _isn, _next_seqno) < unwrap(_last_ackno, _isn, _next_seqno) && window_size <= _win_sz) {
        // it was a out-time ack
        return;
    }
    if (unwrap(ackno, _isn, _next_seqno) > _next_seqno) {
        // invaild ackno
        return;
    }
    if (unwrap(ackno, _isn, _next_seqno) > unwrap(_last_ackno, _isn, _next_seqno)) {
        // it has new data
        _consecutive_retransmissions_cnt = 0;
        _timer.reset(_current_time);
        _last_ackno = ackno;
    }
    _win_sz = window_size;
    decltype(_store) tmp_list;
    size_t store_sz{0};
    for (auto &cur : _store) {
        auto seg = cur;
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <=
            unwrap(ackno, _isn, _next_seqno)) {
            // i would insert seg in Sequence
        } else {
            store_sz += seg.payload().size();
            tmp_list.push_back(cur);
        }
    }
    _store.swap(tmp_list);
    if (store_sz > _win_sz) {
        // my store segs have not been cusumed
        return;
    }
    if (window_size == 0) {
        // treat window_size = 0 as 1
        _left_win_sz = 1;
        _store.clear();
    } else {
        _left_win_sz = window_size - store_sz;
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _current_time += ms_since_last_tick;
    if (_timer.check_if_expired(_current_time)) {
        if (!_store.empty()) {
            auto seg = _store.front();
            _segments_out.push(seg);
            _timer.retrans(_current_time);
            if (_win_sz != 0) {
                _consecutive_retransmissions_cnt += 1;
                _timer.double_rto();
            }
        }
    }

    fill_window();
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_cnt; }

void TCPSender::send_empty_segment() {
    TCPHeader header;
    Buffer payload;
    header.seqno = _last_ackno;
    TCPSegment seg;
    seg.header() = header;
    seg.payload() = payload;
    _segments_out.push(seg);
}
