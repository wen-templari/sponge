#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <bits/stdint-uintn.h>
#include <ios>
#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

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
    cout << "filling  window: " << _left_window_size << "\n";
    if ((_reachSyn && stream_in().buffer_empty() && !stream_in().eof()) || _reachFin || _left_window_size == 0 || bytes_in_flight() > _window_size) {
        return;
    }
    TCPSegment seg;
    // if in waiting for stream to begin (no SYN sent)
    if (!_reachSyn) {
        seg.header().syn = true;
        _reachSyn = true;
    }
    bool exceedsWindowSize = false;
    if (_left_window_size > TCPConfig::MAX_PAYLOAD_SIZE) {
        exceedsWindowSize = true;
    }
    seg.payload() = stream_in().read(_left_window_size > TCPConfig::MAX_PAYLOAD_SIZE ? TCPConfig::MAX_PAYLOAD_SIZE
                                                                                     : _left_window_size);
    _left_window_size -= seg.length_in_sequence_space();
    cout << "payload read " << seg.payload().size() << "\n";
    // if reach eof and have window space for fin
    cout << "reach EOF: " << stream_in().eof() << " | left_window_size: " << _left_window_size << "\n";
    if (stream_in().eof() && _left_window_size > 0) {
        cout << "set fin \n";
        seg.header().fin = true;
        _left_window_size--;
        _reachFin = true;
    }

    if (seg.length_in_sequence_space() == 0) {
        return;
    }

    _bytes_in_flight += seg.length_in_sequence_space();
    // _window_size -= seg.length_in_sequence_space();
    seg.header().seqno = next_seqno();
    _outstanding_segments.push_back(seg);
    _segments_out.push(seg);
    _next_seqno += seg.length_in_sequence_space();

    cout << "header: " << seg.header().to_string();
    cout << "header.seqno: " << unwrap(seg.header().seqno, _isn, _next_seqno) << "\n";
    cout << "ISN " << _isn << "\n";
    cout << "bytes_in_flight: " << _bytes_in_flight << "\n";
    cout << "next seqno " << _next_seqno << "\n\n";
    if (exceedsWindowSize) {
        this->fill_window();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto unwrapped_ackno = unwrap(ackno, _isn, _next_seqno);

    cout << "\nack received: " << unwrapped_ackno << " | window_size: " << window_size << "\n";

    auto abs_seg_tailno = [this](TCPSegment seg) {
        auto abs_seqno = unwrap(seg.header().seqno, _isn, _next_seqno);
        return abs_seqno + seg.length_in_sequence_space();
    };

    // ignore impossible ackno
    if (unwrapped_ackno > _next_seqno) {
        return;
    }

    _initial_window_zero = window_size == 0;
    _window_size = _initial_window_zero ? 1 : window_size;
    _left_window_size = _window_size;

    // If the receiver has announced a window size of zero,
    // the fill window method should act like the window size is one.
    _rto = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;

    for (auto iter = _outstanding_segments.begin(); iter < _outstanding_segments.end(); ++iter) {
        auto abs_seqno = unwrap(iter->header().seqno, _isn, _next_seqno);
        cout << "abs_seqno: " << abs_seqno << " | sequence_space_size: " << iter->length_in_sequence_space() << "\n";
        if (abs_seg_tailno(*iter) < unwrapped_ackno + 1) {
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
    // timer expired
    cout << "timer expired " << _last_tick << " >= " << _rto << "\n";
    cout << "window_size " << _window_size << "\n";
    cout << "initial_zero " << _initial_window_zero << "\n";

    // a) retransmit earliest segment
    _segments_out.push(_outstanding_segments.front());
    cout << "resend: " << _segments_out.back().header().to_string() << "\n\n";

    // b) if window size is nonzero
    //    i.  incr consecutive retransmissions
    //    ii. double rto
    if (_window_size != 0 && !_initial_window_zero) {
        _consecutive_retransmissions++;
        _rto *= 2;
    }

    // c) reset timer
    _last_tick = 0;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
