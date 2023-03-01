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

    if (header.syn && !_next_abs_seqno) {
        // receive a syn segment
        _sender_isn = seg.header().seqno;
        _next_abs_seqno = 1;
        // why not return if the header.syn, in my understanding, the syn seg may
        // also convery the fin bit, so it is better not return to trans the fin
        // message to _reassembler
        //
        // update 23.3.1 when I finish the lab3 sender, I realize it is impossble that
        // sender only has 1 win_sz at beginning, which means the sender could only
        // send syn but no syn combine fin
    } else if (!_next_abs_seqno) {
        return;  // no need write
    }
    auto seg_abs_seqno = unwrap(header.seqno, _sender_isn, _next_abs_seqno);
    auto payload_abs_seqno = seg_abs_seqno + header.syn;
    if (payload_abs_seqno + payload.size() + header.fin <= _next_abs_seqno) {
        // an out time seg, return it!
        return;
    }
    auto unacceptable_abs_seqno = stream_out().bytes_written() + _capacity;
    if (payload_abs_seqno > unacceptable_abs_seqno) {
      // make sure the unacceptable_abs_seqno - unassembler_abs_seqno <= _capacity; 
      // I think it was unnecessrary, but the lab doc froce me to do that.
        return;
    }

    size_t index = header.syn ? seg_abs_seqno : seg_abs_seqno - 1;

    _reassembler.push_substring(payload.copy(), index, header.fin);
    auto &out_bytes = _reassembler.stream_out();
    auto received_sz = out_bytes.bytes_written();
    _next_abs_seqno = received_sz + 1;
    if (stream_out().input_ended()) {
      // the fin bit also occupy a seqno
        _next_abs_seqno++;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_next_abs_seqno) {
      // no syn seg, in state listen, waiting for SYN, ackno is empty
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
