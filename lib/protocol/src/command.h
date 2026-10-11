// Text protocol (PROTOCOL.md, proto 1): one line = one command. Pure C++.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "civ.h"

namespace proto {

constexpr int PROTO_VERSION = 1;
constexpr int WPM_MIN = 5;
constexpr int WPM_MAX = 50;

enum class Mode : uint8_t { Ble = 0, Http = 1, Cwd = 2 };

const char* modeName(Mode m);
bool parseMode(const std::string& s, Mode& out);

enum class Cmd : uint8_t {
    Send,
    Wpm,
    Stop,
    Status,
    Ver,
    Wifi,
    SetMode,
    ApiKey,
    Freq,
    FreqWatch,  // FREQ ON / OFF: `watch`
    Civ,    // civQuery, or set `civ`
    CivTx,  // a CI-V frame for the radio: `frame`
    CivRx,  // CIVRX ON / OFF: `watch`
    Rotate,  // rotateQuery, or set `rotation`
    Error,  // answer with "ERR " + error
};

struct Command {
    Cmd cmd = Cmd::Error;
    const char* error = "";  // Cmd::Error: "cmd", "arg", "range", "mode"
    std::string text;        // SEND text, WIFI ssid, APIKEY key
    std::string password;    // WIFI password
    int wpm = 0;
    Mode mode = Mode::Ble;
    bool watch = false;
    bool civQuery = false;
    bool rotateQuery = false;
    int rotation = 0;  // ROTATE: display rotation in quarter turns, 0-3
    civ::Config civ;
    std::vector<uint8_t> frame;  // CIVTX, checked by civ::addressFrame
};

// Parses one line. `active` is the running mode: WIFI, MODE, APIKEY and CIV with
// arguments are accepted only over BLE.
Command parse(const std::string& line, Mode active);

// Response formatting shared by all transports.
std::string formatStatus(bool sending, size_t remaining, int wpm);
std::string formatVersion(const char* fwVersion);

}  // namespace proto
