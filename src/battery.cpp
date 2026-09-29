#include "battery.h"

#include <Arduino.h>

#include "battery_curve.h"

namespace battery {

namespace {

constexpr uint32_t PERIOD_MS = 30 * 1000;
constexpr int SAMPLES = 16;
uint32_t lastSample = 0;
uint32_t mv = 0;

void sample() {
    uint32_t sum = 0;
    for (int i = 0; i < SAMPLES; i++) sum += analogReadMilliVolts(BAT_PIN);
    mv = 2 * sum / SAMPLES;  // 1:2 divider
    lastSample = millis();
    Serial.printf("[bat] %u mV %u %%\n", unsigned(mv), keyer::batteryPercent(mv));
}

}  // namespace

// analogReadMilliVolts() configures the pin (11 dB attenuation by default).
void begin() { sample(); }

void loop() {
    if (millis() - lastSample >= PERIOD_MS) sample();
}

uint8_t percent() { return keyer::batteryPercent(mv); }
uint32_t millivolts() { return mv; }

}  // namespace battery
