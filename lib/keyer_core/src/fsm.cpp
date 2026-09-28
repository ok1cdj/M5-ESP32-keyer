#include "fsm.h"

namespace keyer {

namespace {

bool pushToQueue(void* ctx, uint8_t item) { return static_cast<TextQueue*>(ctx)->push(item); }

}  // namespace

Output Core::step(const Event& ev, const Pins& pins) {
    (void)pins;  // read by the paddle states in the next phase
    Output out;
    switch (ev.type) {
        case EventType::Text:
            onText(ev, out);
            break;

        case EventType::Stop:
        case EventType::ButtonDown: {
            bool wasActive = state_ != State::Idle;
            abort(out);
            if (wasActive) notify(out, Notice::Stopped);
            break;
        }

        case EventType::ButtonUp:
        case EventType::Paddle:
            break;

        case EventType::Timer:
            // A timer event queued before STOP carries an old generation.
            if (state_ != State::Idle && ev.gen == timerGen_) onTimer(out);
            break;

        case EventType::Watchdog:
            if (keyed_ && ev.gen == keyGen_) {
                abort(out);
                notify(out, Notice::ErrWatchdog);
            }
            break;

        case EventType::WpmSet:
            gen_.setWpm(uint8_t(ev.value));
            out.push({ActionType::SaveWpm, 0, 0, Notice::Done, gen_.wpm()});
            break;
    }
    return out;
}

void Core::onText(const Event& ev, Output& out) {
    NormalizeResult r = normalize(ev.text, ev.len, pushToQueue, &queue_);
    if (r.badChar) notify(out, Notice::ErrChar);
    if (r.overflow) notify(out, Notice::ErrFull);
    if (state_ == State::Idle && !queue_.empty()) {
        afterChar_ = false;
        advance(out);
    }
}

void Core::onTimer(Output& out) {
    if (state_ == State::TextOn) {
        out.push({ActionType::KeyOff});
        keyed_ = false;
        uint32_t gap;
        if (gen_.hasElement() || gen_.joined()) {
            gap = gen_.ditUs();  // element gap, also between prosign letters
            afterChar_ = false;
        } else {
            gap = 3 * gen_.ditUs();  // character gap
            afterChar_ = true;
        }
        if (!gen_.hasElement()) current_ = false;
        state_ = State::TextOff;
        startTimer(out, gap);
    } else if (state_ == State::TextOff) {
        advance(out);
    }
}

// Starts the next element, the next character or a word gap; ends in IDLE
// when the queue is empty.
void Core::advance(Output& out) {
    if (gen_.hasElement()) {
        keyOn(out, gen_.nextElementUs());
        return;
    }
    uint8_t item;
    while (queue_.pop(item)) {
        if (item == ' ') {
            // Word gap is 7 dits; 3 of them were already spent after the character.
            uint32_t units = afterChar_ ? 4 : 7;
            afterChar_ = false;
            state_ = State::TextOff;
            startTimer(out, units * gen_.ditUs());
            return;
        }
        if (gen_.load(item)) {
            current_ = true;
            keyOn(out, gen_.nextElementUs());
            return;
        }
    }
    gen_.clear();
    current_ = false;
    state_ = State::Idle;
    notify(out, Notice::Done);
}

void Core::keyOn(Output& out, uint32_t us) {
    keyGen_++;
    out.push({ActionType::KeyOn, 0, keyGen_});
    keyed_ = true;
    state_ = State::TextOn;
    startTimer(out, us);
}

void Core::startTimer(Output& out, uint32_t us) {
    timerGen_++;
    out.push({ActionType::TimerStart, us, timerGen_});
}

// Output off, timer cancelled, queue dropped, back to IDLE.
void Core::abort(Output& out) {
    if (keyed_) out.push({ActionType::KeyOff});
    if (state_ != State::Idle) out.push({ActionType::TimerCancel});
    timerGen_++;  // invalidate a timer event that is already queued
    keyed_ = false;
    queue_.clear();
    gen_.clear();
    current_ = false;
    afterChar_ = false;
    state_ = State::Idle;
}

void Core::notify(Output& out, Notice n, uint16_t value) {
    out.push({ActionType::Notify, 0, 0, n, value});
}

}  // namespace keyer
