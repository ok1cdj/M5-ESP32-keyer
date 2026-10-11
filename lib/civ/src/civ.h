// Icom CI-V: frame parsing and the link state machine (rate and address
// detection, polling, loss). Pure C++, the UART lives in src/radio/civ.cpp.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace civ {

constexpr uint8_t PREAMBLE = 0xFE;
constexpr uint8_t END = 0xFD;
constexpr uint8_t JAM = 0xFC;          // collision signal on the bus
constexpr uint8_t BROADCAST = 0x00;    // "to" of the Transceive messages
constexpr uint8_t CONTROLLER = 0xE0;   // our address

constexpr uint8_t CMD_TRX_FREQ = 0x00;  // Transceive: frequency changed
constexpr uint8_t CMD_TRX_MODE = 0x01;  // Transceive: mode changed
constexpr uint8_t CMD_READ_FREQ = 0x03;
constexpr uint8_t CMD_READ_MODE = 0x04;

constexpr uint8_t MODE_UNKNOWN = 0xFF;

struct Frame {
    static constexpr size_t MAX_DATA = 32;
    static constexpr size_t MAX_BYTES = MAX_DATA + 6;  // with FE FE to from cmd ... FD
    uint8_t to = 0;
    uint8_t from = 0;
    uint8_t cmd = 0;
    uint8_t data[MAX_DATA] = {};
    uint8_t len = 0;
};

// Byte-at-a-time parser of FE FE <to> <from> <cmd> <data> FD.
class FrameParser {
public:
    // Returns true when a complete frame is available in frame().
    bool feed(uint8_t b);
    const Frame& frame() const { return frame_; }
    void reset() { state_ = State::Idle; }

private:
    enum class State : uint8_t { Idle, Pre1, To, From, Cmd, Data };
    State state_ = State::Idle;
    Frame frame_;
};

// The frame back on the wire (FE FE ... FD), returns the length.
size_t encodeFrame(const Frame& f, uint8_t* out);

// Tunnel (CIVTX): checks that bytes[0..n) is one CI-V frame and addresses it
// from us to `rig`, whatever addresses the client put in.
bool addressFrame(uint8_t* bytes, size_t n, uint8_t rig);

// Uppercase hex without separators and back (CIVTX / CIVRX lines).
std::string toHex(const uint8_t* bytes, size_t n);
// Returns the byte count, 0 for bad hex or more than `max` bytes.
size_t fromHex(const std::string& hex, uint8_t* out, size_t max);

// Frequency in BCD, least significant byte first (4 or 5 bytes).
bool decodeFreq(const uint8_t* d, size_t n, uint64_t& hz);
const char* modeName(uint8_t mode);

// Writes FE FE rig ctrl cmd FD into out (6 bytes), returns the length.
size_t buildQuery(uint8_t rig, uint8_t ctrl, uint8_t cmd, uint8_t* out);

// "FREQ 14025000 CW" or "FREQ NONE" (PROTOCOL.md).
std::string formatFreq(bool valid, uint64_t hz, uint8_t mode);
// "14 025.00 CW" for the display, empty when not valid.
std::string formatDisplay(bool valid, uint64_t hz, uint8_t mode);

enum class LinkMode : uint8_t { Off = 0, Auto = 1, Fixed = 2 };

struct Config {
    LinkMode mode = LinkMode::Auto;
    uint32_t baud = 19200;  // Fixed only
    uint8_t rig = 0;        // Fixed only
};

bool validBaud(uint32_t baud);

struct State {
    bool locked = false;   // rig address and rate known
    bool valid = false;    // frequency and mode known
    uint64_t hz = 0;
    uint8_t mode = MODE_UNKNOWN;
    uint32_t baud = 0;
    uint8_t rig = 0;
    uint32_t seq = 0;      // incremented when valid, hz or mode change
};

// Answer to CIV: "CIV AUTO 19200 58", "CIV AUTO - -" while searching,
// "CIV FIXED 9600 60", "CIV OFF" (PROTOCOL.md).
std::string formatLink(const Config& cfg, const State& st);

// The link state machine. The driver calls onFrame() for every parsed
// frame and poll() periodically; poll() says which rate to use and which
// query to send. Times are milliseconds from any wrapping clock.
class Link {
public:
    static constexpr uint32_t SEARCH_DWELL_MS = 2000;  // per rate
    static constexpr uint32_t PROBE_GAP_MS = 150;
    static constexpr uint32_t SEND_GAP_MS = 60;
    static constexpr uint32_t QUIET_MS = 5000;  // no Transceive -> poll
    static constexpr uint32_t POLL_MS = 1000;
    static constexpr uint32_t LOST_MS = 10000;

    struct Action {
        uint32_t baud = 0;  // rate the UART must run at, 0 = UART off
        uint8_t rig = 0;    // send a query to this address...
        uint8_t cmd = 0;    // ...with this command, when send is true
        bool send = false;
    };

    // The rig found last time (stored by the driver): searched first, so a
    // rig with a changed address and Transceive quiet is found after a restart.
    // Takes effect at the next configure().
    void setHint(uint8_t rig, uint32_t baud);
    void configure(const Config& cfg, uint32_t now);
    void onFrame(const Frame& f, uint32_t now);
    Action poll(uint32_t now);

    const State& state() const { return state_; }
    const Config& config() const { return cfg_; }

private:
    void lock(uint8_t rig, uint32_t now);
    void unlock(uint32_t now);
    void update(bool hzKnown, uint64_t hz, uint8_t mode);
    uint8_t probeAddr(size_t i) const;  // 0 past the end
    bool due(uint32_t since, uint32_t gap, uint32_t now) const {
        return uint32_t(now - since) >= gap;
    }

    Config cfg_;
    State state_;
    bool hzKnown_ = false;
    uint64_t hz_ = 0;
    uint8_t mode_ = MODE_UNKNOWN;
    uint8_t hintRig_ = 0;
    uint32_t hintBaud_ = 0;
    uint8_t baudIndex_ = 0;
    uint32_t searchStart_ = 0;
    uint8_t probeIndex_ = 0;
    uint32_t lastSend_ = 0;
    uint32_t lastRx_ = 0;
    uint32_t lastBroadcast_ = 0;
    bool heardBroadcast_ = false;
    uint32_t lastPoll_ = 0;
    uint8_t pending_[2] = {};
    uint8_t pendingLen_ = 0;
};

}  // namespace civ
