// M5-ESP32-keyer v2 — CW keyer for the M5Stack Atom family.
// Copyright (C) OK1CDJ, GPL-3.0-or-later.
//
// Boot order matters for safety: the key output is opened by a static
// constructor in key_output.cpp before anything here runs, and again right
// after M5Unified has initialised the board.
#include <M5Unified.h>

#include "battery.h"
#include "core/commands.h"
#include "core/key_output.h"
#include "core/keyer_task.h"
#include "radio/radio.h"
#include "ui/mode_select.h"
#include "settings.h"
#include "transport/transport.h"
#include "ui/ui.h"
#include "version.h"

namespace {

// Turning the VFO knob broadcasts a frequency every few ms.
constexpr uint32_t FREQ_NOTIFY_GAP_MS = 200;

transport::Transport* active = nullptr;
uint32_t freqSeq = 0;
uint32_t freqSentAt = 0;

void notifyClient(const char* line) {
    if (active) active->notify(line);
}

transport::Transport& transportFor(proto::Mode m) {
    switch (m) {
        case proto::Mode::Http: return transport::http();
        case proto::Mode::Cwd: return transport::cwdaemon();
        case proto::Mode::Ble: break;
    }
    return transport::ble();
}

// Passes frequency and mode changes from the radio to the display and, after
// FREQ ON, to the client: at most one per FREQ_NOTIFY_GAP_MS, the latest one
// always goes out.
void reportFrequency() {
    civ::State st = radio::snapshot().state;
    bool request = commands::takeFreqRequest();
    if (!request && (st.seq == freqSeq || uint32_t(millis() - freqSentAt) < FREQ_NOTIFY_GAP_MS)) return;
    freqSeq = st.seq;
    freqSentAt = millis();
    if (commands::freqWatch()) notifyClient(civ::formatFreq(st.valid, st.hz, st.mode).c_str());
    ui::setRadio(civ::formatDisplay(st.valid, st.hz, st.mode));
}

// CIVRX ON: every frame from the radio to the client, as it came.
void forwardCiv() {
    uint8_t b[civ::Frame::MAX_BYTES];
    size_t n;
    while (radio::takeTapped(b, n)) notifyClient(("CIVRX " + civ::toHex(b, n)).c_str());
}

}  // namespace

void setup() {
    key_output::init();

    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.external_display_value = 0;  // don't probe the header pins for add-on displays
    cfg.external_speaker_value = 0;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    M5.begin(cfg);
    key_output::init();

    Serial.printf("\nM5-ESP32-keyer %s (%s)\n", FW_VERSION, BOARD_NAME);

    settings::begin();
    ui::setRotation(settings::rotation());
    ui::begin();
    mode_select::runIfButtonHeld();

    proto::Mode mode = settings::mode();
    Serial.printf("mode %s\n", proto::modeName(mode));

    battery::begin();
    keyer_task::begin(settings::wpm());
    radio::begin(settings::civConfig());

    active = &transportFor(mode);
    keyer_task::setNotifier(notifyClient);
    active->begin();
    ui::showMode(mode);
}

void loop() {
    battery::loop();
    active->loop();
    reportFrequency();
    forwardCiv();
    ui::loop();
    commands::restartIfRequested();
    delay(20);
}
