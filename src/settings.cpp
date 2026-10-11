#include "settings.h"

#include <Preferences.h>

#include "element_generator.h"

namespace settings {

namespace {

// Same namespace and keys (ssid, password, apikey) as v1.
constexpr const char* NS = "keyer";
Preferences prefs;

// isKey() first: Preferences logs an error for every missing key.
std::string getString(const char* key) {
    if (!prefs.isKey(key)) return {};
    return std::string(prefs.getString(key, "").c_str());
}

}  // namespace

void begin() { prefs.begin(NS, false); }

proto::Mode mode() {
    uint8_t m = prefs.getUChar("mode", uint8_t(proto::Mode::Ble));
    return m <= uint8_t(proto::Mode::Cwd) ? proto::Mode(m) : proto::Mode::Ble;
}

void setMode(proto::Mode m) { prefs.putUChar("mode", uint8_t(m)); }

uint8_t wpm() {
    uint8_t w = prefs.getUChar("wpm", keyer::WPM_DEFAULT);
    return (w >= keyer::WPM_MIN && w <= keyer::WPM_MAX) ? w : keyer::WPM_DEFAULT;
}

void setWpm(uint8_t wpm) {
    if (prefs.getUChar("wpm", 0) != wpm) prefs.putUChar("wpm", wpm);
}

std::string ssid() { return getString("ssid"); }
std::string password() { return getString("password"); }

void setWifi(const std::string& ssid, const std::string& password) {
    prefs.putString("ssid", ssid.c_str());
    prefs.putString("password", password.c_str());
}

std::string apiKey() { return getString("apikey"); }
void setApiKey(const std::string& key) { prefs.putString("apikey", key.c_str()); }

uint8_t rotation() { return prefs.getUChar("rot", 0) & 3; }

void setRotation(uint8_t r) {
    if (prefs.getUChar("rot", 0) != r) prefs.putUChar("rot", r);
}

civ::Config civConfig() {
    civ::Config c;
    uint8_t m = prefs.getUChar("civ", uint8_t(civ::LinkMode::Auto));
    c.mode = m <= uint8_t(civ::LinkMode::Fixed) ? civ::LinkMode(m) : civ::LinkMode::Auto;
    if (c.mode == civ::LinkMode::Fixed) {
        c.baud = prefs.getUInt("civbaud", 0);
        c.rig = prefs.getUChar("civaddr", 0);
        if (!civ::validBaud(c.baud) || c.rig == 0) c = civ::Config();
    }
    return c;
}

void setCivConfig(const civ::Config& cfg) {
    prefs.putUChar("civ", uint8_t(cfg.mode));
    if (cfg.mode == civ::LinkMode::Fixed) {
        prefs.putUInt("civbaud", cfg.baud);
        prefs.putUChar("civaddr", cfg.rig);
    }
}

void civLast(uint8_t& rig, uint32_t& baud) {
    rig = prefs.getUChar("civlrig", 0);
    baud = prefs.getUInt("civlbaud", 0);
}

void setCivLast(uint8_t rig, uint32_t baud) {
    if (prefs.getUChar("civlrig", 0) != rig) prefs.putUChar("civlrig", rig);
    if (prefs.getUInt("civlbaud", 0) != baud) prefs.putUInt("civlbaud", baud);
}

}  // namespace settings
