#include "battery.h"

#include <Arduino.h>

#include "battery_curve.h"

namespace battery {

namespace {

constexpr uint32_t PERIOD_MS = 30 * 1000;
constexpr int SAMPLES = 16;

// The base's divider is 2 x 1 MOhm, and the ADC reads that high source impedance
// low. Measured on an AtomS3 at the charger's 4.20 V end of charge: 4045 mV, so
// the S3 builds set BAT_CAL_PERMILLE=1038. 1000 means no correction.
#ifndef BAT_CAL_PERMILLE
#define BAT_CAL_PERMILLE 1000
#endif
uint32_t lastSample = 0;
uint32_t mv = 0;

void sample() {
    uint32_t sum = 0;
    for (int i = 0; i < SAMPLES; i++) sum += analogReadMilliVolts(BAT_PIN);
    mv = 2 * sum / SAMPLES * BAT_CAL_PERMILLE / 1000;  // 1:2 divider, calibrated
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
