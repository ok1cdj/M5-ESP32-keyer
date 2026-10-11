#include "radio/radio.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <hal/gpio_ll.h>
#include <freertos/queue.h>

#include <atomic>

#include "settings.h"

namespace radio {

namespace {

constexpr uart_port_t PORT = UART_NUM_1;
constexpr gpio_num_t PIN = gpio_num_t(CIV_PIN);
constexpr int RX_BUFFER = 256;
constexpr uint32_t TICK_MS = 5;
// The bus counts as free after this long without a byte (about 2 bytes at
// 4800 Bd); waiting for it avoids most collisions.
constexpr uint32_t IDLE_MS = 5;
constexpr uint32_t IDLE_WAIT_MS = 50;
// After a frame of ours, the next one waits for the rig's answer (or this
// long): the IC-706 drops a command that arrives before it has answered the
// previous one.
constexpr uint32_t REPLY_WAIT_MS = 100;

civ::Link link;
civ::FrameParser parser;
uint32_t baud = 0;  // 0 = UART not installed
uint32_t lastByte = 0;
bool awaitingReply = false;
civ::Link::Action own;  // our next query, sent when the bus is free
uint32_t sentAt = 0;

struct RawFrame {
    uint8_t n;
    uint8_t b[civ::Frame::MAX_BYTES];
};
constexpr UBaseType_t TX_QUEUE = 8;
constexpr UBaseType_t TAP_QUEUE = 24;  // a VFO knob sends a frame every few ms
QueueHandle_t txQueue = nullptr;
QueueHandle_t tapQueue = nullptr;
std::atomic<bool> tap{false};

// Shared with the other tasks.
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
Snapshot shared;
civ::Config pendingCfg;
bool cfgPending = false;

void uartOff() {
    uart_driver_delete(PORT);
    gpio_reset_pin(PIN);  // GPIO function, output off, pull-up on
    gpio_input_enable(PIN);
    baud = 0;
}

void uartOn(uint32_t rate) {
    if (baud == 0) {
        uart_config_t c = {};
        c.baud_rate = int(rate);
        c.data_bits = UART_DATA_8_BITS;
        c.parity = UART_PARITY_DISABLE;
        c.stop_bits = UART_STOP_BITS_1;
        c.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        c.source_clk = UART_SCLK_DEFAULT;
        uart_driver_install(PORT, RX_BUFFER, 0, 0, nullptr, 0);
        uart_param_config(PORT, &c);
        uart_set_pin(PORT, PIN, PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        // One wire: the TX signal may only pull the line low, the internal
        // pull-up brings it back (the Grove has no 3.3 V for an external one,
        // README). RX keeps reading the same pin, so everything we send comes
        // back as an echo. Only the pad driver changes here: gpio_set_direction()
        // would reconnect the pin to the plain GPIO output (low) and hold the bus.
        gpio_ll_od_enable(GPIO_LL_GET_HW(GPIO_PORT_0), PIN);
        gpio_pullup_en(PIN);
    } else {
        uart_set_baudrate(PORT, rate);
    }
    uart_flush_input(PORT);
    parser.reset();
    baud = rate;
}

#ifdef CIV_DEBUG
// Bench diagnostics: once a second the rate, bytes heard, frames parsed, the
// pin level and the first bytes. Build with -DCIV_DEBUG.
uint32_t dbgBytes = 0, dbgFrames = 0, dbgAt = 0;
uint8_t dbgFirst[12];
size_t dbgFirstLen = 0;

void debugLog(uint32_t now) {
    if (uint32_t(now - dbgAt) < 1000) return;
    dbgAt = now;
    std::string hex = civ::toHex(dbgFirst, dbgFirstLen);
    Serial.printf("[civ] %u Bd, %u bytes, %u frames, pin %d, %s\n", unsigned(baud),
                  unsigned(dbgBytes), unsigned(dbgFrames), gpio_get_level(PIN), hex.c_str());
    dbgBytes = dbgFrames = 0;
    dbgFirstLen = 0;
}
#endif

void receive(uint32_t now) {
    uint8_t buf[64];
    int n;
    while ((n = uart_read_bytes(PORT, buf, sizeof buf, 0)) > 0) {
        lastByte = now;
#ifdef CIV_DEBUG
        dbgBytes += n;
        for (int i = 0; i < n && dbgFirstLen < sizeof dbgFirst; i++) dbgFirst[dbgFirstLen++] = buf[i];
#endif
        for (int i = 0; i < n; i++) {
            if (!parser.feed(buf[i])) continue;
            if (parser.frame().from != civ::CONTROLLER) awaitingReply = false;
#ifdef CIV_DEBUG
            dbgFrames++;
#endif
            const civ::Frame& f = parser.frame();
            link.onFrame(f, now);
            const civ::State& st = link.state();
            if (tap && st.locked && f.from == st.rig) {
                RawFrame r;
                r.n = uint8_t(civ::encodeFrame(f, r.b));
                xQueueSend(tapQueue, &r, 0);  // full: the client is too slow, drop
            }
        }
    }
}

// Waits for a quiet bus, then sends. False when the bus stays busy.
bool sendBytes(const uint8_t* b, size_t n) {
    uint32_t start = millis();
    while (uint32_t(millis() - lastByte) < IDLE_MS) {
        if (uint32_t(millis() - start) >= IDLE_WAIT_MS) return false;
        vTaskDelay(1);
        receive(millis());
    }
    uart_write_bytes(PORT, b, n);
    awaitingReply = true;
    sentAt = millis();
    return true;
}

bool canSend() {
    return baud != 0 && (!awaitingReply || uint32_t(millis() - sentAt) >= REPLY_WAIT_MS);
}

void send(uint8_t rig, uint8_t cmd) {
    uint8_t q[6];
    size_t n = civ::buildQuery(rig, civ::CONTROLLER, cmd, q);
    sendBytes(q, n);  // a lost query is repeated by the next poll
}

void log(const civ::State& before, const civ::State& now) {
    if (now.locked && (!before.locked || now.rig != before.rig || now.baud != before.baud)) {
        Serial.printf("[civ] rig %02X at %u Bd\n", now.rig, unsigned(now.baud));
        if (link.config().mode == civ::LinkMode::Auto) {
            // Asked first after the next start (Link::setHint).
            link.setHint(now.rig, now.baud);
            settings::setCivLast(now.rig, now.baud);
        }
    } else if (!now.locked && before.locked) {
        Serial.println("[civ] rig lost, searching");
    }
}

void task(void*) {
    for (;;) {
        uint32_t now = millis();
        portENTER_CRITICAL(&lock);
        bool apply = cfgPending;
        civ::Config cfg = pendingCfg;
        cfgPending = false;
        portEXIT_CRITICAL(&lock);
        if (apply) link.configure(cfg, now);

        civ::State before = link.state();
        if (baud != 0) receive(now);
        civ::Link::Action a = link.poll(now);
        if (a.baud != baud) {
            if (a.baud == 0) uartOff();
            else uartOn(a.baud);
            own.send = false;
            awaitingReply = false;
        }
        // One frame on the bus at a time; a client's frame goes before our
        // own query, which the Link simply repeats later.
        if (a.send) own = a;  // waits here while the bus is busy
        RawFrame r;
        if (canSend() && xQueueReceive(txQueue, &r, 0) == pdTRUE) {
            // Rig lost since it was queued: dropped, the client times out.
            if (link.state().locked) sendBytes(r.b, r.n);
        } else if (own.send && canSend()) {
            send(own.rig, own.cmd);
            own.send = false;
        }
        log(before, link.state());
#ifdef CIV_DEBUG
        debugLog(now);
#endif

        portENTER_CRITICAL(&lock);
        shared.cfg = link.config();
        shared.state = link.state();
        portEXIT_CRITICAL(&lock);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

}  // namespace

void begin(const civ::Config& cfg) {
    txQueue = xQueueCreate(TX_QUEUE, sizeof(RawFrame));
    tapQueue = xQueueCreate(TAP_QUEUE, sizeof(RawFrame));
    uint8_t rig;
    uint32_t rate;
    settings::civLast(rig, rate);
    link.setHint(rig, rate);
    configure(cfg);
    // Below the keyer task (20, core 0), so the keying timing is not affected.
    xTaskCreatePinnedToCore(task, "civ", 3072, nullptr, 5, nullptr, 1);
}

void configure(const civ::Config& cfg) {
    portENTER_CRITICAL(&lock);
    pendingCfg = cfg;
    cfgPending = true;
    portEXIT_CRITICAL(&lock);
}

TxResult sendFrame(const uint8_t* bytes, size_t n) {
    civ::State st = snapshot().state;
    if (!st.locked) return TxResult::NoRig;
    RawFrame r;
    if (n > sizeof r.b) return TxResult::Busy;
    memcpy(r.b, bytes, n);
    r.n = uint8_t(n);
    if (!civ::addressFrame(r.b, n, st.rig)) return TxResult::NoRig;
    return xQueueSend(txQueue, &r, 0) == pdTRUE ? TxResult::Ok : TxResult::Busy;
}

void setTap(bool on) {
    tap = on;
    if (!on && tapQueue) xQueueReset(tapQueue);
}

bool takeTapped(uint8_t* out, size_t& n) {
    RawFrame r;
    if (!tapQueue || xQueueReceive(tapQueue, &r, 0) != pdTRUE) return false;
    memcpy(out, r.b, r.n);
    n = r.n;
    return true;
}

Snapshot snapshot() {
    portENTER_CRITICAL(&lock);
    Snapshot s = shared;
    portEXIT_CRITICAL(&lock);
    return s;
}

}  // namespace radio
