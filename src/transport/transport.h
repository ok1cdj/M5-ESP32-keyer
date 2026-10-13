// A transport translates its input into keyer commands. Exactly one runs.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace transport {

class Transport {
public:
    virtual ~Transport() = default;
    virtual void begin() = 0;
    virtual void loop() {}
    // Asynchronous message from the keyer core (called from the keyer task).
    virtual void notify(const char* line) = 0;
    // A CI-V frame from the radio for a binary CI-V client (BLE only).
    virtual void notifyRaw(const uint8_t*, size_t) {}
    // Shown on the status display; empty when there is none.
    virtual std::string address() const { return std::string(); }
};

Transport& ble();
Transport& http();
Transport& cwdaemon();

// "keyer-XXXX" from the last two bytes of the MAC address.
std::string deviceName();

}  // namespace transport
