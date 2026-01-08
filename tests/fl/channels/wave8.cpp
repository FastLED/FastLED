/// @file wave8.cpp
/// @brief Unit tests for waveform generation and transposition
///
/// Tests the wave transpose functionality used for multi-lane LED protocols.

#include "fl/channels/wave8.h"
#include "fl/stl/cstring.h"
#include "doctest.h"
#include "fl/chipsets/led_timing.h"

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

TEST_CASE("wave8Untranspose_2_simple_pattern") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with known pattern
    uint8_t lanes[2] = {0xff, 0x00};
    uint8_t transposed[2 * sizeof(Wave8Byte)]; // 16 bytes
    uint8_t untransposed[2 * sizeof(Wave8Byte)]; // 16 bytes

    // Transpose
    wave8Transpose_2(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_2(transposed, untransposed);

    // Verify: untransposed should match the original Wave8Byte structures
    // The first 8 bytes should be all 0xFF (lane0 = 0xff)
    for (int i = 0; i < 8; i++) {
        REQUIRE(untransposed[i] == 0xFF);
    }

    // The second 8 bytes should be all 0x00 (lane1 = 0x00)
    for (int i = 8; i < 16; i++) {
        REQUIRE(untransposed[i] == 0x00);
    }
}

TEST_CASE("wave8Untranspose_2_complex_pattern") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with alternating pattern
    uint8_t lanes[2] = {0xAA, 0x55}; // 0b10101010, 0b01010101
    uint8_t transposed[2 * sizeof(Wave8Byte)];
    uint8_t untransposed[2 * sizeof(Wave8Byte)];

    // Transpose
    wave8Transpose_2(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_2(transposed, untransposed);

    // Verify: Each symbol should match the expected wave pattern
    // For 0xAA (10101010): alternating 0xFF and 0x00 bytes
    // Symbol 0 (bit 7 = 1): 0xFF
    REQUIRE(untransposed[0] == 0xFF);
    // Symbol 1 (bit 6 = 0): 0x00
    REQUIRE(untransposed[1] == 0x00);
    // Symbol 2 (bit 5 = 1): 0xFF
    REQUIRE(untransposed[2] == 0xFF);
    // Symbol 3 (bit 4 = 0): 0x00
    REQUIRE(untransposed[3] == 0x00);
    // Symbol 4 (bit 3 = 1): 0xFF
    REQUIRE(untransposed[4] == 0xFF);
    // Symbol 5 (bit 2 = 0): 0x00
    REQUIRE(untransposed[5] == 0x00);
    // Symbol 6 (bit 1 = 1): 0xFF
    REQUIRE(untransposed[6] == 0xFF);
    // Symbol 7 (bit 0 = 0): 0x00
    REQUIRE(untransposed[7] == 0x00);

    // For 0x55 (01010101): alternating 0x00 and 0xFF bytes
    // Symbol 0 (bit 7 = 0): 0x00
    REQUIRE(untransposed[8] == 0x00);
    // Symbol 1 (bit 6 = 1): 0xFF
    REQUIRE(untransposed[9] == 0xFF);
    // Symbol 2 (bit 5 = 0): 0x00
    REQUIRE(untransposed[10] == 0x00);
    // Symbol 3 (bit 4 = 1): 0xFF
    REQUIRE(untransposed[11] == 0xFF);
    // Symbol 4 (bit 3 = 0): 0x00
    REQUIRE(untransposed[12] == 0x00);
    // Symbol 5 (bit 2 = 1): 0xFF
    REQUIRE(untransposed[13] == 0xFF);
    // Symbol 6 (bit 1 = 0): 0x00
    REQUIRE(untransposed[14] == 0x00);
    // Symbol 7 (bit 0 = 1): 0xFF
    REQUIRE(untransposed[15] == 0xFF);
}

TEST_CASE("wave8Untranspose_4_simple_pattern") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with all 0xFF
    uint8_t lanes[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t transposed[4 * sizeof(Wave8Byte)]; // 32 bytes
    uint8_t untransposed[4 * sizeof(Wave8Byte)]; // 32 bytes

    // Transpose
    wave8Transpose_4(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_4(transposed, untransposed);

    // Verify: all lanes should be 0xFF (8 bytes per lane × 4 lanes)
    for (int i = 0; i < 32; i++) {
        REQUIRE(untransposed[i] == 0xFF);
    }
}

TEST_CASE("wave8Untranspose_4_alternating_pattern") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with alternating pattern
    uint8_t lanes[4] = {0xFF, 0x00, 0xFF, 0x00};
    uint8_t transposed[4 * sizeof(Wave8Byte)];
    uint8_t untransposed[4 * sizeof(Wave8Byte)];

    // Transpose
    wave8Transpose_4(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_4(transposed, untransposed);

    // Verify: Lane 0 (bytes 0-7) should be all 0xFF
    for (int i = 0; i < 8; i++) {
        REQUIRE(untransposed[i] == 0xFF);
    }

    // Verify: Lane 1 (bytes 8-15) should be all 0x00
    for (int i = 8; i < 16; i++) {
        REQUIRE(untransposed[i] == 0x00);
    }

    // Verify: Lane 2 (bytes 16-23) should be all 0xFF
    for (int i = 16; i < 24; i++) {
        REQUIRE(untransposed[i] == 0xFF);
    }

    // Verify: Lane 3 (bytes 24-31) should be all 0x00
    for (int i = 24; i < 32; i++) {
        REQUIRE(untransposed[i] == 0x00);
    }
}

TEST_CASE("wave8Untranspose_4_distinct_patterns") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with distinct values per lane
    uint8_t lanes[4] = {0x01, 0x02, 0x04, 0x08};
    uint8_t transposed[4 * sizeof(Wave8Byte)];
    uint8_t untransposed[4 * sizeof(Wave8Byte)];

    // Transpose
    wave8Transpose_4(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_4(transposed, untransposed);

    // Verify lane 0: 0x01 (0b00000001)
    // Symbols 0-6 should be 0x00 (bits 7-1 are 0)
    for (int i = 0; i < 7; i++) {
        REQUIRE(untransposed[i] == 0x00);
    }
    // Symbol 7 should be 0xFF (bit 0 is 1)
    REQUIRE(untransposed[7] == 0xFF);

    // Verify lane 1: 0x02 (0b00000010)
    // Symbols 0-5 should be 0x00 (bits 7-2 are 0)
    for (int i = 8; i < 8 + 6; i++) {
        REQUIRE(untransposed[i] == 0x00);
    }
    // Symbol 6 should be 0xFF (bit 1 is 1)
    REQUIRE(untransposed[14] == 0xFF);
    // Symbol 7 should be 0x00 (bit 0 is 0)
    REQUIRE(untransposed[15] == 0x00);

    // Verify lane 2: 0x04 (0b00000100)
    // Symbols 0-4 should be 0x00 (bits 7-3 are 0)
    for (int i = 16; i < 16 + 5; i++) {
        REQUIRE(untransposed[i] == 0x00);
    }
    // Symbol 5 should be 0xFF (bit 2 is 1)
    REQUIRE(untransposed[21] == 0xFF);
    // Symbols 6-7 should be 0x00 (bits 1-0 are 0)
    REQUIRE(untransposed[22] == 0x00);
    REQUIRE(untransposed[23] == 0x00);

    // Verify lane 3: 0x08 (0b00001000)
    // Symbols 0-3 should be 0x00 (bits 7-4 are 0)
    for (int i = 24; i < 24 + 4; i++) {
        REQUIRE(untransposed[i] == 0x00);
    }
    // Symbol 4 should be 0xFF (bit 3 is 1)
    REQUIRE(untransposed[28] == 0xFF);
    // Symbols 5-7 should be 0x00 (bits 2-0 are 0)
    for (int i = 29; i < 32; i++) {
        REQUIRE(untransposed[i] == 0x00);
    }
}

TEST_CASE("wave8Transpose_8_all_ones") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case: All lanes 0xFF
    uint8_t lanes[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t output[8 * sizeof(Wave8Byte)]; // 64 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_8(lanes, lut, output);

    // All lanes have 1s → all bits set in output
    // Expected: 0xFF for all 64 output bytes
    for (int i = 0; i < 64; i++) {
        REQUIRE(output[i] == 0xFF);
    }
}

TEST_CASE("wave8Transpose_8_all_zeros") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case: All lanes 0x00
    uint8_t lanes[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t output[8 * sizeof(Wave8Byte)]; // 64 bytes
    fl::memset(output, 0xFF, sizeof(output)); // Pre-fill to verify clearing

    wave8Transpose_8(lanes, lut, output);

    // All lanes have 0s → all bits clear in output
    // Expected: 0x00 for all 64 output bytes
    for (int i = 0; i < 64; i++) {
        REQUIRE(output[i] == 0x00);
    }
}

TEST_CASE("wave8Transpose_8_alternating_pattern") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case: Alternating pattern
    uint8_t lanes[8] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    uint8_t output[8 * sizeof(Wave8Byte)]; // 64 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_8(lanes, lut, output);

    // Lane pattern: [L7=0x00, L6=0xFF, L5=0x00, L4=0xFF, L3=0x00, L2=0xFF, L1=0x00, L0=0xFF]
    // Lane 0 (0xFF) = all 1s → contributes bit 0 (LSB)
    // Lane 1 (0x00) = all 0s → contributes bit 1
    // Lane 2 (0xFF) = all 1s → contributes bit 2
    // Lane 3 (0x00) = all 0s → contributes bit 3
    // Lane 4 (0xFF) = all 1s → contributes bit 4
    // Lane 5 (0x00) = all 0s → contributes bit 5
    // Lane 6 (0xFF) = all 1s → contributes bit 6
    // Lane 7 (0x00) = all 0s → contributes bit 7 (MSB)
    // Result: 0b01010101 = 0x55 for all 64 output bytes
    for (int i = 0; i < 64; i++) {
        REQUIRE(output[i] == 0x55);
    }
}

TEST_CASE("wave8Transpose_8_distinct_patterns") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case: Each lane has single bit set
    uint8_t lanes[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    uint8_t output[8 * sizeof(Wave8Byte)]; // 64 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_8(lanes, lut, output);

    // Each lane has a single bit set at different positions
    // Symbol 0 (bit 7): Only lane 7 has bit 7 set (0x80)
    // Expected pattern: 0b10000000 = 0x80
    for (int i = 0; i < 8; i++) {
        REQUIRE(output[i] == 0x80);
    }

    // Symbol 7 (bit 0): Only lane 0 has bit 0 set (0x01)
    // Expected pattern: 0b00000001 = 0x01
    for (int i = 56; i < 64; i++) {
        REQUIRE(output[i] == 0x01);
    }
}

TEST_CASE("wave8Untranspose_8_all_ones") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with all 0xFF
    uint8_t lanes[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t transposed[8 * sizeof(Wave8Byte)]; // 64 bytes
    uint8_t untransposed[8 * sizeof(Wave8Byte)]; // 64 bytes

    // Transpose
    wave8Transpose_8(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_8(transposed, untransposed);

    // Verify: all lanes should be 0xFF (8 bytes per lane × 8 lanes)
    for (int i = 0; i < 64; i++) {
        REQUIRE(untransposed[i] == 0xFF);
    }
}

TEST_CASE("wave8Untranspose_8_alternating_pattern") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with alternating pattern
    uint8_t lanes[8] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    uint8_t transposed[8 * sizeof(Wave8Byte)];
    uint8_t untransposed[8 * sizeof(Wave8Byte)];

    // Transpose
    wave8Transpose_8(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_8(transposed, untransposed);

    // Verify each lane
    for (int lane = 0; lane < 8; lane++) {
        uint8_t expected = (lane % 2 == 0) ? 0xFF : 0x00;
        for (int i = 0; i < 8; i++) {
            REQUIRE(untransposed[lane * 8 + i] == expected);
        }
    }
}

TEST_CASE("wave8Untranspose_8_distinct_patterns") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with distinct values per lane
    uint8_t lanes[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    uint8_t transposed[8 * sizeof(Wave8Byte)];
    uint8_t untransposed[8 * sizeof(Wave8Byte)];

    // Transpose
    wave8Transpose_8(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_8(transposed, untransposed);

    // Verify each lane has the expected pattern
    const uint8_t expected_patterns[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

    for (int lane = 0; lane < 8; lane++) {
        // Find which bit is set in the expected pattern
        int set_bit = -1;
        for (int bit = 0; bit < 8; bit++) {
            if (expected_patterns[lane] & (1 << bit)) {
                set_bit = bit;
                break;
            }
        }

        // Verify the wave pattern
        for (int symbol = 0; symbol < 8; symbol++) {
            uint8_t expected = (symbol == (7 - set_bit)) ? 0xFF : 0x00;
            REQUIRE(untransposed[lane * 8 + symbol] == expected);
        }
    }
}

TEST_CASE("wave8Transpose_16_all_ones") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case: All lanes 0xFF
    uint8_t lanes[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t output[16 * sizeof(Wave8Byte)]; // 128 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_16(lanes, lut, output);

    // All lanes have 1s → all bits set in output
    // Expected: 0xFF for all 128 output bytes
    for (int i = 0; i < 128; i++) {
        REQUIRE(output[i] == 0xFF);
    }
}

TEST_CASE("wave8Transpose_16_all_zeros") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case: All lanes 0x00
    uint8_t lanes[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t output[16 * sizeof(Wave8Byte)]; // 128 bytes
    fl::memset(output, 0xFF, sizeof(output)); // Pre-fill to verify clearing

    wave8Transpose_16(lanes, lut, output);

    // All lanes have 0s → all bits clear in output
    // Expected: 0x00 for all 128 output bytes
    for (int i = 0; i < 128; i++) {
        REQUIRE(output[i] == 0x00);
    }
}

TEST_CASE("wave8Transpose_16_alternating_pattern") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case: Alternating pattern
    uint8_t lanes[16] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
                         0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    uint8_t output[16 * sizeof(Wave8Byte)]; // 128 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_16(lanes, lut, output);

    // Lane pattern: [L15=0x00, L14=0xFF, ..., L1=0x00, L0=0xFF]
    // Even lanes (0, 2, 4, ..., 14) = 0xFF → contribute bit
    // Odd lanes (1, 3, 5, ..., 15) = 0x00 → no contribution
    // Result: 0b01010101 = 0x55 for low byte (lanes 0-7)
    //         0b01010101 = 0x55 for high byte (lanes 8-15)
    for (int i = 0; i < 128; i++) {
        REQUIRE(output[i] == 0x55);
    }
}

TEST_CASE("wave8Transpose_16_distinct_patterns") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test case: Each lane has single bit set at different positions
    uint8_t lanes[16] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                         0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    uint8_t output[16 * sizeof(Wave8Byte)]; // 128 bytes
    fl::memset(output, 0, sizeof(output));

    wave8Transpose_16(lanes, lut, output);

    // Symbol 0 (bit 7): Only lanes 7 and 15 have bit 7 set (0x80)
    // Expected: low byte = 0x80 (lane 7), high byte = 0x80 (lane 15)
    REQUIRE(output[0] == 0x80);  // High byte (lanes 8-15)
    REQUIRE(output[1] == 0x80);  // Low byte (lanes 0-7)

    // Symbol 7 (bit 0): Only lanes 0 and 8 have bit 0 set (0x01)
    // Expected: low byte = 0x01 (lane 0), high byte = 0x01 (lane 8)
    REQUIRE(output[112] == 0x01); // High byte (lanes 8-15)
    REQUIRE(output[113] == 0x01); // Low byte (lanes 0-7)
}

TEST_CASE("wave8Untranspose_16_all_ones") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with all 0xFF
    uint8_t lanes[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t transposed[16 * sizeof(Wave8Byte)]; // 128 bytes
    uint8_t untransposed[16 * sizeof(Wave8Byte)]; // 128 bytes

    // Transpose
    wave8Transpose_16(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_16(transposed, untransposed);

    // Verify: all lanes should be 0xFF (8 bytes per lane × 16 lanes)
    for (int i = 0; i < 128; i++) {
        REQUIRE(untransposed[i] == 0xFF);
    }
}

TEST_CASE("wave8Untranspose_16_alternating_pattern") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with alternating pattern
    uint8_t lanes[16] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
                         0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    uint8_t transposed[16 * sizeof(Wave8Byte)];
    uint8_t untransposed[16 * sizeof(Wave8Byte)];

    // Transpose
    wave8Transpose_16(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_16(transposed, untransposed);

    // Verify each lane
    for (int lane = 0; lane < 16; lane++) {
        uint8_t expected = (lane % 2 == 0) ? 0xFF : 0x00;
        for (int i = 0; i < 8; i++) {
            REQUIRE(untransposed[lane * 8 + i] == expected);
        }
    }
}

TEST_CASE("wave8Untranspose_16_distinct_patterns") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with distinct values per lane
    uint8_t lanes[16] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                         0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    uint8_t transposed[16 * sizeof(Wave8Byte)];
    uint8_t untransposed[16 * sizeof(Wave8Byte)];

    // Transpose
    wave8Transpose_16(lanes, lut, transposed);

    // Untranspose
    wave8Untranspose_16(transposed, untransposed);

    // Verify each lane has the expected pattern
    const uint8_t expected_patterns[16] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                                           0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

    for (int lane = 0; lane < 16; lane++) {
        // Find which bit is set in the expected pattern
        int set_bit = -1;
        for (int bit = 0; bit < 8; bit++) {
            if (expected_patterns[lane] & (1 << bit)) {
                set_bit = bit;
                break;
            }
        }

        // Verify the wave pattern
        for (int symbol = 0; symbol < 8; symbol++) {
            uint8_t expected = (symbol == (7 - set_bit)) ? 0xFF : 0x00;
            REQUIRE(untransposed[lane * 8 + symbol] == expected);
        }
    }
}
