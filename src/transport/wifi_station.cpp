#include "transport/wifi_station.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "settings.h"
#include "transport/transport.h"
#include "ui/ui.h"

namespace wifi_station {

namespace {

std::string hostname;
const char* service;
const char* proto;
uint16_t servicePort;
volatile bool connected = false;

void onEvent(arduino_event_id_t event, arduino_event_info_t) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            connected = true;
            Serial.printf("[wifi] %s, http://%s.local\n", WiFi.localIP().toString().c_str(), hostname.c_str());
            MDNS.end();
            if (MDNS.begin(hostname.c_str())) MDNS.addService(service, proto, servicePort);
            ui::setAddress(address());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            if (connected) {
                connected = false;
                ui::setAddress(address());
            }
            break;
        default:
            break;
    }
}

}  // namespace

void begin(const char* mdnsService, const char* mdnsProto, uint16_t port) {
    setCpuFrequencyMhz(160);
    service = mdnsService;
    proto = mdnsProto;
    servicePort = port;
    hostname = transport::deviceName();

    std::string ssid = settings::ssid();
    WiFi.onEvent(onEvent);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname.c_str());
    WiFi.setAutoReconnect(true);
    if (ssid.empty()) {
        Serial.println("[wifi] no credentials, set them over BLE: WIFI <ssid>\\t<password>");
        ui::setAddress("no Wi-Fi config");
        return;
    }
    WiFi.begin(ssid.c_str(), settings::password().c_str());
    ui::setAddress(address());
    Serial.printf("[wifi] connecting to %s\n", ssid.c_str());
}

std::string address() {
    if (!connected) return "Wi-Fi ...";
    return WiFi.localIP().toString().c_str();
}

}  // namespace wifi_station
