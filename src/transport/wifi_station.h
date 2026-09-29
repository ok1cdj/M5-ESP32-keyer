// Wi-Fi client for the HTTP and CWD modes: credentials from NVS (set over BLE
// with the WIFI command), DHCP, mDNS name keyer-XXXX.local. No AP, no portal.
#pragma once

#include <string>

namespace wifi_station {

// mdnsService/mdnsProto: advertised mDNS service, e.g. "http"/"tcp".
void begin(const char* mdnsService, const char* mdnsProto, uint16_t port);

// "192.168.1.42", or a short status text while not connected.
std::string address();

}  // namespace wifi_station
