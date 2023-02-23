#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <bits/stdint-uintn.h>
#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _rto{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    auto send = [this](TCPSegment segment_to_send) {
        _bytes_in_flight += segment_to_send.length_in_sequence_space();
        _window_size -= segment_to_send.length_in_sequence_space();
        segment_to_send.header().seqno = wrap(_next_seqno, _isn);
        _outstanding_segments.push_back(segment_to_send);
        _segments_out.push(segment_to_send);
        _next_seqno += segment_to_send.length_in_sequence_space();
        // The sequence number occupied by the SYN flag is the ISN
        // if(segment_to_send.header().syn){
        //     _next_seqno--;
        // }
        cout << segment_to_send.header().to_string() << "\n";
        cout << "ISN " << _isn << "\n";
        cout << "bytes_in_flight: " << _bytes_in_flight << "\n";
        cout << "next seqno " << _next_seqno << "\n\n";
    };

    if ((_reachSyn && stream_in().buffer_empty() && !stream_in().eof()) || _reachFin) {
        return;
    }
    TCPSegment seg;
    // if in waiting for stream to begin (no SYN sent)
    if (!_reachSyn) {
        seg.header().syn = true;
        _reachSyn = true;
    }
    seg.payload() = stream_in().read(_window_size);
    cout << "payload read " << seg.payload().size() << "\n";
    cout << "stream size " << stream_in().buffer_size() << "\n";
    // if reach eof and have window space for fin
    cout << "reach EOF: " << stream_in().eof()
         << " | left_window_size: " << _window_size - seg.length_in_sequence_space() << "\n";
    if (stream_in().eof() && seg.length_in_sequence_space() < _window_size) {
        cout << "set fin \n";
        seg.header().fin = true;
        _reachFin = true;
    }
    send(seg);
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto unwrapped_ackno = unwrap(ackno, _isn, 0);
    cout << "ack received: " << unwrapped_ackno << " | window_size: " << window_size << "\n";
    // If the receiver has announced a window size of zero,
    // the fill window method should act like the window size is one.
    _rto = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;
    _window_size = window_size != 0 ? window_size : 1;

    for (auto iter = _outstanding_segments.begin(); iter < _outstanding_segments.end(); ++iter) {
        auto abs_seqno = unwrap(iter->header().seqno, _isn, 0);
        cout << "abs_seqno: " << abs_seqno << " | sequence_space_size: " << iter->length_in_sequence_space() << "\n";
        if ((abs_seqno + iter->length_in_sequence_space()) < unwrapped_ackno + 1) {
            // has outstanding data
            _bytes_in_flight -= iter->length_in_sequence_space();
            _outstanding_segments.erase(iter);
            _last_tick = 0;

            cout << "bytes_in_flight " << _bytes_in_flight << "\n\n";
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_outstanding_segments.empty()) {
        return;
    }
    _last_tick += ms_since_last_tick;
    if (_last_tick < _rto) {
        return;
    }
    cout << "timer expired" << _last_tick << "\n";
    // timer expired
    // a) retransmit earliest segment
    _segments_out.push(_outstanding_segments.front());
    // b) if window size is nonzero
    //    i.  incr consecutive retransmissions
    //    ii. double rto
    if (_window_size != 0) {
        _consecutive_retransmissions++;
        _rto *= 2;
    }
    // c) reset timer
    _last_tick = 0;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(0, _isn);
    _segments_out.push(seg);
}
