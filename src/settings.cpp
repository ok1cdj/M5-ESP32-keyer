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

}  // namespace settings
