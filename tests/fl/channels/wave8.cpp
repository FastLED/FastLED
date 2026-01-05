/// @file wave8.cpp
/// @brief Unit tests for waveform generation and transposition
///
/// Tests the wave transpose functionality used for multi-lane LED protocols.

#include "fl/channels/wave8.h"
#include "fl/stl/cstring.h"
#include "test.h"

using namespace fl;

TEST_CASE("buildWave8ExpansionLUT") {
    // Create timing where bit0 is at 1/4 time, bit1 is at 3/4 time
    ChipsetTiming timing;
    timing.T1 = 250; // bit0 goes LOW at 1/4 of period (250/1000)
    timing.T2 = 500; // bit1 goes LOW at 3/4 of period ((250+500)/1000)
    timing.T3 = 250; // period = 1000ns total

    // Build the LUT
    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Expected waveforms with 8 pulses per bit:
    // bit0: 1/4 * 8 = 2 HIGH pulses, 6 LOW pulses
    // bit1: 3/4 * 8 = 6 HIGH pulses, 2 LOW pulses

    // Test nibble 0xA (1010 binary) - used in pattern 0xAA
    // 0xA = bit3=1, bit2=0, bit1=1, bit0=0 (MSB first)
    const uint8_t nibble = 0xA;

    // Check bit 3 (MSB, value=1) -> should use bit1 waveform (6 HIGH, 2 LOW)
    // Expected: 0b11111100 = 0xFC
    REQUIRE(lut.lut[nibble][0].data == 0xFC);

    // Check bit 2 (value=0) -> should use bit0 waveform (2 HIGH, 6 LOW)
    // Expected: 0b11000000 = 0xC0
    REQUIRE(lut.lut[nibble][1].data == 0xC0);

    // Check bit 1 (value=1) -> should use bit1 waveform (6 HIGH, 2 LOW)
    // Expected: 0b11111100 = 0xFC
    REQUIRE(lut.lut[nibble][2].data == 0xFC);

    // Check bit 0 (LSB, value=0) -> should use bit0 waveform (2 HIGH, 6 LOW)
    // Expected: 0b11000000 = 0xC0
    REQUIRE(lut.lut[nibble][3].data == 0xC0);
}

TEST_CASE("convertToWave8Bit") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    // This creates simple patterns for testing transpose
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Convert to Wave8Bit
    // lane0: 0xff (all bits are 1) = high nibble 0xF, low nibble 0xF
    // lane1: 0x00 (all bits are 0) = high nibble 0x0, low nibble 0x0
    uint8_t lanes[2] = {0xff, 0x00};
    uint8_t output[2 * sizeof(Wave8Byte)]; // 16 bytes

    wave8Transpose_2(lanes, lut, output);

    // Test transposed output
    // Expected: 0xAA (0b10101010) for all 16 bytes
    // - lane0 has all 1s (0xff) → all 8 symbols have all bits set (11111111)
    // - lane1 has all 0s (0x00) → all 8 symbols have all bits clear (00000000)
    // - Bit interleaving: [lane0_bit, lane1_bit, lane0_bit, lane1_bit, ...]
    // - Result: 0b10101010 = 0xAA for every output byte

    for (int i = 0; i < 16; i++) {
        REQUIRE(output[i] == 0xAA);
    }
}

TEST_CASE("wave8Transpose_4_all_ones_and_zeros") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case 1: Alternating pattern (0xFF, 0x00, 0xFF, 0x00)
    // Lane 0: 0xFF (all 1s) → all HIGH waveform
    // Lane 1: 0x00 (all 0s) → all LOW waveform
    // Lane 2: 0xFF (all 1s) → all HIGH waveform
    // Lane 3: 0x00 (all 0s) → all LOW waveform
    uint8_t lanes[4] = {0xFF, 0x00, 0xFF, 0x00};
    uint8_t output[4 * sizeof(Wave8Byte)]; // 32 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_4(lanes, lut, output);

    // Expected: 4-lane interleaving with pattern [L3, L2, L1, L0, L3, L2, L1, L0]
    // Lane 0 (0xFF) = all 1s → contributes bit 0 (LSB)
    // Lane 1 (0x00) = all 0s → contributes bit 1
    // Lane 2 (0xFF) = all 1s → contributes bit 2
    // Lane 3 (0x00) = all 0s → contributes bit 3 (MSB)
    // Result: 0b0101 = 0x5 for high nibble, 0b0101 = 0x5 for low nibble
    // Expected: 0x55 for all 32 output bytes
    for (int i = 0; i < 32; i++) {
        REQUIRE(output[i] == 0x55);
    }
}

TEST_CASE("wave8Transpose_4_all_ones") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case 2: All lanes 0xFF
    uint8_t lanes[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t output[4 * sizeof(Wave8Byte)]; // 32 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_4(lanes, lut, output);

    // All lanes have 1s → all bits set in output
    // Expected: 0xFF for all 32 output bytes
    for (int i = 0; i < 32; i++) {
        REQUIRE(output[i] == 0xFF);
    }
}

TEST_CASE("wave8Transpose_4_all_zeros") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case 3: All lanes 0x00
    uint8_t lanes[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t output[4 * sizeof(Wave8Byte)]; // 32 bytes
    fl::memset(output, 0xFF, sizeof(output)); // Pre-fill to verify clearing

    wave8Transpose_4(lanes, lut, output);

    // All lanes have 0s → all bits clear in output
    // Expected: 0x00 for all 32 output bytes
    for (int i = 0; i < 32; i++) {
        REQUIRE(output[i] == 0x00);
    }
}

TEST_CASE("wave8Transpose_4_distinct_patterns") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case 4: Distinct values per lane
    // Lane 0: 0x01 (0b00000001) - only LSB set
    // Lane 1: 0x02 (0b00000010)
    // Lane 2: 0x04 (0b00000100)
    // Lane 3: 0x08 (0b00001000)
    uint8_t lanes[4] = {0x01, 0x02, 0x04, 0x08};
    uint8_t output[4 * sizeof(Wave8Byte)]; // 32 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_4(lanes, lut, output);

    // Verify first symbol (corresponds to bit 7 of input bytes, all 0)
    // All lanes have bit 7 = 0 → output bytes should be 0x00
    for (int i = 0; i < 4; i++) {
        REQUIRE(output[i] == 0x00); // Symbol 0 (bit 7)
    }

    // Verify last symbol (corresponds to bit 0 of input bytes)
    // Lane 0: bit 0 = 1 → contributes to bit positions for lane 0
    // Lane 1: bit 0 = 0
    // Lane 2: bit 0 = 0
    // Lane 3: bit 0 = 0
    // Expected pattern: only lane 0 bits set → 0b00010001 = 0x11
    for (int i = 28; i < 32; i++) {
        REQUIRE(output[i] == 0x11); // Symbol 7 (bit 0)
    }
}

TEST_CASE("wave8Transpose_4_incremental_verification") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test incremental transposition with specific byte pattern
    // Lane pattern: 0xAA (0b10101010) for all lanes
    uint8_t lanes[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    uint8_t output[4 * sizeof(Wave8Byte)]; // 32 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_4(lanes, lut, output);

    // 0xAA = 0b10101010 (alternating bits)
    // Each symbol processes one bit position across all lanes
    // All lanes have same pattern → all bits interleaved identically
    // For symbols with bit=1: all lanes contribute 1 → 0b11111111 = 0xFF
    // For symbols with bit=0: all lanes contribute 0 → 0b00000000 = 0x00

    // Symbol 0 (bit 7 = 1): 0xFF (4 bytes)
    for (int i = 0; i < 4; i++) {
        REQUIRE(output[i] == 0xFF);
    }

    // Symbol 1 (bit 6 = 0): 0x00 (4 bytes)
    for (int i = 4; i < 8; i++) {
        REQUIRE(output[i] == 0x00);
    }

    // Symbol 2 (bit 5 = 1): 0xFF (4 bytes)
    for (int i = 8; i < 12; i++) {
        REQUIRE(output[i] == 0xFF);
    }

    // Symbol 3 (bit 4 = 0): 0x00 (4 bytes)
    for (int i = 12; i < 16; i++) {
        REQUIRE(output[i] == 0x00);
    }
}
