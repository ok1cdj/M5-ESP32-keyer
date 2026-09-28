// Status display on AtomS3 (128x128). Display only: the controls are the same
// as on the Lite boards (pressing the screen is the button).
#include <M5Unified.h>

#include "battery.h"
#include "core/keyer_task.h"
#include "ui/ui.h"

namespace ui {

namespace {

constexpr uint32_t BACKLIGHT_MS = 10 * 1000;
constexpr uint8_t BRIGHTNESS = 80;

proto::Mode mode = proto::Mode::Ble;
// Written from the Wi-Fi event task, read by loop().
char address[40];
portMUX_TYPE addressLock = portMUX_INITIALIZER_UNLOCKED;
volatile bool wakePending = false;
uint32_t darkAt = 0;
uint32_t lastActivity = 0;
std::string shown;

void wake() {
    M5.Display.setBrightness(BRIGHTNESS);
    darkAt = millis() + BACKLIGHT_MS;
}

std::string content() {
    keyer_task::Snapshot s = keyer_task::snapshot();
    char addr[sizeof(address)];
    portENTER_CRITICAL(&addressLock);
    memcpy(addr, address, sizeof(addr));
    portEXIT_CRITICAL(&addressLock);
    char buf[96];
    snprintf(buf, sizeof(buf), "%s\n%s\n%u WPM\n%u %%\n%s", proto::modeName(mode), addr, s.wpm,
             battery::percent(), s.sending ? ("TX " + std::to_string(s.remaining)).c_str() : "");
    return buf;
}

void draw(const std::string& text) {
    auto& d = M5.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setTextDatum(top_left);
    keyer_task::Snapshot s = keyer_task::snapshot();
    int y = 4;
    size_t start = 0;
    int lineNo = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        std::string line = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (lineNo == 0) {
            d.setTextSize(3);
        } else if (lineNo == 1) {
            d.setTextSize(1);
        } else {
            d.setTextSize(2);
        }
        if (lineNo == 3) {
            // battery bar: 4 cells
            uint8_t pct = battery::percent();
            for (int i = 0; i < 4; i++) {
                int x = 4 + i * 12;
                if (pct > i * 25) d.fillRect(x, y + 2, 10, 12, TFT_GREEN);
                else d.drawRect(x, y + 2, 10, 12, TFT_DARKGREY);
            }
            d.drawString(line.c_str(), 56, y);
        } else if (!line.empty()) {
            if (lineNo == 4 && s.sending) d.setTextColor(TFT_RED, TFT_BLACK);
            d.drawString(line.c_str(), 4, y);
            d.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        y += lineNo == 0 ? 30 : (lineNo == 1 ? 14 : 22);
        lineNo++;
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
}

}  // namespace

void begin() {
    M5.Display.setRotation(0);
    M5.Display.fillScreen(TFT_BLACK);
    wake();
}

void showModeChoice(proto::Mode m) {
    auto& d = M5.Display;
    d.setBrightness(BRIGHTNESS);
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setTextDatum(middle_center);
    d.setTextSize(4);
    d.drawString(proto::modeName(m), d.width() / 2, d.height() / 2);
}

void showMode(proto::Mode m) {
    mode = m;
    shown.clear();
    wake();
}

void clientEvent() { wakePending = true; }

void setAddress(const std::string& a) {
    portENTER_CRITICAL(&addressLock);
    strlcpy(address, a.c_str(), sizeof(address));
    portEXIT_CRITICAL(&addressLock);
    wakePending = true;
}

void loop() {
    uint32_t act = keyer_task::activityCounter();
    if (wakePending || act != lastActivity) {
        wakePending = false;
        lastActivity = act;
        wake();
    }
    std::string c = content();
    if (c != shown) {
        shown = c;
        draw(c);
    }
    if (darkAt != 0 && int32_t(millis() - darkAt) >= 0) {
        M5.Display.setBrightness(0);
        darkAt = 0;
    }
}

}  // namespace ui
