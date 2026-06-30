/// @file wave8.cpp
/// @brief Unit tests for waveform generation and transposition
///
/// Tests the wave transpose functionality used for multi-lane LED protocols.

#include "fl/channels/wave8.h"
#include "fl/channels/detail/wave8.hpp"
#include "fl/stl/cstring.h"
#include "test.h"
#include "fl/chipsets/led_timing.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("buildWave8ExpansionLUT") {
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
    FL_REQUIRE(lut.lut[nibble][0].data == 0xFC);

    // Check bit 2 (value=0) -> should use bit0 waveform (2 HIGH, 6 LOW)
    // Expected: 0b11000000 = 0xC0
    FL_REQUIRE(lut.lut[nibble][1].data == 0xC0);

    // Check bit 1 (value=1) -> should use bit1 waveform (6 HIGH, 2 LOW)
    // Expected: 0b11111100 = 0xFC
    FL_REQUIRE(lut.lut[nibble][2].data == 0xFC);

    // Check bit 0 (LSB, value=0) -> should use bit0 waveform (2 HIGH, 6 LOW)
    // Expected: 0b11000000 = 0xC0
    FL_REQUIRE(lut.lut[nibble][3].data == 0xC0);
}

FL_TEST_CASE("convertToWave8Bit") {
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
        FL_REQUIRE(output[i] == 0xAA);
    }
}

FL_TEST_CASE("buildWave8ByteExpansionLUT matches nibble path for all 256 bytes") {
    // #2526: the byte-indexed LUT expansion must be bit-identical to the
    // nibble-LUT expansion for every possible input byte.
    ChipsetTiming timing;
    timing.T1 = 250;
    timing.T2 = 500;
    timing.T3 = 250;

    Wave8BitExpansionLut nibble = buildWave8ExpansionLUT(timing);
    Wave8ByteExpansionLut byteLut = buildWave8ByteExpansionLUT(nibble);

    for (int b = 0; b < 256; ++b) {
        Wave8Byte ref;   // nibble path (current production)
        Wave8Byte got;   // byte path (#2526)
        detail::wave8_convert_byte_to_wave8byte(static_cast<u8>(b), nibble, &ref);
        detail::wave8_expand_byte(static_cast<u8>(b), byteLut, &got);
        for (int s = 0; s < 8; ++s) {
            FL_REQUIRE(got.symbols[s].data == ref.symbols[s].data);
        }
    }
}

FL_TEST_CASE("wave8Transpose_4_all_ones_and_zeros") {
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
        FL_REQUIRE(output[i] == 0x55);
    }
}

FL_TEST_CASE("wave8Transpose_4_all_ones") {
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
        FL_REQUIRE(output[i] == 0xFF);
    }
}

FL_TEST_CASE("wave8Transpose_4_all_zeros") {
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
        FL_REQUIRE(output[i] == 0x00);
    }
}

FL_TEST_CASE("wave8Transpose_4_distinct_patterns") {
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
        FL_REQUIRE(output[i] == 0x00); // Symbol 0 (bit 7)
    }

    // Verify last symbol (corresponds to bit 0 of input bytes)
    // Lane 0: bit 0 = 1 → contributes to bit positions for lane 0
    // Lane 1: bit 0 = 0
    // Lane 2: bit 0 = 0
    // Lane 3: bit 0 = 0
    // Expected pattern: only lane 0 bits set → 0b00010001 = 0x11
    for (int i = 28; i < 32; i++) {
        FL_REQUIRE(output[i] == 0x11); // Symbol 7 (bit 0)
    }
}

FL_TEST_CASE("wave8Transpose_4_incremental_verification") {
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
        FL_REQUIRE(output[i] == 0xFF);
    }

    // Symbol 1 (bit 6 = 0): 0x00 (4 bytes)
    for (int i = 4; i < 8; i++) {
        FL_REQUIRE(output[i] == 0x00);
    }

    // Symbol 2 (bit 5 = 1): 0xFF (4 bytes)
    for (int i = 8; i < 12; i++) {
        FL_REQUIRE(output[i] == 0xFF);
    }

    // Symbol 3 (bit 4 = 0): 0x00 (4 bytes)
    for (int i = 12; i < 16; i++) {
        FL_REQUIRE(output[i] == 0x00);
    }
}

FL_TEST_CASE("wave8Untranspose_2_simple_pattern") {
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
        FL_REQUIRE(untransposed[i] == 0xFF);
    }

    // The second 8 bytes should be all 0x00 (lane1 = 0x00)
    for (int i = 8; i < 16; i++) {
        FL_REQUIRE(untransposed[i] == 0x00);
    }
}

FL_TEST_CASE("wave8Untranspose_2_complex_pattern") {
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
    FL_REQUIRE(untransposed[0] == 0xFF);
    // Symbol 1 (bit 6 = 0): 0x00
    FL_REQUIRE(untransposed[1] == 0x00);
    // Symbol 2 (bit 5 = 1): 0xFF
    FL_REQUIRE(untransposed[2] == 0xFF);
    // Symbol 3 (bit 4 = 0): 0x00
    FL_REQUIRE(untransposed[3] == 0x00);
    // Symbol 4 (bit 3 = 1): 0xFF
    FL_REQUIRE(untransposed[4] == 0xFF);
    // Symbol 5 (bit 2 = 0): 0x00
    FL_REQUIRE(untransposed[5] == 0x00);
    // Symbol 6 (bit 1 = 1): 0xFF
    FL_REQUIRE(untransposed[6] == 0xFF);
    // Symbol 7 (bit 0 = 0): 0x00
    FL_REQUIRE(untransposed[7] == 0x00);

    // For 0x55 (01010101): alternating 0x00 and 0xFF bytes
    // Symbol 0 (bit 7 = 0): 0x00
    FL_REQUIRE(untransposed[8] == 0x00);
    // Symbol 1 (bit 6 = 1): 0xFF
    FL_REQUIRE(untransposed[9] == 0xFF);
    // Symbol 2 (bit 5 = 0): 0x00
    FL_REQUIRE(untransposed[10] == 0x00);
    // Symbol 3 (bit 4 = 1): 0xFF
    FL_REQUIRE(untransposed[11] == 0xFF);
    // Symbol 4 (bit 3 = 0): 0x00
    FL_REQUIRE(untransposed[12] == 0x00);
    // Symbol 5 (bit 2 = 1): 0xFF
    FL_REQUIRE(untransposed[13] == 0xFF);
    // Symbol 6 (bit 1 = 0): 0x00
    FL_REQUIRE(untransposed[14] == 0x00);
    // Symbol 7 (bit 0 = 1): 0xFF
    FL_REQUIRE(untransposed[15] == 0xFF);
}

FL_TEST_CASE("wave8Untranspose_4_simple_pattern") {
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
        FL_REQUIRE(untransposed[i] == 0xFF);
    }
}

FL_TEST_CASE("wave8Untranspose_4_alternating_pattern") {
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
        FL_REQUIRE(untransposed[i] == 0xFF);
    }

    // Verify: Lane 1 (bytes 8-15) should be all 0x00
    for (int i = 8; i < 16; i++) {
        FL_REQUIRE(untransposed[i] == 0x00);
    }

    // Verify: Lane 2 (bytes 16-23) should be all 0xFF
    for (int i = 16; i < 24; i++) {
        FL_REQUIRE(untransposed[i] == 0xFF);
    }

    // Verify: Lane 3 (bytes 24-31) should be all 0x00
    for (int i = 24; i < 32; i++) {
        FL_REQUIRE(untransposed[i] == 0x00);
    }
}

FL_TEST_CASE("wave8Untranspose_4_distinct_patterns") {
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
        FL_REQUIRE(untransposed[i] == 0x00);
    }
    // Symbol 7 should be 0xFF (bit 0 is 1)
    FL_REQUIRE(untransposed[7] == 0xFF);

    // Verify lane 1: 0x02 (0b00000010)
    // Symbols 0-5 should be 0x00 (bits 7-2 are 0)
    for (int i = 8; i < 8 + 6; i++) {
        FL_REQUIRE(untransposed[i] == 0x00);
    }
    // Symbol 6 should be 0xFF (bit 1 is 1)
    FL_REQUIRE(untransposed[14] == 0xFF);
    // Symbol 7 should be 0x00 (bit 0 is 0)
    FL_REQUIRE(untransposed[15] == 0x00);

    // Verify lane 2: 0x04 (0b00000100)
    // Symbols 0-4 should be 0x00 (bits 7-3 are 0)
    for (int i = 16; i < 16 + 5; i++) {
        FL_REQUIRE(untransposed[i] == 0x00);
    }
    // Symbol 5 should be 0xFF (bit 2 is 1)
    FL_REQUIRE(untransposed[21] == 0xFF);
    // Symbols 6-7 should be 0x00 (bits 1-0 are 0)
    FL_REQUIRE(untransposed[22] == 0x00);
    FL_REQUIRE(untransposed[23] == 0x00);

    // Verify lane 3: 0x08 (0b00001000)
    // Symbols 0-3 should be 0x00 (bits 7-4 are 0)
    for (int i = 24; i < 24 + 4; i++) {
        FL_REQUIRE(untransposed[i] == 0x00);
    }
    // Symbol 4 should be 0xFF (bit 3 is 1)
    FL_REQUIRE(untransposed[28] == 0xFF);
    // Symbols 5-7 should be 0x00 (bits 2-0 are 0)
    for (int i = 29; i < 32; i++) {
        FL_REQUIRE(untransposed[i] == 0x00);
    }
}

FL_TEST_CASE("wave8Transpose_8_all_ones") {
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
        FL_REQUIRE(output[i] == 0xFF);
    }
}

FL_TEST_CASE("wave8Transpose_8_all_zeros") {
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
        FL_REQUIRE(output[i] == 0x00);
    }
}

FL_TEST_CASE("wave8Transpose_8_alternating_pattern") {
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
        FL_REQUIRE(output[i] == 0x55);
    }
}

FL_TEST_CASE("wave8Transpose_8_distinct_patterns") {
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
        FL_REQUIRE(output[i] == 0x80);
    }

    // Symbol 7 (bit 0): Only lane 0 has bit 0 set (0x01)
    // Expected pattern: 0b00000001 = 0x01
    for (int i = 56; i < 64; i++) {
        FL_REQUIRE(output[i] == 0x01);
    }
}

FL_TEST_CASE("wave8Untranspose_8_all_ones") {
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
        FL_REQUIRE(untransposed[i] == 0xFF);
    }
}

FL_TEST_CASE("wave8Untranspose_8_alternating_pattern") {
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
            FL_REQUIRE(untransposed[lane * 8 + i] == expected);
        }
    }
}

FL_TEST_CASE("wave8Untranspose_8_distinct_patterns") {
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
            FL_REQUIRE(untransposed[lane * 8 + symbol] == expected);
        }
    }
}

FL_TEST_CASE("wave8Transpose_16_all_ones") {
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
        FL_REQUIRE(output[i] == 0xFF);
    }
}

FL_TEST_CASE("wave8Transpose_16_all_zeros") {
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
        FL_REQUIRE(output[i] == 0x00);
    }
}

FL_TEST_CASE("wave8Transpose_16_alternating_pattern") {
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
        FL_REQUIRE(output[i] == 0x55);
    }
}

FL_TEST_CASE("wave8Transpose_16_distinct_patterns") {
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
    FL_REQUIRE(output[0] == 0x80);  // High byte (lanes 8-15)
    FL_REQUIRE(output[1] == 0x80);  // Low byte (lanes 0-7)

    // Symbol 7 (bit 0): Only lanes 0 and 8 have bit 0 set (0x01)
    // Expected: low byte = 0x01 (lane 0), high byte = 0x01 (lane 8)
    FL_REQUIRE(output[112] == 0x01); // High byte (lanes 8-15)
    FL_REQUIRE(output[113] == 0x01); // Low byte (lanes 0-7)
}

FL_TEST_CASE("wave8Untranspose_16_all_ones") {
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
        FL_REQUIRE(untransposed[i] == 0xFF);
    }
}

FL_TEST_CASE("wave8Untranspose_16_alternating_pattern") {
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
            FL_REQUIRE(untransposed[lane * 8 + i] == expected);
        }
    }
}

FL_TEST_CASE("wave8Untranspose_16_distinct_patterns") {
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
            FL_REQUIRE(untransposed[lane * 8 + symbol] == expected);
        }
    }
}

// #2524: the optimized two-pass (8x8) transposes must be bit-identical to a
// ground-truth naive transpose across random inputs (the known-answer tests
// above only cover specific patterns).
FL_TEST_CASE("wave8 transpose_8/_16 == naive (random)") {
    auto naive8 = [](const Wave8Byte in[8], u8 out[8 * sizeof(Wave8Byte)]) {
        for (int s = 0; s < 8; s++) {
            for (int k = 0; k < 8; k++) {
                int b = 7 - k;
                int byte = 0;
                for (int l = 0; l < 8; l++) {
                    byte |= ((in[l].symbols[s].data >> b) & 1) << l;
                }
                out[s * 8 + k] = static_cast<u8>(byte);
            }
        }
    };
    auto naive16 = [](const Wave8Byte in[16], u8 out[16 * sizeof(Wave8Byte)]) {
        for (int s = 0; s < 8; s++) {
            for (int b = 0; b < 8; b++) {
                int lo = 0;
                int hi = 0;
                for (int l = 0; l < 8; l++) {
                    lo |= ((in[l].symbols[s].data >> b) & 1) << l;
                    hi |= ((in[l + 8].symbols[s].data >> b) & 1) << l;
                }
                out[s * 16 + (7 - b) * 2] = static_cast<u8>(lo);
                out[s * 16 + (7 - b) * 2 + 1] = static_cast<u8>(hi);
            }
        }
    };

    u32 seed = 0xABCDEF01u;
    for (int round = 0; round < 400; round++) {
        Wave8Byte w8[16];
        for (int l = 0; l < 16; l++) {
            for (int s = 0; s < 8; s++) {
                seed = seed * 1664525u + 1013904223u;
                w8[l].symbols[s].data = static_cast<u8>(seed >> 24);
            }
        }
        u8 got[16 * sizeof(Wave8Byte)];
        u8 exp[16 * sizeof(Wave8Byte)];

        fl::detail::wave8_transpose_8(w8, got);
        naive8(w8, exp);
        for (int i = 0; i < 8 * static_cast<int>(sizeof(Wave8Byte)); i++) {
            FL_REQUIRE(got[i] == exp[i]);
        }

        fl::detail::wave8_transpose_16(w8, got);
        naive16(w8, exp);
        for (int i = 0; i < 16 * static_cast<int>(sizeof(Wave8Byte)); i++) {
            FL_REQUIRE(got[i] == exp[i]);
        }
    }
}

FL_TEST_CASE("wave8Transpose_16x2_pipe2 == two sequential wave8Transpose_16 (random)") {
    // #2548: the 2-position fused pipe2 path must be bit-identical to running
    // wave8Transpose_16 twice on the same inputs. Validates that the internal
    // OR-tree interleaving does not change the result.
    ChipsetTiming timing;
    timing.T1 = 400;
    timing.T2 = 450;
    timing.T3 = 400;
    Wave8BitExpansionLut nib_lut = buildWave8ExpansionLUT(timing);
    Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib_lut);

    u32 seed = 0x12345678u;
    for (int round = 0; round < 1000; round++) {
        u8 lanes_a[16];
        u8 lanes_b[16];
        for (int l = 0; l < 16; l++) {
            seed = seed * 1664525u + 1013904223u;
            lanes_a[l] = static_cast<u8>(seed >> 24);
            seed = seed * 1664525u + 1013904223u;
            lanes_b[l] = static_cast<u8>(seed >> 24);
        }
        u8 ref_a[16 * sizeof(Wave8Byte)];
        u8 ref_b[16 * sizeof(Wave8Byte)];
        u8 got_a[16 * sizeof(Wave8Byte)];
        u8 got_b[16 * sizeof(Wave8Byte)];

        wave8Transpose_16(lanes_a, byte_lut, ref_a);
        wave8Transpose_16(lanes_b, byte_lut, ref_b);
        wave8Transpose_16x2_pipe2(lanes_a, lanes_b, byte_lut, got_a, got_b);

        for (int i = 0; i < 16 * static_cast<int>(sizeof(Wave8Byte)); i++) {
            FL_REQUIRE(got_a[i] == ref_a[i]);
            FL_REQUIRE(got_b[i] == ref_b[i]);
        }
    }
}

FL_TEST_CASE("wave8Transpose_16_bf1 == wave8Transpose_16 (random)") {
    // #2548 BF1: chipset-aware direct encode. Must produce bit-identical
    // output to the byte_lut path across 1000 random inputs.
    ChipsetTiming timing;
    timing.T1 = 400;
    timing.T2 = 450;
    timing.T3 = 400;
    Wave8BitExpansionLut nib_lut = buildWave8ExpansionLUT(timing);
    Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib_lut);

    u32 seed = 0x1357BD9Fu;
    for (int round = 0; round < 1000; round++) {
        u8 lanes[16];
        for (int l = 0; l < 16; l++) {
            seed = seed * 1664525u + 1013904223u;
            lanes[l] = static_cast<u8>(seed >> 24);
        }
        u8 ref[16 * sizeof(Wave8Byte)];
        u8 got[16 * sizeof(Wave8Byte)];
        wave8Transpose_16(lanes, byte_lut, ref);
        wave8Transpose_16_bf1(lanes, byte_lut, got);
        for (int i = 0; i < 16 * static_cast<int>(sizeof(Wave8Byte)); i++) {
            FL_REQUIRE(got[i] == ref[i]);
        }
    }
}

FL_TEST_CASE("wave8Transpose_16_bf1 == wave8Transpose_16 (multiple timings)") {
    // BF1 must hold for ANY Wave8 chipset/timing, not just one.
    const struct { u32 T1, T2, T3; } timings[] = {
        {250, 500, 250},    // 1/4, 3/4 pulse split
        {400, 450, 400},    // WS2812B Wave8
        {350, 350, 550},    // WS2812 variant
        {100, 800, 350},    // extreme bit-0 short
        {600, 100, 550},    // extreme bit-0 long
    };
    for (const auto &t : timings) {
        ChipsetTiming timing;
        timing.T1 = t.T1; timing.T2 = t.T2; timing.T3 = t.T3;
        Wave8BitExpansionLut nib = buildWave8ExpansionLUT(timing);
        Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib);

        u32 seed = (t.T1 * 31u) ^ (t.T2 * 17u) ^ t.T3;
        for (int round = 0; round < 200; ++round) {
            u8 lanes[16];
            for (int l = 0; l < 16; l++) {
                seed = seed * 1664525u + 1013904223u;
                lanes[l] = static_cast<u8>(seed >> 24);
            }
            u8 ref[16 * sizeof(Wave8Byte)];
            u8 got[16 * sizeof(Wave8Byte)];
            wave8Transpose_16(lanes, byte_lut, ref);
            wave8Transpose_16_bf1(lanes, byte_lut, got);
            for (int i = 0; i < 16 * static_cast<int>(sizeof(Wave8Byte)); i++) {
                FL_REQUIRE(got[i] == ref[i]);
            }
        }
    }
}

FL_TEST_CASE("wave8Transpose_8_bf1 == wave8Transpose_8 (random)") {
    ChipsetTiming timing;
    timing.T1 = 400; timing.T2 = 450; timing.T3 = 400;
    Wave8BitExpansionLut nib = buildWave8ExpansionLUT(timing);
    Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib);

    u32 seed = 0xFACE0008u;
    for (int round = 0; round < 1000; ++round) {
        u8 lanes[8];
        for (int l = 0; l < 8; ++l) {
            seed = seed * 1664525u + 1013904223u;
            lanes[l] = static_cast<u8>(seed >> 24);
        }
        u8 ref[8 * sizeof(Wave8Byte)];
        u8 got[8 * sizeof(Wave8Byte)];
        wave8Transpose_8(lanes, byte_lut, ref);
        wave8Transpose_8_bf1(lanes, byte_lut, got);
        for (int i = 0; i < 8 * static_cast<int>(sizeof(Wave8Byte)); ++i) {
            FL_REQUIRE(got[i] == ref[i]);
        }
    }
}

FL_TEST_CASE("wave8Transpose_4_bf1 == wave8Transpose_4 (random)") {
    ChipsetTiming timing;
    timing.T1 = 400; timing.T2 = 450; timing.T3 = 400;
    Wave8BitExpansionLut nib = buildWave8ExpansionLUT(timing);
    Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib);

    u32 seed = 0xFACE0004u;
    for (int round = 0; round < 1000; ++round) {
        u8 lanes[4];
        for (int l = 0; l < 4; ++l) {
            seed = seed * 1664525u + 1013904223u;
            lanes[l] = static_cast<u8>(seed >> 24);
        }
        u8 ref[4 * sizeof(Wave8Byte)];
        u8 got[4 * sizeof(Wave8Byte)];
        wave8Transpose_4(lanes, byte_lut, ref);
        wave8Transpose_4_bf1(lanes, byte_lut, got);
        for (int i = 0; i < 4 * static_cast<int>(sizeof(Wave8Byte)); ++i) {
            FL_REQUIRE(got[i] == ref[i]);
        }
    }
}

FL_TEST_CASE("wave8Transpose_2_bf1 == wave8Transpose_2 (random)") {
    ChipsetTiming timing;
    timing.T1 = 400; timing.T2 = 450; timing.T3 = 400;
    Wave8BitExpansionLut nib = buildWave8ExpansionLUT(timing);
    Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib);

    u32 seed = 0xFACE0002u;
    for (int round = 0; round < 1000; ++round) {
        u8 lanes[2];
        for (int l = 0; l < 2; ++l) {
            seed = seed * 1664525u + 1013904223u;
            lanes[l] = static_cast<u8>(seed >> 24);
        }
        u8 ref[2 * sizeof(Wave8Byte)];
        u8 got[2 * sizeof(Wave8Byte)];
        wave8Transpose_2(lanes, byte_lut, ref);
        wave8Transpose_2_bf1(lanes, byte_lut, got);
        for (int i = 0; i < 2 * static_cast<int>(sizeof(Wave8Byte)); ++i) {
            FL_REQUIRE(got[i] == ref[i]);
        }
    }
}

FL_TEST_CASE("wave8Transpose_8/4/2_bf1 == wave8Transpose_X (multiple timings)") {
    // BF1 must hold for any Wave8 chipset/timing across lane counts.
    const struct { u32 T1, T2, T3; } timings[] = {
        {250, 500, 250}, {400, 450, 400}, {350, 350, 550},
        {100, 800, 350}, {600, 100, 550},
    };
    for (const auto &t : timings) {
        ChipsetTiming timing;
        timing.T1 = t.T1; timing.T2 = t.T2; timing.T3 = t.T3;
        Wave8BitExpansionLut nib = buildWave8ExpansionLUT(timing);
        Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib);

        u32 seed = (t.T1 * 31u) ^ (t.T2 * 17u) ^ t.T3;
        for (int round = 0; round < 200; ++round) {
            u8 l8[8], l4[4], l2[2];
            for (int l = 0; l < 8; ++l) {
                seed = seed * 1664525u + 1013904223u;
                l8[l] = static_cast<u8>(seed >> 24);
                if (l < 4) l4[l] = l8[l];
                if (l < 2) l2[l] = l8[l];
            }
            u8 r8[64], g8[64], r4[32], g4[32], r2[16], g2[16];
            wave8Transpose_8(l8, byte_lut, r8);
            wave8Transpose_8_bf1(l8, byte_lut, g8);
            wave8Transpose_4(l4, byte_lut, r4);
            wave8Transpose_4_bf1(l4, byte_lut, g4);
            wave8Transpose_2(l2, byte_lut, r2);
            wave8Transpose_2_bf1(l2, byte_lut, g2);
            for (int i = 0; i < 64; ++i) FL_REQUIRE(g8[i] == r8[i]);
            for (int i = 0; i < 32; ++i) FL_REQUIRE(g4[i] == r4[i]);
            for (int i = 0; i < 16; ++i) FL_REQUIRE(g2[i] == r2[i]);
        }
    }
}

FL_TEST_CASE("wave8Transpose_16x4_bf1_pipe4 == 4× wave8Transpose_16 (random)") {
    ChipsetTiming timing;
    timing.T1 = 400; timing.T2 = 450; timing.T3 = 400;
    Wave8BitExpansionLut nib = buildWave8ExpansionLUT(timing);
    Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib);

    u32 seed = 0xCAFEBABEu;
    for (int round = 0; round < 500; ++round) {
        u8 la[16], lb[16], lc[16], ld[16];
        for (int l = 0; l < 16; l++) {
            seed = seed * 1664525u + 1013904223u; la[l] = static_cast<u8>(seed >> 24);
            seed = seed * 1664525u + 1013904223u; lb[l] = static_cast<u8>(seed >> 24);
            seed = seed * 1664525u + 1013904223u; lc[l] = static_cast<u8>(seed >> 24);
            seed = seed * 1664525u + 1013904223u; ld[l] = static_cast<u8>(seed >> 24);
        }
        u8 ra[128], rb[128], rc[128], rd[128];
        u8 ga[128], gb[128], gc[128], gd[128];
        wave8Transpose_16(la, byte_lut, ra);
        wave8Transpose_16(lb, byte_lut, rb);
        wave8Transpose_16(lc, byte_lut, rc);
        wave8Transpose_16(ld, byte_lut, rd);
        wave8Transpose_16x4_bf1_pipe4(la, lb, lc, ld, byte_lut, ga, gb, gc, gd);
        for (int i = 0; i < 128; i++) {
            FL_REQUIRE(ga[i] == ra[i]);
            FL_REQUIRE(gb[i] == rb[i]);
            FL_REQUIRE(gc[i] == rc[i]);
            FL_REQUIRE(gd[i] == rd[i]);
        }
    }
}

FL_TEST_CASE("wave8Transpose_16x4_pipe4 == four sequential wave8Transpose_16 (random)") {
    // #2548: the 4-position fused pipe4 path must be bit-identical to running
    // wave8Transpose_16 four times on the same inputs. Confirms that scaling
    // the cross-position interleaving from 2 to 4 still preserves semantics.
    ChipsetTiming timing;
    timing.T1 = 400;
    timing.T2 = 450;
    timing.T3 = 400;
    Wave8BitExpansionLut nib_lut = buildWave8ExpansionLUT(timing);
    Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nib_lut);

    u32 seed = 0x9ABCDEF0u;
    for (int round = 0; round < 500; round++) {
        u8 lanes_a[16];
        u8 lanes_b[16];
        u8 lanes_c[16];
        u8 lanes_d[16];
        for (int l = 0; l < 16; l++) {
            seed = seed * 1664525u + 1013904223u;
            lanes_a[l] = static_cast<u8>(seed >> 24);
            seed = seed * 1664525u + 1013904223u;
            lanes_b[l] = static_cast<u8>(seed >> 24);
            seed = seed * 1664525u + 1013904223u;
            lanes_c[l] = static_cast<u8>(seed >> 24);
            seed = seed * 1664525u + 1013904223u;
            lanes_d[l] = static_cast<u8>(seed >> 24);
        }
        u8 ref_a[16 * sizeof(Wave8Byte)];
        u8 ref_b[16 * sizeof(Wave8Byte)];
        u8 ref_c[16 * sizeof(Wave8Byte)];
        u8 ref_d[16 * sizeof(Wave8Byte)];
        u8 got_a[16 * sizeof(Wave8Byte)];
        u8 got_b[16 * sizeof(Wave8Byte)];
        u8 got_c[16 * sizeof(Wave8Byte)];
        u8 got_d[16 * sizeof(Wave8Byte)];

        wave8Transpose_16(lanes_a, byte_lut, ref_a);
        wave8Transpose_16(lanes_b, byte_lut, ref_b);
        wave8Transpose_16(lanes_c, byte_lut, ref_c);
        wave8Transpose_16(lanes_d, byte_lut, ref_d);
        wave8Transpose_16x4_pipe4(lanes_a, lanes_b, lanes_c, lanes_d, byte_lut,
                                  got_a, got_b, got_c, got_d);

        for (int i = 0; i < 16 * static_cast<int>(sizeof(Wave8Byte)); i++) {
            FL_REQUIRE(got_a[i] == ref_a[i]);
            FL_REQUIRE(got_b[i] == ref_b[i]);
            FL_REQUIRE(got_c[i] == ref_c[i]);
            FL_REQUIRE(got_d[i] == ref_d[i]);
        }
    }
}

// =============================================================================
// Cross-driver wave8 regression tests (issue #3023 workstream B).
//
// Every wave8-using driver (parlio, i2s, uart, spi, future lpuart) packs LED
// bits through the shared `Wave8BitExpansionLut` / `Wave8ByteExpansionLut`
// pipeline. A bug in the LUT builders corrupts every driver simultaneously.
// The original "matches nibble path" test only walked one synthetic timing —
// a regression that only shows up at a real chipset's bit distribution would
// have slipped through. The cases below widen the matrix to the real timings
// the project actually ships.
// =============================================================================

namespace {

template <typename TRAIT>
ChipsetTiming makeRealTiming() {
    ChipsetTiming t;
    t.T1 = TRAIT::T1;
    t.T2 = TRAIT::T2;
    t.T3 = TRAIT::T3;
    return t;
}

struct RealTimingEntry {
    const char* name;
    ChipsetTiming timing;
};

}  // namespace

FL_TEST_CASE("Wave8 byte LUT == nibble LUT for all 256 bytes across real chipsets") {
    // Covers the wave8-using chipset families with distinct T1/T2/T3 ratios:
    //  - 800 kHz: WS2812, WS2812B-V5, SK6812 RGBW, UCS1903B, TM1809, PL9823.
    //  - 400 kHz: WS2811, WS2815 (overclockable), UCS1903_400KHZ.
    const RealTimingEntry entries[] = {
        {"WS2812_800KHZ",   makeRealTiming<TIMING_WS2812_800KHZ>()},
        {"WS2812B_V5",      makeRealTiming<TIMING_WS2812B_V5>()},
        {"WS2813",          makeRealTiming<TIMING_WS2813>()},
        {"SK6812",          makeRealTiming<TIMING_SK6812>()},
        {"UCS1903B_800KHZ", makeRealTiming<TIMING_UCS1903B_800KHZ>()},
        {"TM1809_800KHZ",   makeRealTiming<TIMING_TM1809_800KHZ>()},
        {"PL9823",          makeRealTiming<TIMING_PL9823>()},
        {"WS2811_400KHZ",   makeRealTiming<TIMING_WS2811_400KHZ>()},
        {"WS2815",          makeRealTiming<TIMING_WS2815>()},
        {"UCS1903_400KHZ",  makeRealTiming<TIMING_UCS1903_400KHZ>()},
    };

    for (const auto& entry : entries) {
        Wave8BitExpansionLut nibble = buildWave8ExpansionLUT(entry.timing);
        Wave8ByteExpansionLut byteLut = buildWave8ByteExpansionLUT(nibble);

        for (int b = 0; b < 256; ++b) {
            Wave8Byte ref;
            Wave8Byte got;
            detail::wave8_convert_byte_to_wave8byte(static_cast<u8>(b), nibble, &ref);
            detail::wave8_expand_byte(static_cast<u8>(b), byteLut, &got);
            for (int s = 0; s < 8; ++s) {
                FL_INFO("chipset=" << entry.name << " byte=0x" << b
                        << " symbol=" << s);
                FL_REQUIRE(got.symbols[s].data == ref.symbols[s].data);
            }
        }
    }
}

FL_TEST_CASE("Wave8 bit cell starts HIGH and ends LOW for every real chipset") {
    // The wave8 wire shape — every bit cell starts HIGH (T1 phase) and ends
    // LOW (T3 phase) — is the framing constraint that lets an inverted UART
    // (#3023 workstream A: `Bus::UART`) carry wave8 traffic byte-by-byte.
    // Pinning the invariant here means a future timing-table edit can't
    // quietly invalidate the assumption the LPUART canHandle() rests on.
    const RealTimingEntry entries[] = {
        {"WS2812_800KHZ",   makeRealTiming<TIMING_WS2812_800KHZ>()},
        {"WS2812B_V5",      makeRealTiming<TIMING_WS2812B_V5>()},
        {"WS2813",          makeRealTiming<TIMING_WS2813>()},
        {"SK6812",          makeRealTiming<TIMING_SK6812>()},
        {"UCS1903B_800KHZ", makeRealTiming<TIMING_UCS1903B_800KHZ>()},
        {"TM1809_800KHZ",   makeRealTiming<TIMING_TM1809_800KHZ>()},
        {"PL9823",          makeRealTiming<TIMING_PL9823>()},
        {"WS2811_400KHZ",   makeRealTiming<TIMING_WS2811_400KHZ>()},
        {"WS2815",          makeRealTiming<TIMING_WS2815>()},
        {"UCS1903_400KHZ",  makeRealTiming<TIMING_UCS1903_400KHZ>()},
    };

    for (const auto& entry : entries) {
        Wave8ByteExpansionLut lut = buildWave8ByteExpansionLUT(
            buildWave8ExpansionLUT(entry.timing));

        // byte 0xFF: first symbol's MSB is the very first pulse on the wire;
        // last symbol's LSB is the last pulse. Both must satisfy the topology.
        const Wave8Byte& all_ones = lut.lut[0xFF];
        FL_INFO("chipset=" << entry.name << " byte=0xFF first pulse HIGH");
        FL_REQUIRE((all_ones.symbols[0].data & 0x80) != 0);
        FL_INFO("chipset=" << entry.name << " byte=0xFF last pulse LOW");
        FL_REQUIRE((all_ones.symbols[7].data & 0x01) == 0);

        // byte 0x00: same framing invariant on the all-zeros path.
        const Wave8Byte& all_zeros = lut.lut[0x00];
        FL_INFO("chipset=" << entry.name << " byte=0x00 first pulse HIGH");
        FL_REQUIRE((all_zeros.symbols[0].data & 0x80) != 0);
        FL_INFO("chipset=" << entry.name << " byte=0x00 last pulse LOW");
        FL_REQUIRE((all_zeros.symbols[7].data & 0x01) == 0);
    }
}

FL_TEST_CASE("wave8Transpose_4 matches between nibble LUT and byte LUT") {
    // Transposing drivers (parlio multi-lane TX, future lpuart fan-out) take
    // one of two `wave8Transpose_N` overloads. Drift between them is a silent
    // cross-driver compat break.
    const RealTimingEntry entries[] = {
        {"WS2812_800KHZ", makeRealTiming<TIMING_WS2812_800KHZ>()},
        {"SK6812",        makeRealTiming<TIMING_SK6812>()},
        {"WS2811_400KHZ", makeRealTiming<TIMING_WS2811_400KHZ>()},
    };

    const uint8_t lane_patterns[][4] = {
        {0xFF, 0x00, 0xFF, 0x00},
        {0x00, 0xFF, 0x00, 0xFF},
        {0xAA, 0x55, 0xAA, 0x55},
        {0xDE, 0xAD, 0xBE, 0xEF},
    };

    for (const auto& entry : entries) {
        Wave8BitExpansionLut nibble_lut = buildWave8ExpansionLUT(entry.timing);
        Wave8ByteExpansionLut byte_lut = buildWave8ByteExpansionLUT(nibble_lut);

        for (const auto& lanes : lane_patterns) {
            uint8_t out_nibble[4 * sizeof(Wave8Byte)] = {};
            uint8_t out_byte[4 * sizeof(Wave8Byte)] = {};

            wave8Transpose_4(lanes, nibble_lut, out_nibble);
            wave8Transpose_4(lanes, byte_lut, out_byte);

            FL_INFO("chipset=" << entry.name
                    << " lanes={0x" << static_cast<int>(lanes[0])
                    << ",0x" << static_cast<int>(lanes[1])
                    << ",0x" << static_cast<int>(lanes[2])
                    << ",0x" << static_cast<int>(lanes[3]) << "}");
            for (int i = 0; i < 4 * static_cast<int>(sizeof(Wave8Byte)); ++i) {
                FL_REQUIRE(out_nibble[i] == out_byte[i]);
            }
        }
    }
}

} // FL_TEST_FILE
