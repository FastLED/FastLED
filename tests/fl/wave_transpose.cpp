/// @file wave_transpose.cpp
/// @brief Unit tests for waveform generation and transposition

#include "test.h"
#include "fl/channels/wave_transpose.h"
#include "ftl/cstring.h"

using namespace fl;

// ============================================================================
// Simplified waveTranspose8_2 tests (LUT-based, fixed 8:1 expansion)
// ============================================================================

TEST_CASE("waveTranspose8_2_simple - basic correctness with known pattern") {
    // Simple test pattern: bit0=all LOW, bit1=all HIGH for 8 pulses
    const uint8_t bit0_wave[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t bit1_wave[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Build LUT
    wave_nibble_lut_t lut;
    buildWaveNibbleLut(lut, bit0_wave, bit1_wave);

    uint8_t output[16];
    fl::memset(output, 0xAA, sizeof(output)); // Fill with pattern to detect writes

    // laneA = 0xFF (all bits 1 → bit1_wave), laneB = 0x00 (all bits 0 → bit0_wave)
    waveTranspose8_2_simple(0xFF, 0x00, lut, output);

    // Verify output was written
    bool has_data = false;
    for (size_t i = 0; i < 16; i++) {
        if (output[i] != 0xAA) {
            has_data = true;
            break;
        }
    }
    CHECK(has_data);

    // With laneA=0xFF (all HIGH pulses) and laneB=0x00 (all LOW pulses):
    // Each output byte packs 4 time ticks, 2 bits per tick (lane0, lane1)
    // Bit pattern: [t0_L0=1, t0_L1=0, t1_L0=1, t1_L1=0, t2_L0=1, t2_L1=0, t3_L0=1, t3_L1=0]
    // Binary: 0b01010101 = 0x55
    for (size_t i = 0; i < 16; i++) {
        CHECK(output[i] == 0x55);
    }
}

TEST_CASE("waveTranspose8_2_simple - all zeros") {
    const uint8_t bit0_wave[8] = {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t bit1_wave[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00};

    // Build LUT
    wave_nibble_lut_t lut;
    buildWaveNibbleLut(lut, bit0_wave, bit1_wave);

    uint8_t output[16];
    fl::memset(output, 0xAA, sizeof(output));

    // Both lanes all zeros → both expand to bit0_wave (3 HIGH, 5 LOW)
    waveTranspose8_2_simple(0x00, 0x00, lut, output);

    // First 3 pulses are HIGH (0xFF), pulse 3 is LOW (0x00)
    // Output byte 0 packs ticks 0-3 (pulses 0-3):
    //   Tick 0 (pulse 0): both HIGH → bits [0,1] = 0b11 = 0x03
    //   Tick 1 (pulse 1): both HIGH → bits [2,3] = 0b1100 = 0x0C
    //   Tick 2 (pulse 2): both HIGH → bits [4,5] = 0b110000 = 0x30
    //   Tick 3 (pulse 3): both LOW → bits [6,7] = 0b00 = 0x00
    //   Total: 0x03 | 0x0C | 0x30 = 0b00111111 = 0x3F
    CHECK(output[0] == 0x3F);
}

TEST_CASE("waveTranspose8_2_simple - all ones") {
    const uint8_t bit0_wave[8] = {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t bit1_wave[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00};

    // Build LUT
    wave_nibble_lut_t lut;
    buildWaveNibbleLut(lut, bit0_wave, bit1_wave);

    uint8_t output[16];
    fl::memset(output, 0x00, sizeof(output));

    // Both lanes all ones → both expand to bit1_wave (5 HIGH, 3 LOW)
    waveTranspose8_2_simple(0xFF, 0xFF, lut, output);

    // First 5 pulses are HIGH → bits set to 1
    // Output byte 0 packs ticks 0,1,2,3: all both lanes HIGH → 0xFF
    CHECK(output[0] == 0xFF);
}

TEST_CASE("waveTranspose8_2_simple - alternating pattern") {
    const uint8_t bit0_wave[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t bit1_wave[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Build LUT
    wave_nibble_lut_t lut;
    buildWaveNibbleLut(lut, bit0_wave, bit1_wave);

    uint8_t output[16];
    fl::memset(output, 0x00, sizeof(output));

    // laneA = 0xAA (10101010), laneB = 0x55 (01010101)
    waveTranspose8_2_simple(0xAA, 0x55, lut, output);

    // Verify we got non-zero data
    bool has_nonzero = false;
    for (size_t i = 0; i < 16; i++) {
        if (output[i] != 0x00) {
            has_nonzero = true;
            break;
        }
    }
    CHECK(has_nonzero);
}

TEST_CASE("waveTranspose8_2_simple - WS2812 timing pattern") {
    // WS2812 realistic timing: bit0 = 3H+5L, bit1 = 5H+3L
    const uint8_t bit0_wave[8] = {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t bit1_wave[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00};

    // Build LUT
    wave_nibble_lut_t lut;
    buildWaveNibbleLut(lut, bit0_wave, bit1_wave);

    uint8_t output[16];
    fl::memset(output, 0x00, sizeof(output));

    // Test with mixed pattern: laneA = 0xF0, laneB = 0x0F
    waveTranspose8_2_simple(0xF0, 0x0F, lut, output);

    // Verify output is populated (we don't verify exact values, just non-zero)
    bool has_nonzero = false;
    for (size_t i = 0; i < 16; i++) {
        if (output[i] != 0x00) {
            has_nonzero = true;
            break;
        }
    }
    CHECK(has_nonzero);
}

// ============================================================================
// Nibble LUT generation tests
// ============================================================================

TEST_CASE("buildWaveNibbleLut - correctness") {
    // Simple test pattern: bit0=all LOW, bit1=all HIGH for 8 pulses
    const uint8_t bit0_wave[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t bit1_wave[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    wave_nibble_lut_t lut;
    buildWaveNibbleLut(lut, bit0_wave, bit1_wave);

    // Test all 16 nibbles - each nibble maps to 32 pulse bytes (4 bits × 8 pulses/bit, MSB first)
    for (int nibble = 0; nibble < 16; nibble++) {
        for (int bit_pos = 0; bit_pos < 4; bit_pos++) {
            int bit_mask = (0x8 >> bit_pos);  // 0x8, 0x4, 0x2, 0x1 (MSB first)
            bool bit_value = (nibble & bit_mask) != 0;
            const uint8_t* expected_wave = bit_value ? bit1_wave : bit0_wave;

            // Verify 8-byte waveform pattern for this bit
            size_t wave_offset = bit_pos * 8;
            for (size_t i = 0; i < 8; i++) {
                CHECK(lut[nibble][wave_offset + i] == expected_wave[i]);
            }
        }
    }

    // Test specific nibble: 0x6 = 0b0110 (bits 3,2,1,0 = 0,1,1,0)
    // Expected waveform: bit0(0) + bit1(1) + bit1(1) + bit0(0)
    uint8_t nibble_0x6[32];
    for (size_t i = 0; i < 8; i++) nibble_0x6[i] = 0x00;       // bit 3 = 0 → bit0_wave
    for (size_t i = 8; i < 16; i++) nibble_0x6[i] = 0xFF;      // bit 2 = 1 → bit1_wave
    for (size_t i = 16; i < 24; i++) nibble_0x6[i] = 0xFF;     // bit 1 = 1 → bit1_wave
    for (size_t i = 24; i < 32; i++) nibble_0x6[i] = 0x00;     // bit 0 = 0 → bit0_wave

    for (size_t i = 0; i < 32; i++) {
        CHECK(lut[0x6][i] == nibble_0x6[i]);
    }
}

TEST_CASE("buildWaveNibbleLut - WS2812 timing") {
    // WS2812 realistic timing: bit0 = 3H+5L, bit1 = 5H+3L
    const uint8_t bit0_wave[8] = {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t bit1_wave[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00};

    wave_nibble_lut_t lut;
    buildWaveNibbleLut(lut, bit0_wave, bit1_wave);

    // Verify nibble 0x0 = 0b0000 (all bits 0)
    for (size_t bit_pos = 0; bit_pos < 4; bit_pos++) {
        size_t offset = bit_pos * 8;
        for (size_t i = 0; i < 8; i++) {
            CHECK(lut[0x0][offset + i] == bit0_wave[i]);
        }
    }

    // Verify nibble 0xF = 0b1111 (all bits 1)
    for (size_t bit_pos = 0; bit_pos < 4; bit_pos++) {
        size_t offset = bit_pos * 8;
        for (size_t i = 0; i < 8; i++) {
            CHECK(lut[0xF][offset + i] == bit1_wave[i]);
        }
    }
}
