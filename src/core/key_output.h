// The key output (GPIO -> resistor -> optocoupler LED). Only keyer_task
// switches it; everything else may only force it open.
#pragma once

namespace key_output {

// Drives the pin low. Also runs from a static constructor before setup(),
// so the output is open before anything else is initialised.
void init();

void set(bool on);

}  // namespace key_output
