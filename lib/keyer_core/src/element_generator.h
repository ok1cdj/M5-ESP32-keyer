// Element generator: turns queue items into dits and dahs with shared WPM
// timing. Text is the only source today; paddles will feed it in the next
// phase without touching the timing.
#pragma once

#include <stdint.h>

#include "morse.h"

namespace keyer {

constexpr uint8_t WPM_MIN = 5;
constexpr uint8_t WPM_MAX = 50;
constexpr uint8_t WPM_DEFAULT = 20;

class ElementGenerator {
public:
    void setWpm(uint8_t wpm) { wpm_ = wpm < WPM_MIN ? WPM_MIN : (wpm > WPM_MAX ? WPM_MAX : wpm); }
    uint8_t wpm() const { return wpm_; }

    // PARIS timing: one dit lasts 1200 / WPM ms.
    uint32_t ditUs() const { return 1200000u / wpm_; }
    uint32_t dahUs() const { return 3 * ditUs(); }

    // Loads a queue item (character, optionally with JOIN_FLAG).
    // Returns false for an item that has no Morse pattern.
    bool load(uint8_t item) {
        pattern_ = morsePattern(char(item & CHAR_MASK));
        joined_ = (item & JOIN_FLAG) != 0;
        return pattern_ != nullptr;
    }

    bool hasElement() const { return pattern_ && *pattern_; }

    // Duration of the next element of the loaded character; advances.
    uint32_t nextElementUs() {
        char e = *pattern_++;
        return e == '-' ? dahUs() : ditUs();
    }

    // True when the loaded character is part of a prosign and the next one
    // follows after an element gap only.
    bool joined() const { return joined_; }

    void clear() {
        pattern_ = nullptr;
        joined_ = false;
    }

private:
    uint8_t wpm_ = WPM_DEFAULT;
    const char* pattern_ = nullptr;
    bool joined_ = false;
};

}  // namespace keyer
