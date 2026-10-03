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
constexpr int MARGIN = 4;

// One status item per line. Each line is redrawn on its own: clearing the
// whole screen before every update makes it flicker.
enum Line { MODE, ADDRESS, WPM, BATTERY, TICKER, LINES };
struct LineLayout {
    int y;
    uint8_t size;  // text size, the built-in font is 6x8 per unit
};
constexpr LineLayout LAYOUT[LINES] = {{4, 3}, {34, 1}, {48, 2}, {70, 2}, {92, 2}};
constexpr int BATTERY_TEXT_X = 56;
// Characters of the ticker that fit the 128 px width at text size 2.
constexpr size_t TICKER_CHARS = 10;
static_assert(TICKER_CHARS <= keyer_task::PENDING_MAX, "ticker longer than the pending text");

proto::Mode mode = proto::Mode::Ble;
// Written from the Wi-Fi event task, read by loop().
char address[40];
portMUX_TYPE addressLock = portMUX_INITIALIZER_UNLOCKED;
volatile bool wakePending = false;
uint32_t darkAt = 0;
uint32_t lastActivity = 0;
std::string shown[LINES];
bool clearAll = true;  // screen content unknown, clear it before drawing
bool asleep = false;   // panel in sleep mode, backlight off

void wake() {
    if (asleep) {
        M5.Display.wakeup();
        asleep = false;
        clearAll = true;  // redraw what changed while the panel slept
    }
    M5.Display.setBrightness(BRIGHTNESS);
    darkAt = millis() + BACKLIGHT_MS;
}

// Backlight off alone leaves the panel controller running (~10 mA);
// sleep mode stops it as well.
void dark() {
    M5.Display.setBrightness(0);
    M5.Display.sleep();
    asleep = true;
}

void content(std::string (&lines)[LINES]) {
    keyer_task::Snapshot s = keyer_task::snapshot();
    char addr[sizeof(address)];
    portENTER_CRITICAL(&addressLock);
    memcpy(addr, address, sizeof(addr));
    portEXIT_CRITICAL(&addressLock);
    char buf[16];
    lines[MODE] = proto::modeName(mode);
    lines[ADDRESS] = addr;
    snprintf(buf, sizeof(buf), "%u WPM", s.wpm);
    lines[WPM] = buf;
    snprintf(buf, sizeof(buf), "%u %%", battery::percent());
    lines[BATTERY] = buf;
    char pending[keyer_task::PENDING_MAX + 1];
    keyer_task::pendingText(pending);
    lines[TICKER] = std::string(pending).substr(0, TICKER_CHARS);
}

// Text is drawn with its background, the rest of the line is cleared after
// it, so the line never goes blank in between.
void drawLine(Line i, const std::string& text) {
    auto& d = M5.Display;
    const LineLayout& l = LAYOUT[i];
    d.setTextDatum(top_left);
    d.setTextSize(l.size);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    int x = MARGIN;
    if (i == BATTERY) {
        // battery bar: 4 cells
        uint8_t pct = battery::percent();
        for (int c = 0; c < 4; c++) {
            int cx = MARGIN + c * 12;
            d.fillRect(cx, l.y + 2, 10, 12, pct > c * 25 ? TFT_GREEN : TFT_BLACK);
            if (pct <= c * 25) d.drawRect(cx, l.y + 2, 10, 12, TFT_DARKGREY);
        }
        x = BATTERY_TEXT_X;
    }
    if (i == TICKER && !text.empty()) {
        // the character being sent in red, the rest of the text after it
        d.setTextColor(TFT_RED, TFT_BLACK);
        x += d.drawString(text.substr(0, 1).c_str(), x, l.y);
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        x += d.drawString(text.c_str() + 1, x, l.y);
    } else {
        x += d.drawString(text.c_str(), x, l.y);
    }
    if (x < d.width()) d.fillRect(x, l.y, d.width() - x, 8 * l.size, TFT_BLACK);
}

}  // namespace

void begin() {
    M5.Display.setRotation(0);
    M5.Display.fillScreen(TFT_BLACK);
    wake();
}

void showModeChoice(proto::Mode m) {
    auto& d = M5.Display;
    wake();
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setTextDatum(middle_center);
    d.setTextSize(4);
    d.drawString(proto::modeName(m), d.width() / 2, d.height() / 2);
    clearAll = true;
}

void showMode(proto::Mode m) {
    mode = m;
    wake();
}

void clientEvent() { wakePending = true; }

void buttonPressed() { wakePending = true; }

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
    // Stay lit while sending so the ticker can be followed.
    if (!asleep && keyer_task::snapshot().sending) darkAt = millis() + BACKLIGHT_MS;
    if (!asleep) {
        if (clearAll) {
            M5.Display.fillScreen(TFT_BLACK);
            for (std::string& l : shown) l.assign(1, '\0');  // matches no content
            clearAll = false;
        }
        std::string lines[LINES];
        content(lines);
        for (int i = 0; i < LINES; i++) {
            if (lines[i] != shown[i]) {
                drawLine(Line(i), lines[i]);
                shown[i] = lines[i];
            }
        }
    }
    if (darkAt != 0 && int32_t(millis() - darkAt) >= 0) {
        dark();
        darkAt = 0;
    }
}

}  // namespace ui
