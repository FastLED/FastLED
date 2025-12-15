/// @file wave_transpose.cpp
/// @brief Unit tests for waveform generation and transposition
///
/// Tests the wave transpose functionality used for multi-lane LED protocols.

#include "fl/channels/wave_transpose.h"
#include "ftl/cstring.h"
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
    for (int i = 0; i < 6; i++) {
        REQUIRE(lut.lut[nibble][0].data[i] == 0xFF);
    }
    for (int i = 6; i < 8; i++) {
        REQUIRE(lut.lut[nibble][0].data[i] == 0x00);
    }

    // Check bit 2 (value=0) -> should use bit0 waveform (2 HIGH, 6 LOW)
    for (int i = 0; i < 2; i++) {
        REQUIRE(lut.lut[nibble][1].data[i] == 0xFF);
    }
    for (int i = 2; i < 8; i++) {
        REQUIRE(lut.lut[nibble][1].data[i] == 0x00);
    }

    // Check bit 1 (value=1) -> should use bit1 waveform (6 HIGH, 2 LOW)
    for (int i = 0; i < 6; i++) {
        REQUIRE(lut.lut[nibble][2].data[i] == 0xFF);
    }
    for (int i = 6; i < 8; i++) {
        REQUIRE(lut.lut[nibble][2].data[i] == 0x00);
    }

    // Check bit 0 (LSB, value=0) -> should use bit0 waveform (2 HIGH, 6 LOW)
    for (int i = 0; i < 2; i++) {
        REQUIRE(lut.lut[nibble][3].data[i] == 0xFF);
    }
    for (int i = 2; i < 8; i++) {
        REQUIRE(lut.lut[nibble][3].data[i] == 0x00);
    }
}

TEST_CASE("convertToWavePulses8") {
    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    // This creates simple patterns for testing transpose
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Convert to WavePulses8
    // lane0: 0xff (all bits are 1) = high nibble 0xF, low nibble 0xF
    // lane1: 0x00 (all bits are 0) = high nibble 0x0, low nibble 0x0
    uint8_t lanes[2] = {0xff, 0x00};
    uint8_t output[2 * sizeof(WavePulses8Symbol)]; // 128 bytes

    waveTranspose8_2(lanes, lut, output);

    // Test transposed output
    // Expected: 0xAA (0b10101010) for all 128 bytes
    // - lane0 has all 1s (0xff) → all 64 pulse bytes have all bits set
    // (11111111)
    // - lane1 has all 0s (0x00) → all 64 pulse bytes have all bits clear
    // (00000000)
    // - Bit interleaving: [lane0_bit, lane1_bit, lane0_bit, lane1_bit, ...]
    // - Result: 0b10101010 = 0xAA for every output byte

    for (int i = 0; i < 128; i++) {
        REQUIRE(output[i] == 0xAA);
    }
}
