// Host tests of the keyer state machine (text part).
#include <unity.h>

#include <string.h>

#include <string>
#include <vector>

#include "fsm.h"

using namespace keyer;

namespace {

// Runs the core like the hardware layer does, with a simulated clock.
struct Sim {
    Core core;
    bool key = false;
    bool timerArmed = false;
    uint32_t timerUs = 0;
    uint32_t timerGen = 0;
    uint32_t keyGen = 0;
    std::vector<Notice> notices;
    std::vector<uint16_t> savedWpm;
    std::string wave;  // one char per dit unit: '=' key down, '_' key up

    void apply(const Output& out) {
        for (uint8_t i = 0; i < out.count; i++) {
            const Action& a = out.items[i];
            switch (a.type) {
                case ActionType::KeyOn:
                    key = true;
                    keyGen = a.gen;
                    break;
                case ActionType::KeyOff: key = false; break;
                case ActionType::TimerStart:
                    timerArmed = true;
                    timerUs = a.us;
                    timerGen = a.gen;
                    break;
                case ActionType::TimerCancel: timerArmed = false; break;
                case ActionType::Notify: notices.push_back(a.notice); break;
                case ActionType::SaveWpm: savedWpm.push_back(a.value); break;
            }
        }
    }

    void send(EventType t, uint32_t gen = 0, uint16_t value = 0) {
        Event e;
        e.type = t;
        e.gen = gen;
        e.value = value;
        apply(core.step(e, Pins{}));
    }

    void text(const char* s) {
        Event e;
        e.type = EventType::Text;
        e.text = s;
        e.len = strlen(s);
        apply(core.step(e, Pins{}));
    }

    // Fires the armed timer once and records the waveform of its interval.
    bool tick() {
        if (!timerArmed) return false;
        uint32_t units = timerUs / (1200000u / core.wpm());
        wave.append(units, key ? '=' : '_');
        timerArmed = false;
        send(EventType::Timer, timerGen);
        return true;
    }

    void runToEnd() {
        for (int guard = 0; guard < 100000 && tick(); guard++) {
        }
    }

    bool hasNotice(Notice n) const {
        for (Notice x : notices)
            if (x == n) return true;
        return false;
    }

    int count(Notice n) const {
        int c = 0;
        for (Notice x : notices)
            if (x == n) c++;
        return c;
    }
};

// Waveform of a message built from patterns, for readable expectations.
std::string expect(const std::vector<std::string>& words) {
    std::string w;
    for (size_t wi = 0; wi < words.size(); wi++) {
        const std::string& word = words[wi];
        for (size_t ci = 0; ci < word.size(); ci++) {
            const char* p = morsePattern(word[ci]);
            for (const char* e = p; *e; e++) {
                w += *e == '-' ? "===" : "=";
                if (e[1]) w += "_";
            }
            if (ci + 1 < word.size()) w += "___";
        }
        w += wi + 1 < words.size() ? "_______" : "___";
    }
    return w;
}

}  // namespace

void test_cq_message_timing() {
    Sim s;
    s.text("CQ CQ DE OK1CDJ");
    s.runToEnd();
    TEST_ASSERT_EQUAL_STRING(expect({"CQ", "CQ", "DE", "OK1CDJ"}).c_str(), s.wave.c_str());
    TEST_ASSERT_FALSE(s.key);
    TEST_ASSERT_TRUE(s.core.state() == State::Idle);
    TEST_ASSERT_EQUAL(1, s.count(Notice::Done));
    TEST_ASSERT_FALSE(s.hasNotice(Notice::ErrChar));
}

void test_dit_length_follows_wpm() {
    Sim s;
    s.core.setWpm(25);
    s.text("E");
    TEST_ASSERT_TRUE(s.key);
    TEST_ASSERT_EQUAL_UINT32(48000, s.timerUs);  // 1200 / 25 ms
}

void test_lowercase_is_uppercased() {
    Sim a, b;
    a.text("cq de ok1cdj");
    b.text("CQ DE OK1CDJ");
    a.runToEnd();
    b.runToEnd();
    TEST_ASSERT_EQUAL_STRING(b.wave.c_str(), a.wave.c_str());
}

void test_prosign_has_no_character_gap() {
    Sim s;
    s.text("<AR>");
    s.runToEnd();
    // .-.-. : A and R joined by an element gap
    TEST_ASSERT_EQUAL_STRING("=_===_=_===_=___", s.wave.c_str());
}

void test_prosign_inside_text() {
    Sim s;
    s.text("K <SK>");
    s.runToEnd();
    TEST_ASSERT_EQUAL_STRING("===_=_===_______=_=_=_===_=_===___", s.wave.c_str());
}

void test_unsupported_char_reported_once() {
    Sim s;
    s.text("E#E@E");
    s.runToEnd();
    TEST_ASSERT_EQUAL(1, s.count(Notice::ErrChar));
    TEST_ASSERT_EQUAL_STRING("=___=___=___", s.wave.c_str());
}

void test_text_appended_while_sending() {
    Sim s;
    s.text("E");
    s.text("T");
    s.runToEnd();
    TEST_ASSERT_EQUAL_STRING("=___===___", s.wave.c_str());
    TEST_ASSERT_EQUAL(1, s.count(Notice::Done));
}

void test_remaining_counts_characters() {
    Sim s;
    s.text("AB C");
    TEST_ASSERT_EQUAL(4, s.core.remaining());  // A (being sent), B, space, C
    s.runToEnd();
    TEST_ASSERT_EQUAL(0, s.core.remaining());
}

void test_stop_in_every_state() {
    const EventType stops[] = {EventType::Stop, EventType::ButtonDown};
    for (EventType stop : stops) {
        // IDLE: nothing to interrupt, no STOPPED
        {
            Sim s;
            s.send(stop);
            TEST_ASSERT_FALSE(s.key);
            TEST_ASSERT_FALSE(s.hasNotice(Notice::Stopped));
        }
        // TEXT_ON: in the middle of a dah
        {
            Sim s;
            s.text("TTT");
            TEST_ASSERT_TRUE(s.core.state() == State::TextOn);
            s.send(stop);
            TEST_ASSERT_FALSE(s.key);
            TEST_ASSERT_FALSE(s.timerArmed);
            TEST_ASSERT_TRUE(s.core.state() == State::Idle);
            TEST_ASSERT_EQUAL(0, s.core.remaining());
            TEST_ASSERT_TRUE(s.hasNotice(Notice::Stopped));
        }
        // TEXT_OFF: in a gap
        {
            Sim s;
            s.text("TTT");
            s.tick();
            TEST_ASSERT_TRUE(s.core.state() == State::TextOff);
            s.send(stop);
            TEST_ASSERT_FALSE(s.key);
            TEST_ASSERT_TRUE(s.core.state() == State::Idle);
            TEST_ASSERT_EQUAL(0, s.core.remaining());
            TEST_ASSERT_TRUE(s.hasNotice(Notice::Stopped));
        }
    }
}

void test_stale_timer_after_stop_does_not_key() {
    Sim s;
    s.text("TTT");
    uint32_t staleGen = s.timerGen;
    s.send(EventType::Stop);
    s.send(EventType::Timer, staleGen);  // was already in the queue
    TEST_ASSERT_FALSE(s.key);
    TEST_ASSERT_TRUE(s.core.state() == State::Idle);
    // new text after a stale timer starts cleanly
    s.text("E");
    uint32_t gen = s.timerGen;
    s.send(EventType::Timer, staleGen);
    TEST_ASSERT_TRUE(s.key);
    TEST_ASSERT_EQUAL_UINT32(gen, s.timerGen);
}

void test_watchdog_opens_output() {
    Sim s;
    s.text("TTT");
    TEST_ASSERT_TRUE(s.key);
    s.send(EventType::Watchdog, s.keyGen);
    TEST_ASSERT_FALSE(s.key);
    TEST_ASSERT_FALSE(s.timerArmed);
    TEST_ASSERT_TRUE(s.core.state() == State::Idle);
    TEST_ASSERT_EQUAL(0, s.core.remaining());
    TEST_ASSERT_TRUE(s.hasNotice(Notice::ErrWatchdog));
}

void test_stale_watchdog_is_ignored() {
    Sim s;
    s.text("TT");
    uint32_t oldKey = s.keyGen;
    s.tick();  // first T off
    s.tick();  // second T on, new key generation
    TEST_ASSERT_TRUE(s.key);
    s.send(EventType::Watchdog, oldKey);
    TEST_ASSERT_TRUE(s.key);
    TEST_ASSERT_FALSE(s.hasNotice(Notice::ErrWatchdog));
    // watchdog while the key is up is ignored too
    s.tick();
    TEST_ASSERT_FALSE(s.key);
    s.send(EventType::Watchdog, s.keyGen);
    TEST_ASSERT_FALSE(s.hasNotice(Notice::ErrWatchdog));
}

void test_wpm_set_saves_and_applies() {
    Sim s;
    s.send(EventType::WpmSet, 0, 30);
    TEST_ASSERT_EQUAL(30, s.core.wpm());
    TEST_ASSERT_EQUAL(1, (int)s.savedWpm.size());
    TEST_ASSERT_EQUAL(30, s.savedWpm[0]);
    s.text("E");
    TEST_ASSERT_EQUAL_UINT32(40000, s.timerUs);
}

void test_queue_overflow_reported() {
    Sim s;
    std::string big(TextQueue::CAPACITY + 10, 'E');
    s.text(big.c_str());
    TEST_ASSERT_TRUE(s.hasNotice(Notice::ErrFull));
    TEST_ASSERT_EQUAL(TextQueue::CAPACITY, s.core.remaining());
}

void test_button_up_and_paddle_do_nothing_yet() {
    Sim s;
    s.text("T");
    s.send(EventType::ButtonUp);
    s.send(EventType::Paddle);
    TEST_ASSERT_TRUE(s.key);
    TEST_ASSERT_TRUE(s.core.state() == State::TextOn);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cq_message_timing);
    RUN_TEST(test_dit_length_follows_wpm);
    RUN_TEST(test_lowercase_is_uppercased);
    RUN_TEST(test_prosign_has_no_character_gap);
    RUN_TEST(test_prosign_inside_text);
    RUN_TEST(test_unsupported_char_reported_once);
    RUN_TEST(test_text_appended_while_sending);
    RUN_TEST(test_remaining_counts_characters);
    RUN_TEST(test_stop_in_every_state);
    RUN_TEST(test_stale_timer_after_stop_does_not_key);
    RUN_TEST(test_watchdog_opens_output);
    RUN_TEST(test_stale_watchdog_is_ignored);
    RUN_TEST(test_wpm_set_saves_and_applies);
    RUN_TEST(test_queue_overflow_reported);
    RUN_TEST(test_button_up_and_paddle_do_nothing_yet);
    return UNITY_END();
}
