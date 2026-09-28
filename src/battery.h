// Battery voltage on the Atomic Battery Base (1:2 divider on BAT_PIN).
#pragma once

#include <stdint.h>

namespace battery {

void begin();
void loop();  // samples every 30 s
uint8_t percent();
uint32_t millivolts();

}  // namespace battery
