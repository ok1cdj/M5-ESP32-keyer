// BLE transport: Nordic UART Service for the text protocol, standard Battery
// Service for the state of charge. Wi-Fi is never initialised in this mode.
#include <Arduino.h>
#include <NimBLEDevice.h>

#include "battery.h"
#include "core/commands.h"
#include "core/keyer_task.h"
#include "line_assembler.h"
#include "transport/transport.h"
#include "ui/ui.h"

namespace transport {

namespace {

const char* NUS_SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
const char* NUS_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // client writes
const char* NUS_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // we notify

constexpr uint16_t ADV_INTERVAL = 1600;  // 1600 * 0.625 ms = 1 s
constexpr uint8_t BATTERY_NOTIFY_STEP = 5;

class Ble : public Transport, NimBLEServerCallbacks, NimBLECharacteristicCallbacks {
public:
    void begin() override {
        setCpuFrequencyMhz(80);

        std::string name = deviceName();
        NimBLEDevice::init(name);
        NimBLEDevice::setMTU(247);

        server_ = NimBLEDevice::createServer();
        server_->setCallbacks(this, false);
        server_->advertiseOnDisconnect(true);

        NimBLEService* nus = server_->createService(NUS_SERVICE);
        tx_ = nus->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);
        NimBLECharacteristic* rx =
            nus->createCharacteristic(NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
        rx->setCallbacks(this);

        NimBLEService* bas = server_->createService(NimBLEUUID(uint16_t(0x180F)));
        level_ = bas->createCharacteristic(NimBLEUUID(uint16_t(0x2A19)),
                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
        batteryShown_ = battery::percent();
        level_->setValue(&batteryShown_, 1);

        // 128-bit UUID and name don't fit into 31 bytes together: name goes
        // into the scan response.
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        NimBLEAdvertisementData advData;
        advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
        advData.setCompleteServices(NimBLEUUID(NUS_SERVICE));
        NimBLEAdvertisementData scanData;
        scanData.setName(name);
        adv->setAdvertisementData(advData);
        adv->setScanResponseData(scanData);
        adv->setMinInterval(ADV_INTERVAL);
        adv->setMaxInterval(ADV_INTERVAL);
        adv->start();
        Serial.printf("[ble] advertising as %s\n", name.c_str());
    }

    void loop() override {
        uint8_t p = battery::percent();
        int diff = int(p) - int(batteryShown_);
        if (diff >= BATTERY_NOTIFY_STEP || diff <= -BATTERY_NOTIFY_STEP) {
            batteryShown_ = p;
            level_->setValue(&batteryShown_, 1);
            if (connected_) level_->notify();
        }
    }

    void notify(const char* line) override { send(line); }

private:
    // Lines go out terminated by "\n", split to the negotiated MTU.
    void send(const std::string& line) {
        if (!connected_) return;
        std::string data = line + "\n";
        uint16_t mtu = server_->getPeerMTU(connHandle_);
        size_t chunk = mtu > 3 ? mtu - 3 : 20;
        for (size_t i = 0; i < data.size(); i += chunk) {
            size_t n = std::min(chunk, data.size() - i);
            tx_->notify(reinterpret_cast<const uint8_t*>(data.data() + i), n, connHandle_);
        }
    }

    static void onLine(void* ctx, const char* line, size_t len) {
        Ble* self = static_cast<Ble*>(ctx);
        if (!line) {
            self->send("ERR length");
            return;
        }
        self->send(commands::execute(std::string(line, len), proto::Mode::Ble));
    }

    void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
        connHandle_ = info.getConnHandle();
        connected_ = true;
        assembler_.reset();
        ui::clientEvent();
        Serial.println("[ble] connected");
    }

    // Safety rule 4: a lost connection stops the transmission.
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
        connected_ = false;
        keyer_task::postStop();
        assembler_.reset();
        ui::clientEvent();
        Serial.printf("[ble] disconnected (reason %d)\n", reason);
    }

    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        NimBLEAttValue v = c->getValue();
        assembler_.feed(v.data(), v.length());
    }

    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* tx_ = nullptr;
    NimBLECharacteristic* level_ = nullptr;
    uint8_t batteryShown_ = 0;
    volatile bool connected_ = false;
    uint16_t connHandle_ = 0;
    proto::LineAssembler assembler_{onLine, this};
};

}  // namespace

Transport& ble() {
    static Ble instance;
    return instance;
}

}  // namespace transport
