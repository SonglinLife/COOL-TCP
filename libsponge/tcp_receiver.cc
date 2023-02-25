#include "tcp_receiver.hh"

#include "tcp_header.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <iostream>
#include <limits>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    Buffer payload = seg.payload();

    if (header.syn) {
        // receive a syn segment
        _sender_isn = seg.header().seqno;
        _next_abs_seqno = 1;
        _has_sender_isn = true;
        // why not return if the header.syn, in my understanding, the syn seg may
        // also convery the fin bit, so it is better not return to trans the fin
        // message to _reassembler
    } else if (!_has_sender_isn) {
        return;  // no need write
    }
    auto seg_abs_seqno = unwrap(seg.header().seqno, _sender_isn, _next_abs_seqno);
    size_t index = header.syn ? seg_abs_seqno : seg_abs_seqno - 1;

    _reassembler.push_substring(payload.copy(), index, header.fin);
    auto &out_bytes = _reassembler.stream_out();
    auto received_sz = out_bytes.bytes_written();
    _next_abs_seqno = received_sz + 1;
    if (_reassembler.eof()) {
        _next_abs_seqno++;
        _eof = true;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_has_sender_isn) {
        return {};
    }
    auto ackno = wrap(_next_abs_seqno, _sender_isn);
    return ackno;
}

size_t TCPReceiver::window_size() const {
    auto &out = _reassembler.stream_out();
    auto sz = _capacity - (out.bytes_written() - out.bytes_read());
    return sz;
}
