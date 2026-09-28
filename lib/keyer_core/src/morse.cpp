#include "morse.h"

namespace keyer {

namespace {

struct Entry {
    char c;
    const char* pattern;
};

const Entry TABLE[] = {
    {'A', ".-"},     {'B', "-..."},   {'C', "-.-."},   {'D', "-.."},
    {'E', "."},      {'F', "..-."},   {'G', "--."},    {'H', "...."},
    {'I', ".."},     {'J', ".---"},   {'K', "-.-"},    {'L', ".-.."},
    {'M', "--"},     {'N', "-."},     {'O', "---"},    {'P', ".--."},
    {'Q', "--.-"},   {'R', ".-."},    {'S', "..."},    {'T', "-"},
    {'U', "..-"},    {'V', "...-"},   {'W', ".--"},    {'X', "-..-"},
    {'Y', "-.--"},   {'Z', "--.."},
    {'0', "-----"},  {'1', ".----"},  {'2', "..---"},  {'3', "...--"},
    {'4', "....-"},  {'5', "....."},  {'6', "-...."},  {'7', "--..."},
    {'8', "---.."},  {'9', "----."},
    {'/', "-..-."},  {'?', "..--.."}, {'.', ".-.-.-"}, {',', "--..--"},
    {'=', "-...-"},  {'+', ".-.-."},  {'-', "-....-"},
};

char upper(char c) { return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c; }

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

}  // namespace

const char* morsePattern(char c) {
    for (const Entry& e : TABLE) {
        if (e.c == c) return e.pattern;
    }
    return nullptr;
}

NormalizeResult normalize(const char* text, size_t len, ItemSink sink, void* ctx) {
    NormalizeResult r{false, false};
    size_t i = 0;
    while (i < len) {
        char c = upper(text[i]);
        if (c == '<') {
            // Prosign: find the closing '>' and join all its letters.
            size_t end = i + 1;
            while (end < len && text[end] != '>' && text[end] != '<') end++;
            if (end < len && text[end] == '>') {
                // Collect supported letters first so the last one can be left unjoined.
                uint8_t items[32];
                size_t n = 0;
                for (size_t k = i + 1; k < end; k++) {
                    char p = upper(text[k]);
                    if (morsePattern(p) && n < sizeof(items)) {
                        items[n++] = uint8_t(p);
                    } else {
                        r.badChar = true;
                    }
                }
                for (size_t k = 0; k < n; k++) {
                    uint8_t item = items[k] | (k + 1 < n ? JOIN_FLAG : 0);
                    if (!sink(ctx, item)) {
                        r.overflow = true;
                        return r;
                    }
                }
                i = end + 1;
                continue;
            }
            r.badChar = true;  // unmatched '<'
            i++;
            continue;
        }
        uint8_t item;
        if (isSpace(c)) {
            item = ' ';
        } else if (morsePattern(c)) {
            item = uint8_t(c);
        } else {
            r.badChar = true;
            i++;
            continue;
        }
        if (!sink(ctx, item)) {
            r.overflow = true;
            return r;
        }
        i++;
    }
    return r;
}

}  // namespace keyer
