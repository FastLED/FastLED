/// @file parlio.cpp
/// @brief Tests for PARLIO-based ChannelEngine WS2812 encoding
///
/// Tests the WS2812-over-PARLIO bit encoding implementation used by ChannelEnginePARLIO.
/// Each LED byte is encoded as 32 bits (4 ticks per bit, 8 bits per byte).

#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

namespace {

/// @brief Encode a single LED byte (0x00-0xFF) into 32-bit PARLIO waveform
/// @param byte LED color byte to encode (R, G, or B value)
/// @return 32-bit waveform for PARLIO transmission
///
/// WS2812 timing with PARLIO 4-tick encoding:
/// - Bit 0: 0b1000 (312.5ns high, 937.5ns low)
/// - Bit 1: 0b1110 (937.5ns high, 312.5ns low)
///
/// Each byte produces 8 × 4 = 32 bits of output.
/// MSB is transmitted first (standard WS2812 protocol).
///
/// Examples:
/// - encodeLedByte(0xFF) = 0xEEEEEEEE (all bits 1)
/// - encodeLedByte(0x00) = 0x88888888 (all bits 0)
/// - encodeLedByte(0xAA) = 0xEE88EE88EE88EE88 (10101010)
static uint32_t encodeLedByte(uint8_t byte) {
    uint32_t result = 0;

    // Process each bit (MSB first)
    for (int i = 7; i >= 0; i--) {
        // Shift result to make room for 4 new bits
        result <<= 4;

        // Check bit value
        if (byte & (1 << i)) {
            // Bit is 1: encode as 0b1110 (hex: 0xE)
            result |= 0xE;
        } else {
            // Bit is 0: encode as 0b1000 (hex: 0x8)
            result |= 0x8;
        }
    }

    return result;
}

} // anonymous namespace

TEST_CASE("WS2812 PARLIO encoding - all zeros") {
    // Test encoding 0x00 (all bits 0)
    uint32_t result = encodeLedByte(0x00);

    // Each bit is 0, so encoding should be 0b1000 for each bit
    // 8 bits × 4 ticks = 32 bits total
    // 0b1000 1000 1000 1000 1000 1000 1000 1000 = 0x88888888
    CHECK_EQ(result, 0x88888888);
}

TEST_CASE("WS2812 PARLIO encoding - all ones") {
    // Test encoding 0xFF (all bits 1)
    uint32_t result = encodeLedByte(0xFF);

    // Each bit is 1, so encoding should be 0b1110 for each bit
    // 8 bits × 4 ticks = 32 bits total
    // 0b1110 1110 1110 1110 1110 1110 1110 1110 = 0xEEEEEEEE
    CHECK_EQ(result, 0xEEEEEEEE);
}

TEST_CASE("WS2812 PARLIO encoding - alternating pattern 0xAA") {
    // Test encoding 0xAA (10101010 in binary)
    uint32_t result = encodeLedByte(0xAA);

    // Binary: 1010 1010
    // Bit 7 (MSB): 1 → 1110
    // Bit 6: 0 → 1000
    // Bit 5: 1 → 1110
    // Bit 4: 0 → 1000
    // Bit 3: 1 → 1110
    // Bit 2: 0 → 1000
    // Bit 1: 1 → 1110
    // Bit 0 (LSB): 0 → 1000
    //
    // Combined: 1110 1000 1110 1000 1110 1000 1110 1000
    // Hex groups: E    8    E    8    E    8    E    8
    CHECK_EQ(result, 0xE8E8E8E8);
}

TEST_CASE("WS2812 PARLIO encoding - alternating pattern 0x55") {
    // Test encoding 0x55 (01010101 in binary)
    uint32_t result = encodeLedByte(0x55);

    // Binary: 0101 0101
    // Bit 7 (MSB): 0 → 1000
    // Bit 6: 1 → 1110
    // Bit 5: 0 → 1000
    // Bit 4: 1 → 1110
    // Bit 3: 0 → 1000
    // Bit 2: 1 → 1110
    // Bit 1: 0 → 1000
    // Bit 0 (LSB): 1 → 1110
    //
    // Combined: 1000 1110 1000 1110 1000 1110 1000 1110
    // Hex groups: 8    E    8    E    8    E    8    E
    CHECK_EQ(result, 0x8E8E8E8E);
}

TEST_CASE("WS2812 PARLIO encoding - arbitrary value 0x0F") {
    // Test encoding 0x0F (00001111 in binary)
    uint32_t result = encodeLedByte(0x0F);

    // Binary: 0000 1111
    // Bits 7-4: 0 0 0 0 → 1000 1000 1000 1000
    // Bits 3-0: 1 1 1 1 → 1110 1110 1110 1110
    //
    // Combined: 1000 1000 1000 1000 1110 1110 1110 1110
    // Hex groups: 8    8    8    8    E    E    E    E
    CHECK_EQ(result, 0x8888EEEE);
}

TEST_CASE("WS2812 PARLIO encoding - arbitrary value 0xF0") {
    // Test encoding 0xF0 (11110000 in binary)
    uint32_t result = encodeLedByte(0xF0);

    // Binary: 1111 0000
    // Bits 7-4: 1 1 1 1 → 1110 1110 1110 1110
    // Bits 3-0: 0 0 0 0 → 1000 1000 1000 1000
    //
    // Combined: 1110 1110 1110 1110 1000 1000 1000 1000
    // Hex groups: E    E    E    E    8    8    8    8
    CHECK_EQ(result, 0xEEEE8888);
}

TEST_CASE("WS2812 PARLIO encoding - arbitrary value 0xC3") {
    // Test encoding 0xC3 (11000011 in binary)
    uint32_t result = encodeLedByte(0xC3);

    // Binary: 1100 0011
    // Bit 7: 1 → 1110
    // Bit 6: 1 → 1110
    // Bit 5: 0 → 1000
    // Bit 4: 0 → 1000
    // Bit 3: 0 → 1000
    // Bit 2: 0 → 1000
    // Bit 1: 1 → 1110
    // Bit 0: 1 → 1110
    //
    // Combined: 1110 1110 1000 1000 1000 1000 1110 1110
    // Hex groups: E    E    8    8    8    8    E    E
    CHECK_EQ(result, 0xEE8888EE);
}

TEST_CASE("WS2812 PARLIO encoding - single bit patterns") {
    // Test each individual bit position

    SUBCASE("Only MSB set (0x80)") {
        uint32_t result = encodeLedByte(0x80);
        // 1000 0000 → 1110 1000 1000 1000 1000 1000 1000 1000
        CHECK_EQ(result, 0xE8888888);
    }

    SUBCASE("Only LSB set (0x01)") {
        uint32_t result = encodeLedByte(0x01);
        // 0000 0001 → 1000 1000 1000 1000 1000 1000 1000 1110
        CHECK_EQ(result, 0x8888888E);
    }

    SUBCASE("Middle bit set (0x10)") {
        uint32_t result = encodeLedByte(0x10);
        // 0001 0000 → 1000 1000 1000 1110 1000 1000 1000 1000
        CHECK_EQ(result, 0x888E8888);
    }
}
