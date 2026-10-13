// BLE transport: Nordic UART Service for the text protocol, standard Battery
// Service for the state of charge. Wi-Fi is never initialised in this mode.
// A connection whose first byte is FE carries raw CI-V frames instead of
// text (apps made for a BLE-to-CI-V adapter, such as OrbitDeck).
#include <Arduino.h>
#include <NimBLEDevice.h>

#include <mutex>

#include "battery.h"
#include "civ.h"
#include "core/commands.h"
#include "core/keyer_task.h"
#include "line_assembler.h"
#include "radio/radio.h"
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

    void notifyRaw(const uint8_t* b, size_t n) override {
        if (raw_) sendBytes(b, n);
    }

private:
    // Lines go out terminated by "\n"; a binary CI-V client gets none.
    void send(const std::string& line) {
        if (raw_) return;
        std::string data = line + "\n";
        sendBytes(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }

    // Split to the negotiated MTU. Called from the BLE host (responses), the
    // keyer task and loop() (FREQ, CI-V); the lock keeps the chunks of one
    // line or frame together.
    void sendBytes(const uint8_t* b, size_t len) {
        if (!connected_) return;
        std::lock_guard<std::mutex> guard(sendLock_);
        uint16_t mtu = server_->getPeerMTU(connHandle_);
        size_t chunk = mtu > 3 ? mtu - 3 : 20;
        for (size_t i = 0; i < len; i += chunk) {
            size_t n = std::min(chunk, len - i);
            tx_->notify(b + i, n, connHandle_);
        }
    }

    // Binary CI-V: every whole frame to the radio, readdressed like CIVTX.
    // There is no answer channel for errors; a dropped frame is repeated by
    // the app's next update, as with a plain adapter.
    void feedCiv(const uint8_t* b, size_t len) {
        for (size_t i = 0; i < len; i++) {
            if (!civParser_.feed(b[i])) continue;
            uint8_t f[civ::Frame::MAX_BYTES];
            size_t n = civ::encodeFrame(civParser_.frame(), f);
            if (radio::sendFrame(f, n) != radio::TxResult::Ok) {
                Serial.printf("[ble] CI-V frame dropped: %s\n", civ::toHex(f, n).c_str());
            }
        }
    }

    void resetConnection() {
        commands::resetSession();
        assembler_.reset();
        civParser_.reset();
        raw_ = false;
        started_ = false;
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
        resetConnection();
        ui::clientEvent();
        Serial.println("[ble] connected");
    }

    // Safety rule 4: a lost connection stops the transmission.
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
        connected_ = false;
        resetConnection();
        keyer_task::postStop();
        ui::clientEvent();
        Serial.printf("[ble] disconnected (reason %d)\n", reason);
    }

    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        NimBLEAttValue v = c->getValue();
        if (v.length() == 0) return;
        if (!started_) {
            // Text never starts with FE, so the first byte decides for the
            // whole connection.
            started_ = true;
            if (v.data()[0] == civ::PREAMBLE) {
                raw_ = true;
                commands::setRawCiv(true);
                Serial.println("[ble] binary CI-V client");
            }
        }
        if (raw_) feedCiv(v.data(), v.length());
        else assembler_.feed(v.data(), v.length());
    }

    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* tx_ = nullptr;
    NimBLECharacteristic* level_ = nullptr;
    uint8_t batteryShown_ = 0;
    volatile bool connected_ = false;
    bool started_ = false;  // the connection has written something
    volatile bool raw_ = false;  // binary CI-V connection
    uint16_t connHandle_ = 0;
    std::mutex sendLock_;
    proto::LineAssembler assembler_{onLine, this};
    civ::FrameParser civParser_;
};

}  // namespace

Transport& ble() {
    static Ble instance;
    return instance;
}

}  // namespace transport
