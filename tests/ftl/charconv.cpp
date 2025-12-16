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
        // Default int is typically 32-bit = 8 hex chars
        CHECK_EQ(to_hex(0), "00000000");
        CHECK_EQ(to_hex(0, false), "00000000");
        CHECK_EQ(to_hex(0, true), "00000000");
    }
}

TEST_CASE("fl::to_hex - positive integers") {
    SUBCASE("single digit") {
        // Default int is 32-bit = 8 hex chars with zero-padding
        CHECK_EQ(to_hex(1), "00000001");
        CHECK_EQ(to_hex(9), "00000009");
        CHECK_EQ(to_hex(15), "0000000f");
        CHECK_EQ(to_hex(15, true), "0000000F");
    }

    SUBCASE("multiple digits") {
        CHECK_EQ(to_hex(16), "00000010");
        CHECK_EQ(to_hex(255), "000000ff");
        CHECK_EQ(to_hex(255, true), "000000FF");
        CHECK_EQ(to_hex(256), "00000100");
        CHECK_EQ(to_hex(4095), "00000fff");
        CHECK_EQ(to_hex(4095, true), "00000FFF");
    }

    SUBCASE("large values") {
        CHECK_EQ(to_hex(65535), "0000ffff");
        CHECK_EQ(to_hex(65535, true), "0000FFFF");
        CHECK_EQ(to_hex(0xDEADBEEF), "deadbeef");
        CHECK_EQ(to_hex(0xDEADBEEF, true), "DEADBEEF");
    }
}

TEST_CASE("fl::to_hex - negative integers") {
    SUBCASE("negative values") {
        // Negative sign before zero-padded hex value
        CHECK_EQ(to_hex(-1), "-00000001");
        CHECK_EQ(to_hex(-16), "-00000010");
        CHECK_EQ(to_hex(-255), "-000000ff");
        CHECK_EQ(to_hex(-255, true), "-000000FF");
    }
}

TEST_CASE("fl::to_hex - different integer types") {
    SUBCASE("uint8_t") {
        uint8_t val = 0xAB;
        // 8-bit = 2 hex chars
        CHECK_EQ(to_hex(val), "ab");
        CHECK_EQ(to_hex(val, true), "AB");
    }

    SUBCASE("uint16_t") {
        uint16_t val = 0x1234;
        // 16-bit = 4 hex chars
        CHECK_EQ(to_hex(val), "1234");
        CHECK_EQ(to_hex(val, true), "1234");
    }

    SUBCASE("uint32_t") {
        uint32_t val = 0xABCD1234;
        // 32-bit = 8 hex chars
        CHECK_EQ(to_hex(val), "abcd1234");
        CHECK_EQ(to_hex(val, true), "ABCD1234");
    }

    SUBCASE("int8_t") {
        int8_t val = -16;
        // 8-bit = 2 hex chars with negative sign
        CHECK_EQ(to_hex(val), "-10");
    }

    SUBCASE("int16_t") {
        int16_t val = -256;
        // 16-bit = 4 hex chars with negative sign
        CHECK_EQ(to_hex(val), "-0100");
    }
}

TEST_CASE("fl::to_hex - case sensitivity") {
    SUBCASE("lowercase") {
        // 32-bit int = 8 hex chars with zero-padding
        CHECK_EQ(to_hex(0xABCDEF, false), "00abcdef");
        CHECK_EQ(to_hex(0xFEDCBA, false), "00fedcba");
    }

    SUBCASE("uppercase") {
        CHECK_EQ(to_hex(0xABCDEF, true), "00ABCDEF");
        CHECK_EQ(to_hex(0xFEDCBA, true), "00FEDCBA");
    }
}
