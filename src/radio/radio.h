// Icom CI-V link to the radio on the Grove SCL pin (CIV_PIN): one wire,
// open-drain, TX and RX of UART1 on the same pin. Frequency and mode only.
#pragma once

#include "civ.h"

namespace radio {

void begin(const civ::Config& cfg);
// Applies a new configuration at once (detection starts again). Any task.
void configure(const civ::Config& cfg);

struct Snapshot {
    civ::Config cfg;
    civ::State state;
};
Snapshot snapshot();  // any task

// CI-V tunnel (CIVTX / CIVRX) for clients that drive the radio themselves.
enum class TxResult { Ok, NoRig, Busy };
// Queues one frame for the bus, addressed from us to the rig found. Any task.
TxResult sendFrame(const uint8_t* bytes, size_t n);
// While on, every frame from the rig is kept for takeTapped(). Any task.
void setTap(bool on);
// The next frame heard from the rig; false when there is none. One task only.
bool takeTapped(uint8_t* out, size_t& n);

}  // namespace radio
