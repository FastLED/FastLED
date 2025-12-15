/// @file wave_transpose.cpp
/// @brief Unit tests for waveform generation and transposition
///
/// Tests the wave transpose functionality used for multi-lane LED protocols.

#include "test.h"
#include "fl/channels/wave_transpose.h"
#include "ftl/cstring.h"

using namespace fl;


TEST_CASE("convertToWavePulses8") {

    // Step 1: Build nibble LUT manually
    // Each nibble (0x0 to 0xF) maps to 4 WavePulses8 structures
    // Each bit in the nibble: 1 -> 0xff * 8, 0 -> 0x00 * 8
    Wave8BitExpansionLut lut;

    // Initialize all 16 nibble entries
    for (int nibble = 0; nibble < 16; nibble++) {
        // For each nibble, generate 4 WavePulses8 (one per bit, MSB first)
        for (int bit_pos = 0; bit_pos < 4; bit_pos++) {
            // Check if bit is set in nibble (MSB first: bit 3, 2, 1, 0)
            bool bit_set = (nibble >> (3 - bit_pos)) & 1;
            uint8_t pulse_value = bit_set ? 0xff : 0x00;

            // Fill all 8 bytes of the WavePulses8
            for (int byte = 0; byte < 8; byte++) {
                lut.lut[nibble][bit_pos].data[byte] = pulse_value;
            }
        }
    }

    // Step 2: Convert to WavePulses8
    // lane0: 0xff (all bits are 1) = high nibble 0xF, low nibble 0xF
    // lane1: 0x00 (all bits are 0) = high nibble 0x0, low nibble 0x0
    uint8_t lanes[2] = {0xff, 0x00};
    uint8_t output[2 * sizeof(WavePulses8Symbol)];  // 128 bytes

    waveTranspose8_2(lanes, lut, output);

    // Step 3: Test transposed output
    // Expected: 0xAA (0b10101010) for all 128 bytes
    // - lane0 has all 1s (0xff) → all 64 pulse bytes have all bits set (11111111)
    // - lane1 has all 0s (0x00) → all 64 pulse bytes have all bits clear (00000000)
    // - Bit interleaving: [lane0_bit, lane1_bit, lane0_bit, lane1_bit, ...]
    // - Result: 0b10101010 = 0xAA for every output byte

    for (int i = 0; i < 128; i++) {
        REQUIRE(output[i] == 0xAA);
    }
}
