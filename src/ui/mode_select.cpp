#include "ui/mode_select.h"

#include <Arduino.h>

#include "settings.h"
#include "ui/ui.h"

namespace mode_select {

namespace {

constexpr uint32_t STEP_MS = 1000;
constexpr uint32_t DEBOUNCE_MS = 50;

bool pressed() { return digitalRead(BTN_PIN) == LOW; }

// True when the button stayed released for DEBOUNCE_MS.
bool released() {
    if (pressed()) return false;
    delay(DEBOUNCE_MS);
    return !pressed();
}

}  // namespace

void runIfButtonHeld() {
    pinMode(BTN_PIN, INPUT);
    if (!pressed()) return;
    delay(DEBOUNCE_MS);
    if (!pressed()) return;

    const proto::Mode modes[] = {proto::Mode::Ble, proto::Mode::Http, proto::Mode::Cwd};
    size_t idx = 0;
    for (;;) {
        ui::showModeChoice(modes[idx]);
        uint32_t shownAt = millis();
        while (millis() - shownAt < STEP_MS) {
            if (released()) {
                settings::setMode(modes[idx]);
                Serial.printf("[mode] %s selected, restarting\n", proto::modeName(modes[idx]));
                delay(300);
                ESP.restart();
            }
            delay(10);
        }
        idx = (idx + 1) % 3;
    }
}

}  // namespace mode_select
