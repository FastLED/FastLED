// Test file for fl/stl/charconv.h
//
// This file tests character conversion functions provided by fl/stl/charconv.h,
// which provides utilities similar to C++17 <charconv>.
//
// Functions tested:
// - fl::to_hex(): Convert integer values to hexadecimal string representation

#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/stl/string.h"

using namespace fl;

// ============================================================================
// Hexadecimal Conversion Tests
// ============================================================================

TEST_CASE("fl::to_hex - zero value") {
    SUBCASE("zero") {
        // Default behavior is minimal representation
        CHECK_EQ(to_hex(0), "0");
        CHECK_EQ(to_hex(0, false), "0");
        CHECK_EQ(to_hex(0, true), "0");
        // With padding enabled
        CHECK_EQ(to_hex(0, false, true), "00000000");
        CHECK_EQ(to_hex(0, true, true), "00000000");
    }
}

TEST_CASE("fl::to_hex - positive integers") {
    SUBCASE("single digit") {
        // Default behavior is minimal representation
        CHECK_EQ(to_hex(1), "1");
        CHECK_EQ(to_hex(9), "9");
        CHECK_EQ(to_hex(15), "f");
        CHECK_EQ(to_hex(15, true), "F");
        // With padding enabled (int is 32-bit = 8 hex chars)
        CHECK_EQ(to_hex(1, false, true), "00000001");
        CHECK_EQ(to_hex(15, true, true), "0000000F");
    }

    SUBCASE("multiple digits") {
        // Minimal representation
        CHECK_EQ(to_hex(16), "10");
        CHECK_EQ(to_hex(255), "ff");
        CHECK_EQ(to_hex(255, true), "FF");
        CHECK_EQ(to_hex(256), "100");
        CHECK_EQ(to_hex(4095), "fff");
        CHECK_EQ(to_hex(4095, true), "FFF");
        // With padding enabled
        CHECK_EQ(to_hex(255, false, true), "000000ff");
        CHECK_EQ(to_hex(4095, true, true), "00000FFF");
    }

    SUBCASE("large values") {
        CHECK_EQ(to_hex(65535), "ffff");
        CHECK_EQ(to_hex(65535, true), "FFFF");
        CHECK_EQ(to_hex(0xDEADBEEF), "deadbeef");
        CHECK_EQ(to_hex(0xDEADBEEF, true), "DEADBEEF");
        // With padding enabled
        CHECK_EQ(to_hex(65535, false, true), "0000ffff");
        CHECK_EQ(to_hex(0xDEADBEEF, true, true), "DEADBEEF");
    }
}

TEST_CASE("fl::to_hex - negative integers") {
    SUBCASE("negative values") {
        // Negative sign before minimal hex value
        CHECK_EQ(to_hex(-1), "-1");
        CHECK_EQ(to_hex(-16), "-10");
        CHECK_EQ(to_hex(-255), "-ff");
        CHECK_EQ(to_hex(-255, true), "-FF");
        // With padding enabled
        CHECK_EQ(to_hex(-1, false, true), "-00000001");
        CHECK_EQ(to_hex(-255, true, true), "-000000FF");
    }
}

TEST_CASE("fl::to_hex - different integer types") {
    SUBCASE("uint8_t") {
        uint8_t val = 0xAB;
        // Minimal representation
        CHECK_EQ(to_hex(val), "ab");
        CHECK_EQ(to_hex(val, true), "AB");
        // With padding (8-bit = 2 hex chars)
        CHECK_EQ(to_hex(val, false, true), "ab");
        CHECK_EQ(to_hex(val, true, true), "AB");
    }

    SUBCASE("uint16_t") {
        uint16_t val = 0x1234;
        // Minimal representation
        CHECK_EQ(to_hex(val), "1234");
        CHECK_EQ(to_hex(val, true), "1234");
        // With padding (16-bit = 4 hex chars) - already full width
        CHECK_EQ(to_hex(val, false, true), "1234");
        CHECK_EQ(to_hex(val, true, true), "1234");
    }

    SUBCASE("uint32_t") {
        uint32_t val = 0xABCD1234;
        // Minimal representation
        CHECK_EQ(to_hex(val), "abcd1234");
        CHECK_EQ(to_hex(val, true), "ABCD1234");
        // With padding (32-bit = 8 hex chars) - already full width
        CHECK_EQ(to_hex(val, false, true), "abcd1234");
        CHECK_EQ(to_hex(val, true, true), "ABCD1234");
    }

    SUBCASE("int8_t") {
        int8_t val = -16;
        // Minimal representation with negative sign
        CHECK_EQ(to_hex(val), "-10");
        // With padding (8-bit = 2 hex chars)
        CHECK_EQ(to_hex(val, false, true), "-10");
    }

    SUBCASE("int16_t") {
        int16_t val = -256;
        // Minimal representation with negative sign
        CHECK_EQ(to_hex(val), "-100");
        // With padding (16-bit = 4 hex chars)
        CHECK_EQ(to_hex(val, false, true), "-0100");
    }
}

TEST_CASE("fl::to_hex - case sensitivity") {
    SUBCASE("lowercase") {
        // Minimal representation
        CHECK_EQ(to_hex(0xABCDEF, false), "abcdef");
        CHECK_EQ(to_hex(0xFEDCBA, false), "fedcba");
        // With padding (32-bit int = 8 hex chars)
        CHECK_EQ(to_hex(0xABCDEF, false, true), "00abcdef");
        CHECK_EQ(to_hex(0xFEDCBA, false, true), "00fedcba");
    }

    SUBCASE("uppercase") {
        // Minimal representation
        CHECK_EQ(to_hex(0xABCDEF, true), "ABCDEF");
        CHECK_EQ(to_hex(0xFEDCBA, true), "FEDCBA");
        // With padding
        CHECK_EQ(to_hex(0xABCDEF, true, true), "00ABCDEF");
        CHECK_EQ(to_hex(0xFEDCBA, true, true), "00FEDCBA");
    }
}
