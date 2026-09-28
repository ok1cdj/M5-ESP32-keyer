// Host tests of line assembly and command parsing.
#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "command.h"
#include "line_assembler.h"

using namespace proto;

namespace {

struct Lines {
    std::vector<std::string> lines;  // "<ERR length>" for a dropped line
    static void fn(void* ctx, const char* line, size_t len) {
        Lines* l = static_cast<Lines*>(ctx);
        l->lines.push_back(line ? std::string(line, len) : std::string("<ERR length>"));
    }
};

void feedChunks(LineAssembler& a, const std::string& data, size_t chunk) {
    for (size_t i = 0; i < data.size(); i += chunk) {
        size_t n = std::min(chunk, data.size() - i);
        a.feed(reinterpret_cast<const uint8_t*>(data.data() + i), n);
    }
}

}  // namespace

void test_line_split_over_20_byte_writes() {
    Lines l;
    LineAssembler a(Lines::fn, &l);
    std::string cmd = "SEND CQ CQ DE OK1CDJ OK1CDJ PSE K\n";
    feedChunks(a, cmd, 20);
    TEST_ASSERT_EQUAL(1, (int)l.lines.size());
    TEST_ASSERT_EQUAL_STRING("SEND CQ CQ DE OK1CDJ OK1CDJ PSE K", l.lines[0].c_str());
}

void test_multiple_lines_in_one_write_and_crlf() {
    Lines l;
    LineAssembler a(Lines::fn, &l);
    feedChunks(a, "WPM 22\r\nSTATUS\nST", 100);
    feedChunks(a, "OP\n", 100);
    TEST_ASSERT_EQUAL(3, (int)l.lines.size());
    TEST_ASSERT_EQUAL_STRING("WPM 22", l.lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING("STATUS", l.lines[1].c_str());
    TEST_ASSERT_EQUAL_STRING("STOP", l.lines[2].c_str());
}

void test_line_length_limit() {
    Lines l;
    LineAssembler a(Lines::fn, &l);
    std::string ok(LineAssembler::MAX_LINE, 'E');
    std::string okCr = ok + "\r";
    std::string tooLong(LineAssembler::MAX_LINE + 1, 'E');
    std::string wayTooLong(1000, 'E');
    feedChunks(a, ok + "\n", 20);
    feedChunks(a, okCr + "\n", 20);
    feedChunks(a, tooLong + "\n", 20);
    feedChunks(a, wayTooLong + "\n", 20);
    feedChunks(a, "VER\n", 20);
    TEST_ASSERT_EQUAL(5, (int)l.lines.size());
    TEST_ASSERT_EQUAL(LineAssembler::MAX_LINE, (int)l.lines[0].size());
    TEST_ASSERT_EQUAL(LineAssembler::MAX_LINE, (int)l.lines[1].size());
    TEST_ASSERT_EQUAL_STRING("<ERR length>", l.lines[2].c_str());
    TEST_ASSERT_EQUAL_STRING("<ERR length>", l.lines[3].c_str());
    TEST_ASSERT_EQUAL_STRING("VER", l.lines[4].c_str());
}

void test_parse_send() {
    Command c = parse("SEND CQ CQ DE OK1CDJ", Mode::Ble);
    TEST_ASSERT_TRUE(c.cmd == Cmd::Send);
    TEST_ASSERT_EQUAL_STRING("CQ CQ DE OK1CDJ", c.text.c_str());
    TEST_ASSERT_TRUE(parse("send test", Mode::Http).cmd == Cmd::Send);
    TEST_ASSERT_EQUAL_STRING("arg", parse("SEND", Mode::Ble).error);
}

void test_parse_wpm_range() {
    Command c = parse("WPM 22", Mode::Ble);
    TEST_ASSERT_TRUE(c.cmd == Cmd::Wpm);
    TEST_ASSERT_EQUAL(22, c.wpm);
    TEST_ASSERT_TRUE(parse("WPM 5", Mode::Ble).cmd == Cmd::Wpm);
    TEST_ASSERT_TRUE(parse("WPM 50", Mode::Ble).cmd == Cmd::Wpm);
    TEST_ASSERT_EQUAL_STRING("range", parse("WPM 4", Mode::Ble).error);
    TEST_ASSERT_EQUAL_STRING("range", parse("WPM 51", Mode::Ble).error);
    TEST_ASSERT_EQUAL_STRING("arg", parse("WPM", Mode::Ble).error);
    TEST_ASSERT_EQUAL_STRING("arg", parse("WPM fast", Mode::Ble).error);
}

void test_parse_simple_commands() {
    TEST_ASSERT_TRUE(parse("STOP", Mode::Cwd).cmd == Cmd::Stop);
    TEST_ASSERT_TRUE(parse("STATUS", Mode::Http).cmd == Cmd::Status);
    TEST_ASSERT_TRUE(parse("VER", Mode::Ble).cmd == Cmd::Ver);
    TEST_ASSERT_EQUAL_STRING("cmd", parse("HELLO", Mode::Ble).error);
    TEST_ASSERT_EQUAL_STRING("cmd", parse("", Mode::Ble).error);
}

void test_parse_wifi() {
    Command c = parse("WIFI My Home Net\tsecret pass", Mode::Ble);
    TEST_ASSERT_TRUE(c.cmd == Cmd::Wifi);
    TEST_ASSERT_EQUAL_STRING("My Home Net", c.text.c_str());
    TEST_ASSERT_EQUAL_STRING("secret pass", c.password.c_str());
    Command open = parse("WIFI cafe\t", Mode::Ble);
    TEST_ASSERT_TRUE(open.cmd == Cmd::Wifi);
    TEST_ASSERT_EQUAL_STRING("", open.password.c_str());
    TEST_ASSERT_EQUAL_STRING("arg", parse("WIFI nopass", Mode::Ble).error);
}

void test_ble_only_commands() {
    TEST_ASSERT_EQUAL_STRING("mode", parse("WIFI a\tb", Mode::Http).error);
    TEST_ASSERT_EQUAL_STRING("mode", parse("MODE BLE", Mode::Http).error);
    TEST_ASSERT_EQUAL_STRING("mode", parse("APIKEY x", Mode::Cwd).error);
    Command m = parse("MODE http", Mode::Ble);
    TEST_ASSERT_TRUE(m.cmd == Cmd::SetMode);
    TEST_ASSERT_TRUE(m.mode == Mode::Http);
    TEST_ASSERT_EQUAL_STRING("arg", parse("MODE USB", Mode::Ble).error);
    Command k = parse("APIKEY abc123", Mode::Ble);
    TEST_ASSERT_TRUE(k.cmd == Cmd::ApiKey);
    TEST_ASSERT_EQUAL_STRING("abc123", k.text.c_str());
    TEST_ASSERT_TRUE(parse("APIKEY", Mode::Ble).cmd == Cmd::ApiKey);
}

void test_formatting() {
    TEST_ASSERT_EQUAL_STRING("IDLE WPM 22", formatStatus(false, 0, 22).c_str());
    TEST_ASSERT_EQUAL_STRING("SENDING 14 WPM 22", formatStatus(true, 14, 22).c_str());
    TEST_ASSERT_EQUAL_STRING("VER keyer 2.0.0 proto 1", formatVersion("2.0.0").c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_line_split_over_20_byte_writes);
    RUN_TEST(test_multiple_lines_in_one_write_and_crlf);
    RUN_TEST(test_line_length_limit);
    RUN_TEST(test_parse_send);
    RUN_TEST(test_parse_wpm_range);
    RUN_TEST(test_parse_simple_commands);
    RUN_TEST(test_parse_wifi);
    RUN_TEST(test_ble_only_commands);
    RUN_TEST(test_formatting);
    return UNITY_END();
}
