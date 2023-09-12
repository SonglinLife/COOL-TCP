#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

void ByteStream::read_buffer_safe(size_t s) const {
    if (s > buffer_size()) {
        throw std::range_error("Index out of range");
    }
}

ByteStream::ByteStream(const size_t capacity)
    : u_buffer_capacity(capacity), u_buffer(make_unique<u_bytes_sequence_type>()) {}

size_t ByteStream::write(const string &data) {
    size_t remaining = remaining_capacity();
    size_t bytes_written = 0;
    for (auto &c : data) {
        if (bytes_written >= remaining) {
            break;
        }
        u_buffer->push_back(c);
        ++bytes_written;
    }
    total_bytes_written_count += bytes_written;
    return bytes_written;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    read_buffer_safe(len);
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << u_buffer->at(i);
    }
    return oss.str();
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    read_buffer_safe(len);
    size_t i = len;
    size_t bytes_read = 0;
    while (i--) {
        u_buffer->pop_front();
        ++bytes_read;
    }
    total_bytes_read_count += bytes_read;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    read_buffer_safe(len);
    std::ostringstream oss;
    size_t i = len;
    size_t bytes_read = 0;
    while (i--) {
        oss << u_buffer->front();
        u_buffer->pop_front();
        ++bytes_read;
    }
    total_bytes_read_count += bytes_read;
    return oss.str();
}

void ByteStream::end_input() { input_ended_count = true; }

bool ByteStream::input_ended() const { return input_ended_count; }

size_t ByteStream::buffer_size() const { return u_buffer->size(); }

bool ByteStream::buffer_empty() const { return !buffer_size(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return total_bytes_written_count; }

size_t ByteStream::bytes_read() const { return total_bytes_read_count; }

size_t ByteStream::remaining_capacity() const { return u_buffer_capacity - buffer_size(); }
