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
#include "ui/mode_select.h"
#include "settings.h"
#include "transport/transport.h"
#include "ui/ui.h"
#include "version.h"

namespace {

transport::Transport* active = nullptr;

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
    ui::begin();
    mode_select::runIfButtonHeld();

    proto::Mode mode = settings::mode();
    Serial.printf("mode %s\n", proto::modeName(mode));

    battery::begin();
    keyer_task::begin(settings::wpm());

    active = &transportFor(mode);
    keyer_task::setNotifier(notifyClient);
    active->begin();
    ui::showMode(mode);
}

void loop() {
    battery::loop();
    active->loop();
    ui::loop();
    commands::restartIfRequested();
    delay(20);
}
