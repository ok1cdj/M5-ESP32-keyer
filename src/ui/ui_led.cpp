// Status on the RGB LED (Atom Lite, AtomS3 Lite). Dark except for the mode
// colour after start and short blinks on client connect / disconnect and on
// a button press.
#include <M5Unified.h>

#include "ui/mode_color.h"
#include "ui/ui.h"

namespace ui {

namespace {

constexpr uint32_t MODE_SHOW_MS = 2000;
constexpr uint32_t BLINK_MS = 80;
constexpr uint8_t BRIGHTNESS = 40;

proto::Mode mode = proto::Mode::Ble;
uint32_t offAt = 0;
volatile bool blinkPending = false;

void set(Rgb c) { M5.Led.setAllColor(c.r, c.g, c.b); }

void off() { M5.Led.setAllColor(0, 0, 0); }

}  // namespace

void begin() {
    M5.Led.setBrightness(BRIGHTNESS);
    off();
}

void showModeChoice(proto::Mode m) { set(modeColor(m)); }

void showMode(proto::Mode m) {
    mode = m;
    set(modeColor(m));
    offAt = millis() + MODE_SHOW_MS;
}

void clientEvent() { blinkPending = true; }

void buttonPressed() { blinkPending = true; }

void setAddress(const std::string&) {}

void loop() {
    if (blinkPending) {
        blinkPending = false;
        set(modeColor(mode));
        offAt = millis() + BLINK_MS;
    }
    if (offAt != 0 && int32_t(millis() - offAt) >= 0) {
        off();
        offAt = 0;
    }
}

}  // namespace ui
