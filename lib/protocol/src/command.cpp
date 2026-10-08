#include "command.h"

namespace proto {

namespace {

std::string upper(std::string s) {
    for (char& c : s) {
        if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    }
    return s;
}

Command error(const char* e) {
    Command c;
    c.cmd = Cmd::Error;
    c.error = e;
    return c;
}

bool parseInt(const std::string& s, int& out, size_t maxDigits = 4) {
    if (s.empty() || s.size() > maxDigits) return false;
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}

bool parseHex(const std::string& s, int& out) {
    if (s.empty() || s.size() > 2) return false;
    int v = 0;
    for (char c : s) {
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else return false;
        v = v * 16 + d;
    }
    out = v;
    return true;
}

// CIV, CIV OFF, CIV AUTO, CIV <baud> <hex address>
Command parseCiv(const std::string& arg) {
    Command c;
    c.cmd = Cmd::Civ;
    std::string u = upper(arg);
    if (u.empty()) {
        c.civQuery = true;
    } else if (u == "OFF") {
        c.civ.mode = civ::LinkMode::Off;
    } else if (u == "AUTO") {
        c.civ.mode = civ::LinkMode::Auto;
    } else {
        size_t sp = u.find(' ');
        if (sp == std::string::npos) return error("arg");
        int baud, rig;
        if (!parseInt(u.substr(0, sp), baud, 5) || !parseHex(u.substr(sp + 1), rig)) {
            return error("arg");
        }
        // 00 is the broadcast, E0 and up are controllers.
        if (!civ::validBaud(uint32_t(baud)) || rig == 0 || rig >= civ::CONTROLLER) {
            return error("range");
        }
        c.civ.mode = civ::LinkMode::Fixed;
        c.civ.baud = uint32_t(baud);
        c.civ.rig = uint8_t(rig);
    }
    return c;
}

}  // namespace

const char* modeName(Mode m) {
    switch (m) {
        case Mode::Ble: return "BLE";
        case Mode::Http: return "HTTP";
        case Mode::Cwd: return "CWD";
    }
    return "?";
}

bool parseMode(const std::string& s, Mode& out) {
    std::string u = upper(s);
    if (u == "BLE") out = Mode::Ble;
    else if (u == "HTTP") out = Mode::Http;
    else if (u == "CWD") out = Mode::Cwd;
    else return false;
    return true;
}

Command parse(const std::string& line, Mode active) {
    size_t sp = line.find(' ');
    std::string word = upper(line.substr(0, sp));
    std::string arg = sp == std::string::npos ? std::string() : line.substr(sp + 1);

    Command c;
    if (word == "SEND") {
        if (arg.empty()) return error("arg");
        c.cmd = Cmd::Send;
        c.text = arg;
    } else if (word == "WPM") {
        int v;
        if (!parseInt(arg, v)) return error("arg");
        if (v < WPM_MIN || v > WPM_MAX) return error("range");
        c.cmd = Cmd::Wpm;
        c.wpm = v;
    } else if (word == "STOP" && arg.empty()) {
        c.cmd = Cmd::Stop;
    } else if (word == "STATUS" && arg.empty()) {
        c.cmd = Cmd::Status;
    } else if (word == "VER" && arg.empty()) {
        c.cmd = Cmd::Ver;
    } else if (word == "WIFI") {
        if (active != Mode::Ble) return error("mode");
        size_t tab = arg.find('\t');
        if (tab == std::string::npos || tab == 0) return error("arg");
        c.cmd = Cmd::Wifi;
        c.text = arg.substr(0, tab);
        c.password = arg.substr(tab + 1);
    } else if (word == "MODE") {
        if (active != Mode::Ble) return error("mode");
        if (!parseMode(arg, c.mode)) return error("arg");
        c.cmd = Cmd::SetMode;
    } else if (word == "APIKEY") {
        if (active != Mode::Ble) return error("mode");
        c.cmd = Cmd::ApiKey;
        c.text = arg;  // empty = authentication off
    } else if (word == "FREQ" && arg.empty()) {
        c.cmd = Cmd::Freq;
    } else if (word == "FREQ") {
        // Asynchronous FREQ messages only reach BLE clients that ask for them.
        if (active != Mode::Ble) return error("mode");
        std::string u = upper(arg);
        if (u != "ON" && u != "OFF") return error("arg");
        c.cmd = Cmd::FreqWatch;
        c.watch = u == "ON";
    } else if (word == "CIVTX") {
        // Raw CI-V for clients that drive the radio themselves (BLE only).
        if (active != Mode::Ble) return error("mode");
        uint8_t b[civ::Frame::MAX_BYTES];
        size_t n = civ::fromHex(arg, b, sizeof b);
        if (n == 0 || !civ::addressFrame(b, n, 0)) return error("arg");
        c.cmd = Cmd::CivTx;
        c.frame.assign(b, b + n);
    } else if (word == "CIVRX") {
        if (active != Mode::Ble) return error("mode");
        std::string u = upper(arg);
        if (u != "ON" && u != "OFF") return error("arg");
        c.cmd = Cmd::CivRx;
        c.watch = u == "ON";
    } else if (word == "CIV") {
        // Reading is harmless, changing is BLE-only like the other settings.
        if (!arg.empty() && active != Mode::Ble) return error("mode");
        return parseCiv(arg);
    } else {
        return error("cmd");
    }
    return c;
}

std::string formatStatus(bool sending, size_t remaining, int wpm) {
    std::string s = sending ? "SENDING " + std::to_string(remaining) + " " : "IDLE ";
    return s + "WPM " + std::to_string(wpm);
}

std::string formatVersion(const char* fwVersion) {
    return std::string("VER keyer ") + fwVersion + " proto " + std::to_string(PROTO_VERSION);
}

}  // namespace proto
