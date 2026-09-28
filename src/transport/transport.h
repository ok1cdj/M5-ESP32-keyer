// A transport translates its input into keyer commands. Exactly one runs.
#pragma once

#include <string>

namespace transport {

class Transport {
public:
    virtual ~Transport() = default;
    virtual void begin() = 0;
    virtual void loop() {}
    // Asynchronous message from the keyer core (called from the keyer task).
    virtual void notify(const char* line) = 0;
    // Shown on the status display; empty when there is none.
    virtual std::string address() const { return std::string(); }
};

Transport& ble();
Transport& http();
Transport& cwdaemon();

// "keyer-XXXX" from the last two bytes of the MAC address.
std::string deviceName();

}  // namespace transport
