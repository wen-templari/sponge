#include "tcp_receiver.hh"

#include "byte_stream.hh"
#include "parser.hh"
#include "wrapping_integers.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    cout << seg.header().to_string() << "\n";
    if (seg.header().syn) {
        _ISN.emplace(seg.header().seqno);
    }
    if (!ackno().has_value() || stream_out().eof()) {
        return;
    }
    cout << "ISN " << _ISN.value() << "\n";
    if (seg.payload().size()) {
        int i = (seg.header().syn) ? 1 : 0;
        uint64_t absSeqno = unwrap(seg.header().seqno + i, _ISN.value(), stream_out().bytes_written());
        _reassembler.push_substring(seg.payload().copy(), absSeqno - 1, seg.header().fin);

        cerr << "push_substring " << seg.payload().copy() << "\n";
        cerr << "size " << seg.payload().size() << "\n";
        cerr << "at " << absSeqno - 1 << "\n";
        cerr << "bytes_written " << _reassembler.stream_out().bytes_written() << "\n";
    }
    if (seg.header().fin && _reassembler.empty()) {
        _reassembler.stream_out().end_input();
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_ISN.has_value()) {
        return std::nullopt;
    }
    int i = (_reassembler.stream_out().input_ended()) ? 2 : 1;
    return _ISN.value() + stream_out().bytes_written() + i;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
