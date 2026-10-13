// Host tests of the CI-V frame parser and the link state machine.
#include <unity.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "civ.h"

using namespace civ;

namespace {

std::vector<Frame> feedAll(FrameParser& p, std::initializer_list<uint8_t> bytes) {
    std::vector<Frame> out;
    for (uint8_t b : bytes) {
        if (p.feed(b)) out.push_back(p.frame());
    }
    return out;
}

Frame frame(uint8_t to, uint8_t from, uint8_t cmd, std::initializer_list<uint8_t> data) {
    Frame f;
    f.to = to;
    f.from = from;
    f.cmd = cmd;
    for (uint8_t b : data) f.data[f.len++] = b;
    return f;
}

// 14.025.000 Hz
const std::initializer_list<uint8_t> F14025 = {0x00, 0x50, 0x02, 0x14, 0x00};

// Runs poll() from `from` to `to` ms in 10 ms steps and collects the sends.
std::vector<Link::Action> run(Link& l, uint32_t from, uint32_t to) {
    std::vector<Link::Action> sends;
    for (uint32_t t = from; t <= to; t += 10) {
        Link::Action a = l.poll(t);
        if (a.send) sends.push_back(a);
    }
    return sends;
}

}  // namespace

void test_parse_broadcast_from_ic706() {
    FrameParser p;
    auto f = feedAll(p, {0xFE, 0xFE, 0x00, 0x58, 0x00, 0x00, 0x50, 0x02, 0x14, 0x00, 0xFD});
    TEST_ASSERT_EQUAL(1, (int)f.size());
    TEST_ASSERT_EQUAL_HEX8(0x00, f[0].to);
    TEST_ASSERT_EQUAL_HEX8(0x58, f[0].from);
    TEST_ASSERT_EQUAL_HEX8(0x00, f[0].cmd);
    TEST_ASSERT_EQUAL(5, f[0].len);
    uint64_t hz;
    TEST_ASSERT_TRUE(decodeFreq(f[0].data, f[0].len, hz));
    TEST_ASSERT_EQUAL_UINT32(14025000, (uint32_t)hz);
}

void test_parse_noise_jam_and_truncated() {
    FrameParser p;
    // Noise, a frame cut by the jammer, a truncated header, then a good one
    // with a triple preamble.
    auto f = feedAll(p, {0x12, 0x99, 0xFD,
                         0xFE, 0xFE, 0x00, 0x58, 0x00, 0x00, 0xFC, 0xFC, 0xFC, 0xFD,
                         0xFE, 0xFE, 0x00, 0xFD,
                         0xFE, 0xFE, 0xFE, 0xE0, 0x58, 0x04, 0x03, 0x01, 0xFD});
    TEST_ASSERT_EQUAL(1, (int)f.size());
    TEST_ASSERT_EQUAL_HEX8(0xE0, f[0].to);
    TEST_ASSERT_EQUAL_HEX8(0x04, f[0].cmd);
    TEST_ASSERT_EQUAL(2, f[0].len);
    TEST_ASSERT_EQUAL_HEX8(0x03, f[0].data[0]);
}

void test_parse_restart_and_overflow() {
    FrameParser p;
    std::vector<Frame> f;
    // A new preamble in the middle restarts the frame.
    f = feedAll(p, {0xFE, 0xFE, 0x00, 0x58, 0x00, 0x11, 0xFE, 0xFE, 0x00, 0x58, 0x01, 0x05, 0xFD});
    TEST_ASSERT_EQUAL(1, (int)f.size());
    TEST_ASSERT_EQUAL_HEX8(0x01, f[0].cmd);
    // Too much data is dropped.
    for (int i = 0; i < 5; i++) p.feed(i < 2 ? 0xFE : 0x10);
    for (size_t i = 0; i < Frame::MAX_DATA + 1; i++) p.feed(0x11);
    TEST_ASSERT_FALSE(p.feed(0xFD));
}

void test_decode_freq() {
    uint64_t hz;
    const uint8_t f160[] = {0x00, 0x00, 0x83, 0x01, 0x00};
    TEST_ASSERT_TRUE(decodeFreq(f160, 5, hz));
    TEST_ASSERT_EQUAL_UINT32(1830000, (uint32_t)hz);
    const uint8_t f2m[] = {0x00, 0x00, 0x30, 0x45, 0x01};
    TEST_ASSERT_TRUE(decodeFreq(f2m, 5, hz));
    TEST_ASSERT_EQUAL_UINT32(145300000, (uint32_t)hz);
    const uint8_t f70[] = {0x50, 0x12, 0x30, 0x32, 0x04};
    TEST_ASSERT_TRUE(decodeFreq(f70, 5, hz));
    TEST_ASSERT_EQUAL_UINT32(432301250, (uint32_t)hz);
    const uint8_t old4[] = {0x00, 0x50, 0x02, 0x07};
    TEST_ASSERT_TRUE(decodeFreq(old4, 4, hz));
    TEST_ASSERT_EQUAL_UINT32(7025000, (uint32_t)hz);
    const uint8_t bad[] = {0x0A, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(decodeFreq(bad, 5, hz));
    TEST_ASSERT_FALSE(decodeFreq(f2m, 3, hz));
}

void test_format() {
    TEST_ASSERT_EQUAL_STRING("FREQ 14025000 CW", formatFreq(true, 14025000, 0x03).c_str());
    TEST_ASSERT_EQUAL_STRING("FREQ 7000000 ?", formatFreq(true, 7000000, 0x17).c_str());
    TEST_ASSERT_EQUAL_STRING("FREQ NONE", formatFreq(false, 0, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("14 025.00 CW", formatDisplay(true, 14025000, 0x03).c_str());
    TEST_ASSERT_EQUAL_STRING("145 300.00 FM", formatDisplay(true, 145300000, 0x05).c_str());
    TEST_ASSERT_EQUAL_STRING("1 830.05 LSB", formatDisplay(true, 1830050, 0x00).c_str());
    TEST_ASSERT_EQUAL_STRING("", formatDisplay(false, 0, 0).c_str());
    Config c;
    State st;
    TEST_ASSERT_EQUAL_STRING("CIV AUTO - -", formatLink(c, st).c_str());
    st.locked = true;
    st.baud = 19200;
    st.rig = 0x58;
    TEST_ASSERT_EQUAL_STRING("CIV AUTO 19200 58", formatLink(c, st).c_str());
    c.mode = LinkMode::Fixed;
    st.rig = 0x0A;
    TEST_ASSERT_EQUAL_STRING("CIV FIXED 19200 0A", formatLink(c, st).c_str());
    c.mode = LinkMode::Off;
    TEST_ASSERT_EQUAL_STRING("CIV OFF", formatLink(c, st).c_str());
    uint8_t q[6];
    TEST_ASSERT_EQUAL(6, (int)buildQuery(0x58, 0xE0, 0x03, q));
    const uint8_t want[] = {0xFE, 0xFE, 0x58, 0xE0, 0x03, 0xFD};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, q, 6);
}

void test_tunnel_helpers() {
    Frame f = frame(0x00, 0x58, 0x00, F14025);
    uint8_t b[Frame::MAX_BYTES];
    size_t n = encodeFrame(f, b);
    TEST_ASSERT_EQUAL_STRING("FEFE00580000500214" "00FD", toHex(b, n).c_str());

    uint8_t t[Frame::MAX_BYTES];
    // kQSO builds its frames for the IC-705 (A4): readdressed to the rig found.
    size_t m = fromHex("fefea4e005005002140" "0fd", t, sizeof t);
    TEST_ASSERT_EQUAL(11, (int)m);
    TEST_ASSERT_TRUE(addressFrame(t, m, 0x58));
    TEST_ASSERT_EQUAL_STRING("FEFE58E00500500214" "00FD", toHex(t, m).c_str());

    TEST_ASSERT_EQUAL(0, (int)fromHex("FEF", t, sizeof t));
    TEST_ASSERT_EQUAL(0, (int)fromHex("FEXX", t, sizeof t));
    TEST_ASSERT_EQUAL(0, (int)fromHex(std::string(2 * (Frame::MAX_BYTES + 1), 'A'), t, sizeof t));
    m = fromHex("FEFEA4E003", t, sizeof t);  // no FD
    TEST_ASSERT_FALSE(addressFrame(t, m, 0x58));
    m = fromHex("FEFEA4E017FDFD", t, sizeof t);  // FD inside
    TEST_ASSERT_FALSE(addressFrame(t, m, 0x58));
    m = fromHex("FEFEA4E0FD", t, sizeof t);  // too short
    TEST_ASSERT_FALSE(addressFrame(t, m, 0x58));
}

// Binary CI-V client (OrbitDeck): one BLE write carries several frames, maybe
// after junk; each one comes out of the parser and back on the wire unchanged.
void test_binary_client_write() {
    FrameParser p;
    auto f = feedAll(p, {0x0D, 0x0A,                                    // junk
                         0xFE, 0xFE, 0x60, 0xE0, 0x07, 0xD0, 0xFD,      // select SUB
                         0xFE, 0xFE, 0x60, 0xE0, 0x05,                  // 435.700.000 Hz
                         0x00, 0x00, 0x70, 0x35, 0x04, 0xFD,
                         0xFE, 0xFE, 0x60, 0xE0, 0x1A, 0x07, 0x01, 0xFD});  // sat mode on
    TEST_ASSERT_EQUAL(3, (int)f.size());
    uint8_t b[Frame::MAX_BYTES];
    size_t n = encodeFrame(f[0], b);
    TEST_ASSERT_EQUAL_STRING("FEFE60E007D0FD", toHex(b, n).c_str());
    n = encodeFrame(f[1], b);
    TEST_ASSERT_EQUAL_STRING("FEFE60E0050000703504FD", toHex(b, n).c_str());
    n = encodeFrame(f[2], b);
    TEST_ASSERT_EQUAL_STRING("FEFE60E01A0701FD", toHex(b, n).c_str());
    TEST_ASSERT_TRUE(addressFrame(b, n, 0x60));
}

void test_link_locks_on_broadcast_and_queries() {
    Link l;
    l.configure(Config{}, 0);
    TEST_ASSERT_EQUAL_UINT32(19200, l.poll(0).baud);
    l.onFrame(frame(0x00, 0x58, 0x00, F14025), 50);
    TEST_ASSERT_TRUE(l.state().locked);
    TEST_ASSERT_EQUAL_HEX8(0x58, l.state().rig);
    // Not reported before the mode is known.
    TEST_ASSERT_FALSE(l.state().valid);
    // The current state is read at once: frequency, then mode.
    auto s = run(l, 60, 200);
    TEST_ASSERT_EQUAL(2, (int)s.size());
    TEST_ASSERT_EQUAL_HEX8(0x58, s[0].rig);
    TEST_ASSERT_EQUAL_HEX8(0x03, s[0].cmd);
    TEST_ASSERT_EQUAL_HEX8(0x04, s[1].cmd);
    uint32_t seq = l.state().seq;
    l.onFrame(frame(0xE0, 0x58, 0x04, {0x03, 0x01}), 210);
    TEST_ASSERT_TRUE(l.state().valid);
    TEST_ASSERT_EQUAL_UINT32(14025000, (uint32_t)l.state().hz);
    TEST_ASSERT_EQUAL_HEX8(0x03, l.state().mode);
    TEST_ASSERT_EQUAL_UINT32(seq + 1, l.state().seq);
    // The same mode again is not a change.
    l.onFrame(frame(0x00, 0x58, 0x01, {0x03, 0x01}), 300);
    TEST_ASSERT_EQUAL_UINT32(seq + 1, l.state().seq);
}

void test_link_ignores_own_echo_and_other_traffic() {
    Link l;
    l.configure(Config{}, 0);
    l.onFrame(frame(0x58, 0xE0, 0x03, {}), 10);   // our probe echo
    l.onFrame(frame(0x58, 0xE1, 0x03, {}), 20);   // another controller
    TEST_ASSERT_FALSE(l.state().locked);
    l.onFrame(frame(0xE0, 0x58, 0x03, F14025), 30);
    l.onFrame(frame(0xE0, 0x58, 0x04, {0x03}), 35);
    TEST_ASSERT_TRUE(l.state().locked);
    // A frame from another device does not change the frequency.
    l.onFrame(frame(0x00, 0x60, 0x00, {0x00, 0x00, 0x30, 0x45, 0x01}), 40);
    TEST_ASSERT_EQUAL_UINT32(14025000, (uint32_t)l.state().hz);
}

void test_link_probes_then_steps_the_rate() {
    Link l;
    l.configure(Config{}, 0);
    auto s = run(l, 0, Link::SEARCH_DWELL_MS - 10);
    TEST_ASSERT_EQUAL(5, (int)s.size());
    TEST_ASSERT_EQUAL_HEX8(0x58, s[0].rig);
    TEST_ASSERT_EQUAL_HEX8(0x94, s[4].rig);
    TEST_ASSERT_EQUAL_UINT32(9600, l.poll(Link::SEARCH_DWELL_MS).baud);
    TEST_ASSERT_EQUAL_UINT32(4800, l.poll(2 * Link::SEARCH_DWELL_MS).baud);
    TEST_ASSERT_EQUAL_UINT32(1200, l.poll(3 * Link::SEARCH_DWELL_MS).baud);
    TEST_ASSERT_EQUAL_UINT32(19200, l.poll(4 * Link::SEARCH_DWELL_MS).baud);
    // A reply to a probe at 9600 locks there.
    l.poll(5 * Link::SEARCH_DWELL_MS);
    l.onFrame(frame(0xE0, 0x4E, 0x03, F14025), 5 * Link::SEARCH_DWELL_MS + 300);
    TEST_ASSERT_TRUE(l.state().locked);
    TEST_ASSERT_EQUAL_UINT32(9600, l.state().baud);
    TEST_ASSERT_EQUAL_HEX8(0x4E, l.state().rig);
}

void test_link_polls_without_transceive_and_loses_the_rig() {
    Link l;
    l.configure(Config{}, 0);
    l.onFrame(frame(0xE0, 0x58, 0x03, F14025), 100);
    l.onFrame(frame(0xE0, 0x58, 0x04, {0x03}), 110);
    run(l, 120, 200);  // the initial reads
    // No broadcast heard: poll every second while the rig answers.
    uint32_t t = 200;
    for (int i = 0; i < 5; i++) {
        auto s = run(l, t + 10, t + 1000);
        TEST_ASSERT_EQUAL(2, (int)s.size());
        t += 1000;
        l.onFrame(frame(0xE0, 0x58, 0x03, F14025), t);
    }
    TEST_ASSERT_TRUE(l.state().valid);
    // Silence: lost after LOST_MS, back to searching.
    uint32_t seq = l.state().seq;
    run(l, t + 10, t + Link::LOST_MS + 20);
    TEST_ASSERT_FALSE(l.state().locked);
    TEST_ASSERT_FALSE(l.state().valid);
    TEST_ASSERT_EQUAL_UINT32(seq + 1, l.state().seq);
}

void test_link_stays_quiet_with_transceive() {
    Link l;
    l.configure(Config{}, 0);
    l.onFrame(frame(0x00, 0x58, 0x00, F14025), 100);
    l.onFrame(frame(0xE0, 0x58, 0x04, {0x03}), 150);
    run(l, 160, 300);
    // Broadcasts within QUIET_MS: no polling.
    TEST_ASSERT_EQUAL(0, (int)run(l, 310, 2990).size());
    l.onFrame(frame(0x00, 0x58, 0x00, F14025), 3000);
    TEST_ASSERT_EQUAL(0, (int)run(l, 3010, 7000).size());
    l.onFrame(frame(0x00, 0x58, 0x00, F14025), 7000);
    TEST_ASSERT_EQUAL(0, (int)run(l, 7010, 11000).size());
    TEST_ASSERT_TRUE(l.state().locked);
}

void test_link_fixed_and_off() {
    Link l;
    Config c;
    c.mode = LinkMode::Fixed;
    c.baud = 9600;
    c.rig = 0x60;
    l.configure(c, 0);
    TEST_ASSERT_TRUE(l.state().locked);
    TEST_ASSERT_EQUAL_UINT32(9600, l.poll(0).baud);
    // Frames from other addresses are ignored.
    l.onFrame(frame(0x00, 0x58, 0x00, F14025), 10);
    TEST_ASSERT_FALSE(l.state().valid);
    l.onFrame(frame(0x00, 0x60, 0x00, F14025), 20);
    l.onFrame(frame(0x00, 0x60, 0x01, {0x03}), 25);
    TEST_ASSERT_TRUE(l.state().valid);
    // Lost: reported, but the link keeps polling the same rig.
    run(l, 30, Link::LOST_MS + 100);
    TEST_ASSERT_FALSE(l.state().valid);
    TEST_ASSERT_TRUE(l.state().locked);
    auto s = run(l, Link::LOST_MS + 110, Link::LOST_MS + 2200);
    TEST_ASSERT_TRUE(s.size() >= 2);
    TEST_ASSERT_EQUAL_HEX8(0x60, s[0].rig);

    c.mode = LinkMode::Off;
    l.configure(c, 0);
    TEST_ASSERT_EQUAL_UINT32(0, l.poll(0).baud);
    TEST_ASSERT_EQUAL(0, (int)run(l, 0, 5000).size());
    l.onFrame(frame(0x00, 0x58, 0x00, F14025), 10);
    TEST_ASSERT_FALSE(l.state().locked);
}

void test_link_hint_first() {
    Link l;
    l.setHint(0x4D, 9600);
    l.configure(Config{}, 0);
    TEST_ASSERT_EQUAL_UINT32(9600, l.poll(0).baud);
    auto s = run(l, 0, Link::SEARCH_DWELL_MS - 10);
    TEST_ASSERT_EQUAL(6, (int)s.size());
    TEST_ASSERT_EQUAL_HEX8(0x4D, s[0].rig);
    TEST_ASSERT_EQUAL_HEX8(0x58, s[1].rig);
    // A hint among the defaults is not asked twice.
    l.setHint(0x58, 19200);
    l.configure(Config{}, 0);
    s = run(l, 0, Link::SEARCH_DWELL_MS - 10);
    TEST_ASSERT_EQUAL(5, (int)s.size());
    TEST_ASSERT_EQUAL_HEX8(0x58, s[0].rig);
    TEST_ASSERT_EQUAL_HEX8(0x4E, s[1].rig);
}

void test_link_clock_wrap() {
    Link l;
    uint32_t t0 = 0xFFFFFF00u;
    l.configure(Config{}, t0);
    l.onFrame(frame(0x00, 0x58, 0x00, F14025), t0 + 10);
    run(l, t0 + 20, t0 + 3000);  // crosses zero
    TEST_ASSERT_TRUE(l.state().locked);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_broadcast_from_ic706);
    RUN_TEST(test_parse_noise_jam_and_truncated);
    RUN_TEST(test_parse_restart_and_overflow);
    RUN_TEST(test_decode_freq);
    RUN_TEST(test_format);
    RUN_TEST(test_tunnel_helpers);
    RUN_TEST(test_binary_client_write);
    RUN_TEST(test_link_locks_on_broadcast_and_queries);
    RUN_TEST(test_link_ignores_own_echo_and_other_traffic);
    RUN_TEST(test_link_probes_then_steps_the_rate);
    RUN_TEST(test_link_polls_without_transceive_and_loses_the_rig);
    RUN_TEST(test_link_stays_quiet_with_transceive);
    RUN_TEST(test_link_fixed_and_off);
    RUN_TEST(test_link_hint_first);
    RUN_TEST(test_link_clock_wrap);
    return UNITY_END();
}
