#include "stream_reassembler.hh"

#include <cstddef>
#include <iostream>
#include <ostream>
#include <vector>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _stored(), _expect_index(0), _eof_index(-1) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    /* DUMMY_CODE(data, index, eof); */
    if (eof) {
        _eof_index = index + data.size();
        // we need to save eof index
    }
    std::vector<size_t> need_merge_range{index, index + data.size()};
    std::string need_merge_string(data);
    decltype(_stored) tmp;
    for (auto iter = _stored.begin(); iter != _stored.end(); ++iter) {
        auto cur_start = iter->first;
        auto cur_end = iter->second.size() + iter->first;
        if (need_merge_range.empty() || cur_end < need_merge_range[0]) {
            tmp.push_back(std::move(*iter));
        } else if (cur_start > need_merge_range[1]) {
            tmp.push_back(std::pair{need_merge_range[0], std::move(need_merge_string)});
            tmp.push_back(std::move(*iter));
            need_merge_range.resize(0);
        } else {
            auto len1 = need_merge_range[0] > cur_start ? need_merge_range[0] - cur_start : static_cast<size_t>(0);
            auto len2 = need_merge_range[1] < cur_end ? cur_end - need_merge_range[1] : static_cast<size_t>(0);
            need_merge_string = iter->second.substr(0, len1) + std::move(need_merge_string);
            need_merge_string += iter->second.substr(iter->second.size() - len2, len2);

            need_merge_range[0] = std::min(need_merge_range[0], cur_start);
            need_merge_range[1] = std::max(need_merge_range[1], cur_end);
        }
    }
    if (!need_merge_range.empty()) {
        tmp.push_back({need_merge_range[0], std::move(need_merge_string)});
    }
    _stored = std::move(tmp);
    // append
    while (!_stored.empty()) {
        auto store_front = _stored.front();
        auto store_front_index = store_front.first;
        auto store_front_str = store_front.second;

        if (store_front_index <= _expect_index) {
            auto pos = std::min(_expect_index - store_front_index, store_front_str.size());
            auto append_str = store_front_str.substr(pos, store_front_str.size() - pos);
            auto write_size = _output.write(append_str);
            _expect_index += write_size;
            _stored.pop_front();
        } else {
            break;
        }
    }

    if (_expect_index == _eof_index) {
        // check if end input
        _output.end_input();
    }

    // shrink
    size_t left_store_size = _capacity - _output.buffer_size();
    tmp.clear();
    for (auto &cur : _stored) {
        if (left_store_size <= 0) {
            break;
        }
        auto cur_index = cur.first;
        auto &cur_str = cur.second;
        auto append_size = left_store_size > cur_str.size() ? cur_str.size() : left_store_size;
        left_store_size -= append_size;
        auto append_str = cur_str.substr(0, append_size);
        tmp.push_back({cur_index, append_str});
    }
    _stored = std::move(tmp);
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t res = 0;
    for (auto &cur : _stored) {
        res += cur.second.size();
    }
    return res;
}

bool StreamReassembler::empty() const { return _stored.empty(); }
