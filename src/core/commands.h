// Executes protocol commands (PROTOCOL.md) shared by the BLE and HTTP transports.
#pragma once

#include <string>

#include "command.h"

namespace commands {

// Parses and executes one line, returns the response line (without "\n").
std::string execute(const std::string& line, proto::Mode active);

// FREQ ON / OFF and CIVRX ON / OFF of the connected BLE client. A new
// connection starts with both off, so clients that don't know them never get
// the asynchronous messages.
bool freqWatch();
void resetSession();

// Binary CI-V session (OrbitDeck and other apps made for a BLE-to-CI-V
// adapter): the connection carries raw frames both ways and no text lines.
// Turns the CIVRX tap on; resetSession() ends it.
void setRawCiv(bool on);
bool rawCiv();
// True once after FREQ ON: the current value goes out at once.
bool takeFreqRequest();

// MODE schedules a restart so the response can reach the client first.
void restartIfRequested();

}  // namespace commands
