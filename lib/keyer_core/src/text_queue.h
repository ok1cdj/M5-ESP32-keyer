// Fixed-size ring buffer of normalised Morse queue items (see morse.h).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace keyer {

class TextQueue {
public:
    static constexpr size_t CAPACITY = 1024;

    bool push(uint8_t item) {
        if (count_ == CAPACITY) return false;
        buf_[(head_ + count_) % CAPACITY] = item;
        count_++;
        return true;
    }

    bool pop(uint8_t& item) {
        if (count_ == 0) return false;
        item = buf_[head_];
        head_ = (head_ + 1) % CAPACITY;
        count_--;
        return true;
    }

    void clear() {
        head_ = 0;
        count_ = 0;
    }

    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }

private:
    uint8_t buf_[CAPACITY];
    size_t head_ = 0;
    size_t count_ = 0;
};

}  // namespace keyer
