// cwdaemon transport (UDP 6789) for logging programs. Plain text is sent,
// escape requests: ESC 0 reset and ESC 4 abort -> STOP, ESC 2<n> -> speed.
// Other escape requests are ignored.
#include <Arduino.h>
#include <AsyncUDP.h>

#include "element_generator.h"
#include "core/keyer_task.h"
#include "transport/transport.h"
#include "transport/wifi_station.h"

namespace transport {

namespace {

constexpr uint16_t PORT = 6789;
constexpr uint8_t ESC = 27;

void onPacket(AsyncUDPPacket& packet) {
    const uint8_t* data = packet.data();
    size_t len = packet.length();
    if (len == 0) return;

    if (data[0] == ESC) {
        if (len < 2) return;
        switch (data[1]) {
            case '0':  // reset
            case '4':  // abort message
                keyer_task::postStop();
                break;
            case '2': {  // speed
                int wpm = 0;
                size_t i = 2;
                for (; i < len && data[i] >= '0' && data[i] <= '9' && i < 6; i++) wpm = wpm * 10 + (data[i] - '0');
                if (i > 2 && wpm >= keyer::WPM_MIN && wpm <= keyer::WPM_MAX) keyer_task::postWpm(uint8_t(wpm));
                break;
            }
            default:
                break;
        }
        return;
    }

    // Trailing line breaks and the echo request '^' are not part of the message.
    while (len > 0 && (data[len - 1] == '\n' || data[len - 1] == '\r' || data[len - 1] == '^' || data[len - 1] == 0)) len--;
    if (len > 0) keyer_task::postText(reinterpret_cast<const char*>(data), len);
}

class Cwdaemon : public Transport {
public:
    void begin() override {
        wifi_station::begin("cwdaemon", "udp", PORT);
        if (udp_.listen(PORT)) {
            udp_.onPacket(onPacket);
            Serial.printf("[cwd] listening on UDP %u\n", PORT);
        }
    }

    void notify(const char*) override {}

    std::string address() const override { return wifi_station::address(); }

private:
    AsyncUDP udp_;
};

}  // namespace

Transport& cwdaemon() {
    static Cwdaemon instance;
    return instance;
}

}  // namespace transport
