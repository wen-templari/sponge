#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : size(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t l = data.length() > remaining_capacity() ? remaining_capacity() : data.length();
    bytesWritten += l;
    for (size_t i = 0; i < l; i++) {
        buffer.push_back(data[i]);
    }
    return l;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    int l = len > buffer.size() ? buffer.size() : len;
    return string().assign(buffer.begin(), buffer.begin() + l);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    int l = len > buffer.size() ? buffer.size() : len;
    bytesRead += l;
    for (int i = l; i > 0; i--) {
        buffer.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { inputEnded = true; }

bool ByteStream::input_ended() const { return inputEnded; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.empty(); }

bool ByteStream::eof() const { return buffer.empty() && inputEnded; }

size_t ByteStream::bytes_written() const { return bytesWritten; }

size_t ByteStream::bytes_read() const { return bytesRead; }

size_t ByteStream::remaining_capacity() const { return size - buffer_size(); }
