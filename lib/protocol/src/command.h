// Text protocol (PROTOCOL.md, proto 1): one line = one command. Pure C++.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

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
    Error,  // answer with "ERR " + error
};

struct Command {
    Cmd cmd = Cmd::Error;
    const char* error = "";  // Cmd::Error: "cmd", "arg", "range", "mode"
    std::string text;        // SEND text, WIFI ssid, APIKEY key
    std::string password;    // WIFI password
    int wpm = 0;
    Mode mode = Mode::Ble;
};

// Parses one line. `active` is the running mode: WIFI, MODE and APIKEY are
// accepted only over BLE.
Command parse(const std::string& line, Mode active);

// Response formatting shared by all transports.
std::string formatStatus(bool sending, size_t remaining, int wpm);
std::string formatVersion(const char* fwVersion);

}  // namespace proto
