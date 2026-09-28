// Executes protocol commands (PROTOCOL.md) shared by the BLE and HTTP transports.
#pragma once

#include <string>

#include "command.h"

namespace commands {

// Parses and executes one line, returns the response line (without "\n").
std::string execute(const std::string& line, proto::Mode active);

// MODE schedules a restart so the response can reach the client first.
void restartIfRequested();

}  // namespace commands
