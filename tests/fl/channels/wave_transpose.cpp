/// @file wave_transpose.cpp
/// @brief Unit tests for waveform generation and transposition
///
/// Tests the wave transpose functionality used for multi-lane LED protocols.

#include "test.h"
#include "fl/channels/wave_transpose.h"
#include "ftl/cstring.h"

using namespace fl;


TEST_CASE("convertToWavePulses8") {

    // Step 1: Build LUT manually
    // bit 1 -> 0xff * 8 (all pulses high)
    // bit 0 -> 0x00 * 8 (all pulses low)
    Wave8BitExpansionLut lut;

    // Initialize bit 0 pattern (all zeros)
    for (int i = 0; i < 8; i++) {
        lut.lut[0].data[i] = 0x00;
    }

    // Initialize bit 1 pattern (all ones)
    for (int i = 0; i < 8; i++) {
        lut.lut[1].data[i] = 0xff;
    }

    // Step 2: Convert to WavePulses8
    // lane0: 0xff (all bits are 1)
    // lane1: 0x00 (all bits are 0)
    uint8_t lanes[2] = {0xff, 0x00};
    uint8_t output[16];

    waveTranspose8_2(lanes, lut, output);

    // Step 3: Test transposed output
    // Expected: alternating bits 0b10101010 (lane0=1, lane1=0, lane0=1, lane1=0, ...)
    // Since all bits in lane0 are 1 (0xff) and all bits in lane1 are 0 (0x00),
    // the transposed output should have the pattern 0xAA (0b10101010) in every byte

    for (int i = 0; i < 16; i++) {
        REQUIRE(output[i] == 0xAA);
    }
}
