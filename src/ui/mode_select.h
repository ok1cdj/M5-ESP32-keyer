#pragma once

namespace mode_select {

// Button held at power-on: cycle the modes (~1 s each); releasing the button
// stores the shown mode and restarts. Returns immediately otherwise.
void runIfButtonHeld();

}  // namespace mode_select
