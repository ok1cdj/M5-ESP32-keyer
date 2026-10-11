#include "core/commands.h"

#include <Arduino.h>

#include <atomic>

#include "core/keyer_task.h"
#include "radio/radio.h"
#include "settings.h"
#include "ui/ui.h"
#include "version.h"

namespace commands {

namespace {

constexpr uint32_t RESTART_DELAY_MS = 300;
volatile uint32_t restartAt = 0;
std::atomic<bool> watch{false};
std::atomic<bool> freqRequest{false};

}  // namespace

std::string execute(const std::string& line, proto::Mode active) {
    proto::Command c = proto::parse(line, active);
    switch (c.cmd) {
        case proto::Cmd::Send:
            return keyer_task::postText(c.text.data(), c.text.size()) ? "OK" : "ERR busy";
        case proto::Cmd::Wpm:
            return keyer_task::postWpm(uint8_t(c.wpm)) ? "OK" : "ERR busy";
        case proto::Cmd::Stop:
            return keyer_task::postStop() ? "OK" : "ERR busy";
        case proto::Cmd::Status: {
            keyer_task::Snapshot s = keyer_task::snapshot();
            return proto::formatStatus(s.sending, s.remaining, s.wpm);
        }
        case proto::Cmd::Ver:
            return proto::formatVersion(FW_VERSION);
        case proto::Cmd::Wifi:
            settings::setWifi(c.text, c.password);
            return "OK";
        case proto::Cmd::SetMode:
            keyer_task::postStop();
            settings::setMode(c.mode);
            restartAt = millis() + RESTART_DELAY_MS;
            if (restartAt == 0) restartAt = 1;
            return "OK";
        case proto::Cmd::ApiKey:
            settings::setApiKey(c.text);
            return "OK";
        case proto::Cmd::Freq: {
            civ::State st = radio::snapshot().state;
            return civ::formatFreq(st.valid, st.hz, st.mode);
        }
        case proto::Cmd::FreqWatch:
            watch = c.watch;
            freqRequest = c.watch;
            return "OK";
        case proto::Cmd::CivTx:
            switch (radio::sendFrame(c.frame.data(), c.frame.size())) {
                case radio::TxResult::Ok: return "OK";
                case radio::TxResult::NoRig: return "ERR norig";
                case radio::TxResult::Busy: break;
            }
            return "ERR busy";
        case proto::Cmd::CivRx:
            radio::setTap(c.watch);
            return "OK";
        case proto::Cmd::Civ:
            if (c.civQuery) {
                radio::Snapshot s = radio::snapshot();
                return civ::formatLink(s.cfg, s.state);
            }
            settings::setCivConfig(c.civ);
            radio::configure(c.civ);
            return "OK";
        case proto::Cmd::Rotate:
            if (!ui::hasDisplay()) return c.rotateQuery ? "ROTATE NONE" : "ERR nodisplay";
            if (c.rotateQuery) return "ROTATE " + std::to_string(ui::rotation() * 90);
            settings::setRotation(uint8_t(c.rotation));
            ui::setRotation(uint8_t(c.rotation));
            return "OK";
        case proto::Cmd::Error:
            break;
    }
    return std::string("ERR ") + c.error;
}

bool freqWatch() { return watch; }

void resetSession() {
    watch = false;
    freqRequest = false;
    radio::setTap(false);
}

bool takeFreqRequest() { return freqRequest.exchange(false); }

void restartIfRequested() {
    if (restartAt != 0 && int32_t(millis() - restartAt) >= 0) ESP.restart();
}

}  // namespace commands
