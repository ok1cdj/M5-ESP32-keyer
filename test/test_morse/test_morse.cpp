// Host tests of the Morse table, text normalisation and battery curve.
#include <string.h>
#include <unity.h>

#include <string>

#include "battery_curve.h"
#include "morse.h"

using namespace keyer;

namespace {

bool toString(void* ctx, uint8_t item) {
    std::string* s = static_cast<std::string*>(ctx);
    if (item & JOIN_FLAG) {
        s->push_back(char(item & CHAR_MASK));
        s->push_back('^');
    } else {
        s->push_back(char(item));
    }
    return true;
}

std::string norm(const char* text, NormalizeResult* r = nullptr) {
    std::string out;
    NormalizeResult res = normalize(text, strlen(text), toString, &out);
    if (r) *r = res;
    return out;
}

}  // namespace

void test_character_set() {
    const char* supported = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/?.,=+-";
    for (const char* c = supported; *c; c++) {
        TEST_ASSERT_NOT_NULL_MESSAGE(morsePattern(*c), std::string(1, *c).c_str());
    }
    TEST_ASSERT_NULL(morsePattern('#'));
    TEST_ASSERT_NULL(morsePattern('a'));
    TEST_ASSERT_EQUAL_STRING("-.-.", morsePattern('C'));
    TEST_ASSERT_EQUAL_STRING("-...-", morsePattern('='));
    TEST_ASSERT_EQUAL_STRING("-....-", morsePattern('-'));
    TEST_ASSERT_EQUAL_STRING("..--..", morsePattern('?'));
}

void test_normalize_uppercases() { TEST_ASSERT_EQUAL_STRING("CQ DE OK1CDJ", norm("cq de ok1cdj").c_str()); }

void test_normalize_prosigns() {
    TEST_ASSERT_EQUAL_STRING("A^R", norm("<AR>").c_str());
    TEST_ASSERT_EQUAL_STRING("S^K", norm("<sk>").c_str());
    TEST_ASSERT_EQUAL_STRING("B^T", norm("<BT>").c_str());
    TEST_ASSERT_EQUAL_STRING("K^N", norm("<KN>").c_str());
    TEST_ASSERT_EQUAL_STRING("TU E^E", norm("TU <EE>").c_str());
}

void test_normalize_bad_chars() {
    NormalizeResult r;
    TEST_ASSERT_EQUAL_STRING("AB", norm("A#B", &r).c_str());
    TEST_ASSERT_TRUE(r.badChar);
    TEST_ASSERT_EQUAL_STRING("AB", norm("A<B", &r).c_str());
    TEST_ASSERT_TRUE(r.badChar);
    norm("CQ", &r);
    TEST_ASSERT_FALSE(r.badChar);
}

void test_normalize_whitespace() { TEST_ASSERT_EQUAL_STRING("A B C", norm("A\tB\nC").c_str()); }

void test_battery_curve() {
    TEST_ASSERT_EQUAL(100, batteryPercent(4300));
    TEST_ASSERT_EQUAL(100, batteryPercent(4200));
    TEST_ASSERT_EQUAL(50, batteryPercent(3830));
    TEST_ASSERT_EQUAL(0, batteryPercent(3300));
    TEST_ASSERT_EQUAL(0, batteryPercent(3000));
    uint8_t prev = 0;
    for (uint32_t mv = 3000; mv <= 4300; mv += 10) {
        uint8_t p = batteryPercent(mv);
        TEST_ASSERT_TRUE(p >= prev);
        prev = p;
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_character_set);
    RUN_TEST(test_normalize_uppercases);
    RUN_TEST(test_normalize_prosigns);
    RUN_TEST(test_normalize_bad_chars);
    RUN_TEST(test_normalize_whitespace);
    RUN_TEST(test_battery_curve);
    return UNITY_END();
}
