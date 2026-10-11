// Status output: the RGB LED on the Lite boards (ui_led.cpp), the display on
// AtomS3 (ui_display.cpp). platformio.ini picks one per environment.
#pragma once

#include <string>

#include "command.h"

namespace ui {

bool hasDisplay();
// Display rotation in quarter turns, 0-3; before begin() or from any task.
void setRotation(uint8_t r);
uint8_t rotation();
void begin();
void showModeChoice(proto::Mode m);  // boot-time mode selection
void showMode(proto::Mode m);        // after start, then dark
void clientEvent();                  // client connected / disconnected; any task
void buttonPressed();                // button pressed (after debounce); any task
void setAddress(const std::string& address);
void setRadio(const std::string& text);  // frequency and mode, "" = none; from loop()
void loop();

}  // namespace ui
