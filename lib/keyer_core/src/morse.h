// Morse table and text normalisation. Pure C++, no Arduino dependency.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace keyer {

// Queue item encoding: plain ASCII of a supported character or ' '.
// JOIN_FLAG marks a prosign letter that is followed by an element gap only
// (no character gap), e.g. <AR> is queued as 'A'|JOIN_FLAG, 'R'.
constexpr uint8_t JOIN_FLAG = 0x80;
constexpr uint8_t CHAR_MASK = 0x7F;

// Returns the pattern ('.' and '-') for an upper-case supported character,
// or nullptr when the character has no Morse representation.
const char* morsePattern(char c);

// Receives normalised queue items one by one. Returns false when the sink is
// full; normalisation then stops.
typedef bool (*ItemSink)(void* ctx, uint8_t item);

struct NormalizeResult {
    bool badChar;   // at least one unsupported character was skipped
    bool overflow;  // the sink refused an item
};

// Upper-cases the text, expands <XY..> prosigns into joined letters, maps
// tabs and line breaks to spaces and skips unsupported characters.
NormalizeResult normalize(const char* text, size_t len, ItemSink sink, void* ctx);

}  // namespace keyer
