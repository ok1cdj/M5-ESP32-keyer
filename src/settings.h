// Persistent settings in NVS.
#pragma once

#include <stdint.h>

#include <string>

#include "command.h"

namespace settings {

void begin();

proto::Mode mode();
void setMode(proto::Mode m);

uint8_t wpm();
void setWpm(uint8_t wpm);

std::string ssid();
std::string password();
void setWifi(const std::string& ssid, const std::string& password);

// Empty key = HTTP authentication off.
std::string apiKey();
void setApiKey(const std::string& key);

}  // namespace settings
