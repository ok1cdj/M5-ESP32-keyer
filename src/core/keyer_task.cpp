#include "core/keyer_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

#include <atomic>

#include "fsm.h"
#include "core/key_output.h"
#include "settings.h"
#include "ui/ui.h"

// Test hook: -DWATCHDOG_TEST=1 keeps the key down after the first element so
// the keying watchdog must open it (hardware check of safety rule 3).
#ifndef WATCHDOG_TEST
#define WATCHDOG_TEST 0
#endif

namespace keyer_task {

namespace {

constexpr uint32_t KEY_WATCHDOG_US = 5 * 1000 * 1000;
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr size_t MAX_TEXT = 256;
constexpr UBaseType_t QUEUE_DEPTH = 8;
// esp_timer callbacks run on core 0 at priority 22. The keyer task runs on the
// same core below it, so a timer callback never overlaps with the task
// re-arming that timer.
constexpr BaseType_t TASK_CORE = 0;
constexpr UBaseType_t TASK_PRIORITY = 20;

enum class Kind : uint8_t { Core, ButtonEdge };

struct Item {
    Kind kind;
    keyer::EventType type;
    uint32_t gen;
    uint16_t value;
    uint16_t len;
    char text[MAX_TEXT];
};

QueueHandle_t queue;
esp_timer_handle_t elementTimer;
esp_timer_handle_t watchdogTimer;
volatile uint32_t elementGen;
volatile uint32_t watchdogGen;
NotifyFn notifier;
keyer::Core core;
std::atomic<uint32_t> packedSnapshot{0};
std::atomic<uint32_t> activity{0};
// Start of the text still to send, for the display. Written by the keyer task,
// read by loop() on the other core.
char pendingBuf[PENDING_MAX + 1];
portMUX_TYPE pendingLock = portMUX_INITIALIZER_UNLOCKED;

// Only the header of an Item is copied for events without text.
constexpr size_t HEADER = offsetof(Item, text);

bool IRAM_ATTR post(const Item& it, bool fromIsr) {
    if (fromIsr) {
        BaseType_t woken = pdFALSE;
        bool ok = xQueueSendFromISR(queue, &it, &woken) == pdTRUE;
        if (woken) portYIELD_FROM_ISR();
        return ok;
    }
    return xQueueSend(queue, &it, 0) == pdTRUE;
}

// Event items live on the stack; only the header fields are set.
Item coreItem(keyer::EventType type, uint32_t gen = 0, uint16_t value = 0) {
    Item it;
    memset(&it, 0, HEADER);
    it.kind = Kind::Core;
    it.type = type;
    it.gen = gen;
    it.value = value;
    return it;
}

void elementTimerCb(void*) {
    Item it = coreItem(keyer::EventType::Timer, elementGen);
    post(it, false);
}

void watchdogTimerCb(void*) {
    Item it = coreItem(keyer::EventType::Watchdog, watchdogGen);
    post(it, false);
}

// Prepared once in begin(): the ISR only hands it to the queue, nothing large
// lives on the interrupt stack.
DRAM_ATTR Item buttonItem;

void IRAM_ATTR buttonIsr() { post(buttonItem, true); }

void publishSnapshot() {
    bool sending = core.state() != keyer::State::Idle;
    uint32_t remaining = core.remaining();
    if (remaining > 0xFFFF) remaining = 0xFFFF;
    packedSnapshot.store((sending ? 1u : 0u) << 31 | remaining << 8 | core.wpm());
    char buf[PENDING_MAX];
    size_t n = core.pending(buf, PENDING_MAX);
    portENTER_CRITICAL(&pendingLock);
    memcpy(pendingBuf, buf, n);
    pendingBuf[n] = '\0';
    portEXIT_CRITICAL(&pendingLock);
}

void emit(const char* line) {
    Serial.printf("[keyer] %s\n", line);
    if (notifier) notifier(line);
}

void notify(keyer::Notice n, uint16_t value) {
    char buf[16];
    switch (n) {
        case keyer::Notice::Done: emit("DONE"); break;
        case keyer::Notice::Stopped: emit("STOPPED"); break;
        case keyer::Notice::ErrChar: emit("ERR char"); break;
        case keyer::Notice::ErrFull: emit("ERR full"); break;
        case keyer::Notice::ErrWatchdog: emit("ERR watchdog"); break;
        case keyer::Notice::Wpm:
            snprintf(buf, sizeof(buf), "WPM %u", value);
            emit(buf);
            break;
    }
}

void execute(const keyer::Output& out) {
    for (uint8_t i = 0; i < out.count; i++) {
        const keyer::Action& a = out.items[i];
        switch (a.type) {
            case keyer::ActionType::KeyOn:
                key_output::set(true);
                esp_timer_stop(watchdogTimer);
                watchdogGen = a.gen;
                esp_timer_start_once(watchdogTimer, KEY_WATCHDOG_US);
                break;
            case keyer::ActionType::KeyOff:
                key_output::set(false);
                esp_timer_stop(watchdogTimer);
                break;
            case keyer::ActionType::TimerStart:
                if (WATCHDOG_TEST && core.keyed()) break;
                esp_timer_stop(elementTimer);
                elementGen = a.gen;
                esp_timer_start_once(elementTimer, a.us);
                break;
            case keyer::ActionType::TimerCancel:
                esp_timer_stop(elementTimer);
                break;
            case keyer::ActionType::Notify:
                notify(a.notice, a.value);
                break;
            case keyer::ActionType::SaveWpm:
                settings::setWpm(uint8_t(a.value));
                break;
        }
    }
}

void step(const keyer::Event& ev) {
    keyer::State before = core.state();
    keyer::Pins pins;
    keyer::Output out = core.step(ev, pins);
    execute(out);
    publishSnapshot();
    if (core.state() != before) activity.fetch_add(1);
}

// Debounce in the task: the ISR only reports an edge, the level is read here.
void onButtonEdge() {
    static bool pressed = false;
    static uint32_t lastChange = 0;
    bool now = digitalRead(BTN_PIN) == LOW;
    uint32_t t = millis();
    if (now == pressed || t - lastChange < DEBOUNCE_MS) return;
    pressed = now;
    lastChange = t;
    keyer::Event ev;
    ev.type = pressed ? keyer::EventType::ButtonDown : keyer::EventType::ButtonUp;
    step(ev);
    activity.fetch_add(1);
    if (pressed) ui::buttonPressed();
}

void task(void*) {
    esp_task_wdt_add(nullptr);  // a stuck keyer task resets the chip -> output open
    Item it;
    for (;;) {
        esp_task_wdt_reset();
        if (xQueueReceive(queue, &it, pdMS_TO_TICKS(1000)) != pdTRUE) continue;
        if (it.kind == Kind::ButtonEdge) {
            onButtonEdge();
            continue;
        }
        keyer::Event ev;
        ev.type = it.type;
        ev.gen = it.gen;
        ev.value = it.value;
        if (it.type == keyer::EventType::Text) {
            ev.text = it.text;
            ev.len = it.len;
        }
        step(ev);
    }
}

}  // namespace

void begin(uint8_t wpm) {
    core.setWpm(wpm);
    publishSnapshot();
    queue = xQueueCreate(QUEUE_DEPTH, sizeof(Item));

    esp_timer_create_args_t args = {};
    args.dispatch_method = ESP_TIMER_TASK;
    args.callback = elementTimerCb;
    args.name = "element";
    esp_timer_create(&args, &elementTimer);
    args.callback = watchdogTimerCb;
    args.name = "key-wdt";
    esp_timer_create(&args, &watchdogTimer);

    xTaskCreatePinnedToCore(task, "keyer", 4096, nullptr, TASK_PRIORITY, nullptr, TASK_CORE);

    memset(&buttonItem, 0, HEADER);
    buttonItem.kind = Kind::ButtonEdge;
    pinMode(BTN_PIN, INPUT);  // external pull-up on all Atom boards
    attachInterrupt(BTN_PIN, buttonIsr, CHANGE);
}

void setNotifier(NotifyFn fn) { notifier = fn; }

bool postText(const char* text, size_t len) {
    Item it = coreItem(keyer::EventType::Text);
    if (len > MAX_TEXT) len = MAX_TEXT;
    memcpy(it.text, text, len);
    it.len = uint16_t(len);
    return post(it, false);
}

bool postStop() { return post(coreItem(keyer::EventType::Stop), false); }

bool postWpm(uint8_t wpm) { return post(coreItem(keyer::EventType::WpmSet, 0, wpm), false); }

Snapshot snapshot() {
    uint32_t p = packedSnapshot.load();
    return Snapshot{(p >> 31) != 0, uint16_t((p >> 8) & 0xFFFF), uint8_t(p & 0xFF)};
}

void pendingText(char* out) {
    portENTER_CRITICAL(&pendingLock);
    memcpy(out, pendingBuf, sizeof(pendingBuf));
    portEXIT_CRITICAL(&pendingLock);
}

uint32_t activityCounter() { return activity.load(); }

}  // namespace keyer_task
