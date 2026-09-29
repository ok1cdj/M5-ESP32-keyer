#include <esp_mac.h>
#include <stdio.h>

#include "transport/transport.h"

namespace transport {

std::string deviceName() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    char name[16];
    snprintf(name, sizeof(name), "keyer-%02X%02X", mac[4], mac[5]);
    return name;
}

}  // namespace transport
