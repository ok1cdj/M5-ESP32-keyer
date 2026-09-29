// Keyer core: event-driven state machine. Pure C++, no Arduino dependency.
//
// step(event, pins) returns the actions a thin hardware layer must perform
// (key the output, arm a timer, notify the client, ...). The core is the only
// owner of the key output; every source (text today, paddles later) goes
// through it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "element_generator.h"
#include "text_queue.h"

namespace keyer {

enum class State : uint8_t {
    Idle,
    TextOn,   // key down, element of a text character
    TextOff,  // key up, gap between elements / characters / words
    // Reserved for the paddle phase, not reachable yet.
    BreakGap,
    PaddleOn,
    PaddleOff,
    Speed,
};

enum class EventType : uint8_t {
    Text,        // text + len
    Stop,        // command STOP, BLE disconnect
    ButtonDown,  // = STOP in this phase
    ButtonUp,    // unused until the paddle phase
    Timer,       // gen = timer generation from TimerStart
    Watchdog,    // gen = key generation from KeyOn
    WpmSet,      // value = WPM (already range checked)
    Paddle,      // ignored until the paddle phase
};

struct Event {
    EventType type;
    uint32_t gen = 0;
    uint16_t value = 0;
    const char* text = nullptr;
    size_t len = 0;
};

struct Pins {
    bool dot = false;
    bool dash = false;
};

enum class ActionType : uint8_t {
    KeyOn,        // gen = key generation, arm the key watchdog with it
    KeyOff,
    TimerStart,   // us, gen
    TimerCancel,
    Notify,       // notice, value
    SaveWpm,      // value
};

enum class Notice : uint8_t {
    Done,
    Stopped,
    ErrChar,
    ErrFull,
    ErrWatchdog,
    Wpm,
};

struct Action {
    ActionType type;
    uint32_t us = 0;
    uint32_t gen = 0;
    Notice notice = Notice::Done;
    uint16_t value = 0;
};

struct Output {
    static constexpr uint8_t MAX = 8;
    Action items[MAX];
    uint8_t count = 0;

    void push(const Action& a) {
        if (count < MAX) items[count++] = a;
    }
};

class Core {
public:
    Output step(const Event& ev, const Pins& pins);

    State state() const { return state_; }
    bool keyed() const { return keyed_; }
    uint8_t wpm() const { return gen_.wpm(); }
    void setWpm(uint8_t wpm) { gen_.setWpm(wpm); }

    // Characters not yet fully sent (the one being sent included).
    size_t remaining() const { return queue_.size() + (current_ ? 1 : 0); }

private:
    void onText(const Event& ev, Output& out);
    void onTimer(Output& out);
    void advance(Output& out);
    void keyOn(Output& out, uint32_t us);
    void startTimer(Output& out, uint32_t us);
    void abort(Output& out);
    void notify(Output& out, Notice n, uint16_t value = 0);

    State state_ = State::Idle;
    ElementGenerator gen_;
    TextQueue queue_;
    bool keyed_ = false;
    bool current_ = false;    // a character is being sent
    bool afterChar_ = false;  // last gap was a character gap (3 dits)
    uint32_t timerGen_ = 0;
    uint32_t keyGen_ = 0;
};

}  // namespace keyer
