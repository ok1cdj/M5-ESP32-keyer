// Status output: the RGB LED on the Lite boards (ui_led.cpp), the display on
// AtomS3 (ui_display.cpp). platformio.ini picks one per environment.
#pragma once

#include <string>

#include "command.h"

namespace ui {

void begin();
void showModeChoice(proto::Mode m);  // boot-time mode selection
void showMode(proto::Mode m);        // after start, then dark
void clientEvent();                  // client connected / disconnected; any task
void buttonPressed();                // button pressed (after debounce); any task
void setAddress(const std::string& address);
void loop();

}  // namespace ui
