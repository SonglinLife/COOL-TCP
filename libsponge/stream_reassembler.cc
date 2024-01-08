#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _current_offset(0), _eof_segment_offset(SIZE_MAX), _wait_queue() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index + data.length() > _current_offset + _capacity) {
        _wait_queue.insert({index, string(data.begin(), data.begin() + _current_offset + _capacity - index)});
    } else {
        _wait_queue.insert({index, data});
    }
    if (eof) _eof_segment_offset = index + data.length();
    while (!_wait_queue.empty() && _wait_queue.begin()->first <= _current_offset) {
        auto [idx, dat] = *_wait_queue.begin();
        _wait_queue.erase(_wait_queue.begin());
        auto incr = _current_offset - idx;
        if (incr < dat.length()) {  // if `dat` is not a subset of assembled bytes
            auto dat_aligned = string(dat.begin() + incr, dat.end());
            auto bytes_written = _output.write(dat_aligned);
            _current_offset += bytes_written;
            if (bytes_written != dat_aligned.length()) {
                _wait_queue.insert({_current_offset, string(dat_aligned.begin() + bytes_written, dat_aligned.end())});
                break;
            }
        }
        if (_eof_segment_offset != SIZE_MAX && _current_offset == _eof_segment_offset)
            _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t last = 0;
    size_t res = 0;
    for (auto& [idx, dat] : _wait_queue) {
        if (last < idx) {
            res += dat.length();
            last = idx + dat.length();
        } else {
            if (last < idx + dat.length()) {
                res += idx + dat.length() - last;
                last = idx + dat.length();
            }
        }
    }
    return res;
}

bool StreamReassembler::empty() const { return _current_offset == 0; }
