#include "core/commands.h"

#include <Arduino.h>

#include "core/keyer_task.h"
#include "settings.h"
#include "version.h"

namespace commands {

namespace {

constexpr uint32_t RESTART_DELAY_MS = 300;
volatile uint32_t restartAt = 0;

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
        case proto::Cmd::Error:
            break;
    }
    return std::string("ERR ") + c.error;
}

void restartIfRequested() {
    if (restartAt != 0 && int32_t(millis() - restartAt) >= 0) ESP.restart();
}

}  // namespace commands
