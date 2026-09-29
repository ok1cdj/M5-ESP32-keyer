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

bool parseInt(const std::string& s, int& out) {
    if (s.empty() || s.size() > 4) return false;
    int v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
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
