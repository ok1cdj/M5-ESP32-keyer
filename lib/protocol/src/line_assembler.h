// Assembles protocol lines from arbitrarily fragmented writes (a BLE write
// without negotiated MTU carries only 20 bytes). Pure C++.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace proto {

class LineAssembler {
public:
    static constexpr size_t MAX_LINE = 256;

    // Called with a complete line (without "\r\n"), or with line == nullptr
    // when a line longer than MAX_LINE was dropped (answer "ERR length").
    typedef void (*LineFn)(void* ctx, const char* line, size_t len);

    LineAssembler(LineFn fn, void* ctx) : fn_(fn), ctx_(ctx) {}

    void feed(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            char c = char(data[i]);
            if (c == '\n') {
                if (!overflow_ && len_ > 0 && buf_[len_ - 1] == '\r') len_--;
                if (overflow_ || len_ > MAX_LINE) {
                    fn_(ctx_, nullptr, 0);
                } else {
                    buf_[len_] = '\0';
                    fn_(ctx_, buf_, len_);
                }
                len_ = 0;
                overflow_ = false;
            } else if (overflow_) {
                // discard until the end of the line
            } else if (len_ < MAX_LINE + 1) {  // +1 keeps room for a trailing '\r'
                buf_[len_++] = c;
            } else {
                overflow_ = true;
            }
        }
    }

    void reset() {
        len_ = 0;
        overflow_ = false;
    }

private:
    LineFn fn_;
    void* ctx_;
    char buf_[MAX_LINE + 2];
    size_t len_ = 0;
    bool overflow_ = false;
};

}  // namespace proto
