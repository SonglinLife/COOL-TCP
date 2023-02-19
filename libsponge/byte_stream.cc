#include "byte_stream.hh"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : capacity_(capacity), buf_(){}

size_t ByteStream::write(const string &data) {
    if (end_input_flag_ | _error) {
        return 0;
    }
    size_t writen_size = std::min(data.size(), capacity_ - buf_.size());
    std::for_each_n(data.begin(), writen_size, [this](const char &ch) { buf_.push_back(ch); });
    total_bytes_written_nums_ += writen_size;
    return writen_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_size = std::min(buf_.size(), len);
    return std::string{buf_.begin(), buf_.begin() + peek_size};
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_size = std::min(buf_.size(), len);
    std::vector<char> tmp{buf_.begin() + pop_size, buf_.end()};
    buf_.swap(tmp);
    total_bytes_read_nums_ += pop_size;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len  0bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t read_size = std::min(buf_.size(), len);
    std::string read_str{buf_.begin(), buf_.begin() + read_size};
    pop_output(read_size);
    return read_str;
}

void ByteStream::end_input() { end_input_flag_ = true; }

bool ByteStream::input_ended() const { return end_input_flag_; }

size_t ByteStream::buffer_size() const { return buf_.size(); }

bool ByteStream::buffer_empty() const { return (buf_.size() == 0); }

bool ByteStream::eof() const { return end_input_flag_ && (buf_.size() == 0); }

size_t ByteStream::bytes_written() const { return total_bytes_written_nums_; }

size_t ByteStream::bytes_read() const { return total_bytes_read_nums_; }

size_t ByteStream::remaining_capacity() const { return capacity_ - buf_.size(); }

