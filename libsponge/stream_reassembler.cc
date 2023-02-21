#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    lastApplied = -1;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        reachEOF = true;
    }
    int applyStart = -1;
    int targetLastApply = lastApplied;
    auto findMax = [](std::map<int, char> map) {
        auto iter = map.begin();
        int maxIndex = -1;
        while (iter != map.end()) {
            if (iter->first > maxIndex) {
                maxIndex = iter->first;
            }
            iter++;
        }
        return maxIndex;
    };
    for (size_t i = 0; i < data.length(); i++) {
        int streamIndex = i + index;
        if (streamIndex <= lastApplied) {
            continue;
        }
        unassaembled.erase(streamIndex);
        if (unassembled_bytes() + _output.buffer_size() + targetLastApply - lastApplied == _capacity) {
            int maxIndex = findMax(unassaembled);
            if (maxIndex > streamIndex) {
                unassaembled.erase(maxIndex);
                reachEOF = false;
            } else {
                break;
            }
        }
        if (streamIndex == targetLastApply + 1) {
            if (applyStart == -1) {
                applyStart = i;
            }
            targetLastApply++;
        } else {
            unassaembled.insert(pair(streamIndex, data[i]));
        }
    }
    string stringFromUnassembled;
    for (size_t j = targetLastApply + 1; j <= _capacity; j++) {
        auto iter = unassaembled.find(j);
        if (iter == unassaembled.end()) {
            break;
        }
        stringFromUnassembled.push_back(iter->second);
        unassaembled.erase(iter);
        targetLastApply++;
    }
    if (targetLastApply > lastApplied) {
        string stringToApply = data.substr(applyStart, targetLastApply - lastApplied) + stringFromUnassembled;
        _output.write(stringToApply);
        lastApplied = targetLastApply;
    }
    if (unassembled_bytes() == 0 && reachEOF) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return unassaembled.size(); }

bool StreamReassembler::empty() const { return _output.buffer_empty() && unassaembled.empty(); }
