// Hardware layer of the keyer core: one FreeRTOS task owns the state machine
// (lib/keyer_core), executes its actions and is the only code that switches
// the key output. Transports and ISRs only post events.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace keyer_task {

// Receives asynchronous messages ("DONE", "STOPPED", "ERR watchdog", ...).
// Called from the keyer task.
typedef void (*NotifyFn)(const char* line);

void begin(uint8_t wpm);
void setNotifier(NotifyFn fn);

// Returns false when the event queue is full.
bool postText(const char* text, size_t len);
bool postStop();
bool postWpm(uint8_t wpm);

struct Snapshot {
    bool sending;
    uint16_t remaining;
    uint8_t wpm;
};
Snapshot snapshot();

// Incremented on every state change the UI should react to (start, end, STOP).
uint32_t activityCounter();

}  // namespace keyer_task
