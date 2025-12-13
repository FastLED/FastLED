// Test file for ftl/charconv.h
//
// This file tests character conversion functions provided by ftl/charconv.h,
// which provides utilities similar to C++17 <charconv>.
//
// Functions tested:
// - fl::to_hex(): Convert integer values to hexadecimal string representation

#include "test.h"
#include "ftl/charconv.h"

using namespace fl;

// ============================================================================
// Hexadecimal Conversion Tests
// ============================================================================

TEST_CASE("fl::to_hex - zero value") {
    SUBCASE("zero") {
        CHECK_EQ(to_hex(0), "0");
        CHECK_EQ(to_hex(0, false), "0");
        CHECK_EQ(to_hex(0, true), "0");
    }
}

TEST_CASE("fl::to_hex - positive integers") {
    SUBCASE("single digit") {
        CHECK_EQ(to_hex(1), "1");
        CHECK_EQ(to_hex(9), "9");
        CHECK_EQ(to_hex(15), "f");
        CHECK_EQ(to_hex(15, true), "F");
    }

    SUBCASE("multiple digits") {
        CHECK_EQ(to_hex(16), "10");
        CHECK_EQ(to_hex(255), "ff");
        CHECK_EQ(to_hex(255, true), "FF");
        CHECK_EQ(to_hex(256), "100");
        CHECK_EQ(to_hex(4095), "fff");
        CHECK_EQ(to_hex(4095, true), "FFF");
    }

    SUBCASE("large values") {
        CHECK_EQ(to_hex(65535), "ffff");
        CHECK_EQ(to_hex(65535, true), "FFFF");
        CHECK_EQ(to_hex(0xDEADBEEF), "deadbeef");
        CHECK_EQ(to_hex(0xDEADBEEF, true), "DEADBEEF");
    }
}

TEST_CASE("fl::to_hex - negative integers") {
    SUBCASE("negative values") {
        CHECK_EQ(to_hex(-1), "-1");
        CHECK_EQ(to_hex(-16), "-10");
        CHECK_EQ(to_hex(-255), "-ff");
        CHECK_EQ(to_hex(-255, true), "-FF");
    }
}

TEST_CASE("fl::to_hex - different integer types") {
    SUBCASE("uint8_t") {
        uint8_t val = 0xAB;
        CHECK_EQ(to_hex(val), "ab");
        CHECK_EQ(to_hex(val, true), "AB");
    }

    SUBCASE("uint16_t") {
        uint16_t val = 0x1234;
        CHECK_EQ(to_hex(val), "1234");
        CHECK_EQ(to_hex(val, true), "1234");
    }

    SUBCASE("uint32_t") {
        uint32_t val = 0xABCD1234;
        CHECK_EQ(to_hex(val), "abcd1234");
        CHECK_EQ(to_hex(val, true), "ABCD1234");
    }

    SUBCASE("int8_t") {
        int8_t val = -16;
        CHECK_EQ(to_hex(val), "-10");
    }

    SUBCASE("int16_t") {
        int16_t val = -256;
        CHECK_EQ(to_hex(val), "-100");
    }
}

TEST_CASE("fl::to_hex - case sensitivity") {
    SUBCASE("lowercase") {
        CHECK_EQ(to_hex(0xABCDEF, false), "abcdef");
        CHECK_EQ(to_hex(0xFEDCBA, false), "fedcba");
    }

    SUBCASE("uppercase") {
        CHECK_EQ(to_hex(0xABCDEF, true), "ABCDEF");
        CHECK_EQ(to_hex(0xFEDCBA, true), "FEDCBA");
    }
}
