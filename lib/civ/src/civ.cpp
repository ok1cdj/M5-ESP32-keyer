#include "civ.h"

#include <stdio.h>

namespace civ {

namespace {

// Rates tried by the detection, most common first.
constexpr uint32_t BAUDS[] = {19200, 9600, 4800, 1200};
constexpr size_t BAUD_COUNT = sizeof(BAUDS) / sizeof(BAUDS[0]);

// Default addresses probed when the rig has Transceive off:
// IC-706MkIIG, IC-706MkII, IC-706, IC-910H, IC-7300.
constexpr uint8_t PROBE_ADDRS[] = {0x58, 0x4E, 0x48, 0x60, 0x94};

}  // namespace

// ---- FrameParser -----------------------------------------------------------

bool FrameParser::feed(uint8_t b) {
    // A preamble byte anywhere starts a new frame; the BCD and mode data
    // never contain FE.
    if (b == PREAMBLE) {
        state_ = (state_ == State::Pre1 || state_ == State::To) ? State::To : State::Pre1;
        return false;
    }
    if (b == JAM) {
        state_ = State::Idle;
        return false;
    }
    switch (state_) {
        case State::Idle:
        case State::Pre1:
            state_ = State::Idle;
            return false;
        case State::To:
            if (b == END) break;
            frame_.to = b;
            state_ = State::From;
            return false;
        case State::From:
            if (b == END) break;
            frame_.from = b;
            state_ = State::Cmd;
            return false;
        case State::Cmd:
            if (b == END) break;
            frame_.cmd = b;
            frame_.len = 0;
            state_ = State::Data;
            return false;
        case State::Data:
            if (b == END) {
                state_ = State::Idle;
                return true;
            }
            if (frame_.len >= Frame::MAX_DATA) break;
            frame_.data[frame_.len++] = b;
            return false;
    }
    state_ = State::Idle;
    return false;
}

// ---- Helpers ---------------------------------------------------------------

size_t encodeFrame(const Frame& f, uint8_t* out) {
    size_t n = 0;
    out[n++] = PREAMBLE;
    out[n++] = PREAMBLE;
    out[n++] = f.to;
    out[n++] = f.from;
    out[n++] = f.cmd;
    for (size_t i = 0; i < f.len; i++) out[n++] = f.data[i];
    out[n++] = END;
    return n;
}

bool addressFrame(uint8_t* b, size_t n, uint8_t rig) {
    if (n < 6 || n > Frame::MAX_BYTES) return false;
    if (b[0] != PREAMBLE || b[1] != PREAMBLE || b[n - 1] != END) return false;
    // Inside, these bytes would end or restart the frame on the bus.
    for (size_t i = 2; i < n - 1; i++) {
        if (b[i] == PREAMBLE || b[i] == END || b[i] == JAM) return false;
    }
    b[2] = rig;
    b[3] = CONTROLLER;
    return true;
}

std::string toHex(const uint8_t* b, size_t n) {
    static const char DIGITS[] = "0123456789ABCDEF";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) {
        s += DIGITS[b[i] >> 4];
        s += DIGITS[b[i] & 0x0F];
    }
    return s;
}

size_t fromHex(const std::string& hex, uint8_t* out, size_t max) {
    if (hex.size() % 2 != 0 || hex.size() / 2 > max) return 0;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = nibble(hex[i]), lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return 0;
        out[i / 2] = uint8_t(hi << 4 | lo);
    }
    return hex.size() / 2;
}

bool decodeFreq(const uint8_t* d, size_t n, uint64_t& hz) {
    if (n != 4 && n != 5) return false;
    uint64_t v = 0;
    for (size_t i = n; i-- > 0;) {
        uint8_t hi = d[i] >> 4, lo = d[i] & 0x0F;
        if (hi > 9 || lo > 9) return false;
        v = v * 100 + hi * 10 + lo;
    }
    hz = v;
    return true;
}

const char* modeName(uint8_t mode) {
    switch (mode) {
        case 0x00: return "LSB";
        case 0x01: return "USB";
        case 0x02: return "AM";
        case 0x03: return "CW";
        case 0x04: return "RTTY";
        case 0x05: return "FM";
        case 0x06: return "WFM";
        case 0x07: return "CW-R";
        case 0x08: return "RTTY-R";
    }
    return "?";
}

size_t buildQuery(uint8_t rig, uint8_t ctrl, uint8_t cmd, uint8_t* out) {
    out[0] = PREAMBLE;
    out[1] = PREAMBLE;
    out[2] = rig;
    out[3] = ctrl;
    out[4] = cmd;
    out[5] = END;
    return 6;
}

std::string formatFreq(bool valid, uint64_t hz, uint8_t mode) {
    if (!valid) return "FREQ NONE";
    return "FREQ " + std::to_string(hz) + " " + modeName(mode);
}

std::string formatDisplay(bool valid, uint64_t hz, uint8_t mode) {
    if (!valid) return "";
    char buf[32];
    unsigned mhz = unsigned(hz / 1000000);
    unsigned khz = unsigned(hz / 1000 % 1000);
    unsigned dec = unsigned(hz % 1000 / 10);
    snprintf(buf, sizeof buf, "%u %03u.%02u %s", mhz, khz, dec, modeName(mode));
    return buf;
}

bool validBaud(uint32_t baud) {
    for (uint32_t b : BAUDS) {
        if (b == baud) return true;
    }
    return false;
}

std::string formatLink(const Config& cfg, const State& st) {
    if (cfg.mode == LinkMode::Off) return "CIV OFF";
    std::string s = cfg.mode == LinkMode::Auto ? "CIV AUTO " : "CIV FIXED ";
    if (!st.locked) return s + "- -";
    char rig[4];
    snprintf(rig, sizeof rig, "%02X", st.rig);
    return s + std::to_string(st.baud) + " " + rig;
}

// ---- Link ------------------------------------------------------------------

void Link::setHint(uint8_t rig, uint32_t baud) {
    hintRig_ = rig;
    hintBaud_ = baud;
}

uint8_t Link::probeAddr(size_t i) const {
    if (hintRig_ != 0) {
        if (i == 0) return hintRig_;
        i--;
    }
    for (uint8_t a : PROBE_ADDRS) {
        if (a == hintRig_) continue;
        if (i-- == 0) return a;
    }
    return 0;
}

void Link::configure(const Config& cfg, uint32_t now) {
    cfg_ = cfg;
    // Search from the rate the rig was last found at.
    baudIndex_ = 0;
    for (size_t i = 0; i < BAUD_COUNT; i++) {
        if (BAUDS[i] == hintBaud_) baudIndex_ = uint8_t(i);
    }
    unlock(now);
    if (cfg_.mode == LinkMode::Fixed) {
        state_.baud = cfg_.baud;
        lock(cfg_.rig, now);
    } else if (cfg_.mode == LinkMode::Off) {
        state_.baud = 0;
    }
}

void Link::unlock(uint32_t now) {
    state_.locked = false;
    state_.rig = 0;
    update(false, 0, MODE_UNKNOWN);
    state_.baud = BAUDS[baudIndex_];
    searchStart_ = now;
    probeIndex_ = 0;
    pendingLen_ = 0;
    heardBroadcast_ = false;
}

void Link::lock(uint8_t rig, uint32_t now) {
    state_.locked = true;
    state_.rig = rig;
    lastRx_ = now;
    lastPoll_ = now;
    // Read the current state at once, Transceive only reports changes.
    pending_[0] = CMD_READ_FREQ;
    pending_[1] = CMD_READ_MODE;
    pendingLen_ = 2;
}

// The frequency is reported once the mode is known too, so a client never
// sees "?" for a mode that is just about to arrive.
void Link::update(bool hzKnown, uint64_t hz, uint8_t mode) {
    hzKnown_ = hzKnown;
    hz_ = hz;
    mode_ = mode;
    bool valid = hzKnown && mode != MODE_UNKNOWN;
    uint64_t shownHz = valid ? hz : 0;
    uint8_t shownMode = valid ? mode : MODE_UNKNOWN;
    if (valid == state_.valid && shownHz == state_.hz && shownMode == state_.mode) return;
    state_.valid = valid;
    state_.hz = shownHz;
    state_.mode = shownMode;
    state_.seq++;
}

void Link::onFrame(const Frame& f, uint32_t now) {
    if (cfg_.mode == LinkMode::Off) return;
    if (f.from == CONTROLLER) return;  // our own echo or another PC

    if (!state_.locked) {
        // Only the rig talks to the broadcast or to us.
        if (f.to != BROADCAST && f.to != CONTROLLER) return;
        lock(f.from, now);
    }
    if (f.from != state_.rig) return;

    lastRx_ = now;
    bool trx = f.cmd == CMD_TRX_FREQ || f.cmd == CMD_TRX_MODE;
    if (trx && f.to == BROADCAST) {
        lastBroadcast_ = now;
        heardBroadcast_ = true;
    }
    uint64_t hz;
    if ((f.cmd == CMD_TRX_FREQ || f.cmd == CMD_READ_FREQ) && decodeFreq(f.data, f.len, hz)) {
        update(true, hz, mode_);
    } else if ((f.cmd == CMD_TRX_MODE || f.cmd == CMD_READ_MODE) && f.len >= 1) {
        update(hzKnown_, hz_, f.data[0]);
    }
}

Link::Action Link::poll(uint32_t now) {
    Action a;
    if (cfg_.mode == LinkMode::Off) return a;

    if (!state_.locked) {
        if (due(searchStart_, SEARCH_DWELL_MS, now)) {
            baudIndex_ = uint8_t((baudIndex_ + 1) % BAUD_COUNT);
            unlock(now);
        }
        a.baud = state_.baud;
        // Probe the known addresses in case Transceive is off.
        uint8_t rig = probeAddr(probeIndex_);
        if (rig != 0 && due(searchStart_, PROBE_GAP_MS * (probeIndex_ + 1), now)) {
            probeIndex_++;
            a.send = true;
            a.rig = rig;
            a.cmd = CMD_READ_FREQ;
            lastSend_ = now;
        }
        return a;
    }

    a.baud = state_.baud;
    if (due(lastRx_, LOST_MS, now)) {
        if (cfg_.mode == LinkMode::Auto) {
            unlock(now);
            a.baud = state_.baud;
            return a;
        }
        // Fixed: keep asking, but report the rig as gone.
        update(false, 0, MODE_UNKNOWN);
        lastRx_ = now;
    }
    // Poll when the rig does not report changes by itself, and until both
    // the frequency and the mode are known.
    bool quiet = !heardBroadcast_ || due(lastBroadcast_, QUIET_MS, now);
    bool incomplete = !state_.valid;
    if (pendingLen_ == 0 && (quiet || incomplete) && due(lastPoll_, POLL_MS, now)) {
        pending_[0] = CMD_READ_FREQ;
        pending_[1] = CMD_READ_MODE;
        pendingLen_ = 2;
        lastPoll_ = now;
    }
    if (pendingLen_ > 0 && due(lastSend_, SEND_GAP_MS, now)) {
        a.send = true;
        a.rig = state_.rig;
        a.cmd = pending_[0];
        pending_[0] = pending_[1];
        pendingLen_--;
        lastSend_ = now;
    }
    return a;
}

}  // namespace civ
