/// @file test_waveform_generator.cpp
/// @brief Tests for generic waveform generator

#include "doctest.h"
#include "fl/channels/waveform_generator.h"
#include "ftl/vector.h"

using namespace fl;

TEST_CASE("generateBit0Waveform: WS2812 @ 20MHz") {
    // WS2812: T1=250ns, T2=625ns, T3=375ns at 20MHz (50ns resolution)
    uint32_t hz = 20000000;
    uint32_t t1 = 250, t2 = 625, t3 = 375;

    fl::vector_fixed<uint8_t, 64> b0_wave;
    b0_wave.resize(64);

    size_t size = generateBit0Waveform(hz, t1, t2, t3, b0_wave.data(), b0_wave.size());

    // 50ns resolution: t1=5, t2=13, t3=8 -> total=26
    CHECK(size == 26);

    // Verify pattern: 5 HIGH, 21 LOW
    for (size_t i = 0; i < 5; i++) {
        CHECK(b0_wave[i] == 0xFF);
    }
    for (size_t i = 5; i < 26; i++) {
        CHECK(b0_wave[i] == 0x00);
    }
}

TEST_CASE("generateBit1Waveform: WS2812 @ 20MHz") {
    // WS2812: T1=250ns, T2=625ns, T3=375ns at 20MHz (50ns resolution)
    uint32_t hz = 20000000;
    uint32_t t1 = 250, t2 = 625, t3 = 375;

    fl::vector_fixed<uint8_t, 64> b1_wave;
    b1_wave.resize(64);

    size_t size = generateBit1Waveform(hz, t1, t2, t3, b1_wave.data(), b1_wave.size());

    // 50ns resolution: t1=5, t2=13, t3=8 -> total=26
    CHECK(size == 26);

    // Verify pattern: 18 HIGH (t1+t2), 8 LOW (t3)
    for (size_t i = 0; i < 18; i++) {
        CHECK(b1_wave[i] == 0xFF);
    }
    for (size_t i = 18; i < 26; i++) {
        CHECK(b1_wave[i] == 0x00);
    }
}

TEST_CASE("expandByteToWaveforms: simple expansion") {
    // Generate simple 4-pulse waveforms
    uint32_t hz = 250000000;  // 250MHz -> 4ns resolution
    uint32_t t1 = 4, t2 = 8, t3 = 4;  // Will give 1+2+1 = 4 pulses

    fl::vector_fixed<uint8_t, 16> b0_wave, b1_wave;
    b0_wave.resize(16);
    b1_wave.resize(16);

    size_t b0_size = generateBit0Waveform(hz, t1, t2, t3, b0_wave.data(), b0_wave.size());
    size_t b1_size = generateBit1Waveform(hz, t1, t2, t3, b1_wave.data(), b1_wave.size());

    CHECK(b0_size == 4);
    CHECK(b1_size == 4);

    // Expand byte 0xAA (10101010)
    fl::vector_fixed<uint8_t, 64> output;
    output.resize(64);

    size_t written = expandByteToWaveforms(0xAA, b0_wave.data(), b0_size, b1_wave.data(), b1_size, output.data(), output.size());

    CHECK(written == 32);  // 8 bits * 4 pulses

    // Verify pattern (MSB first): 1,0,1,0,1,0,1,0
    for (int bitPos = 7; bitPos >= 0; bitPos--) {
        bool bitValue = (0xAA >> bitPos) & 0x01;
        size_t baseIdx = (7 - bitPos) * 4;

        const auto& expectedWave = bitValue ? b1_wave : b0_wave;
        for (size_t i = 0; i < 4; i++) {
            CHECK(output[baseIdx + i] == expectedWave[i]);
        }
    }
}

TEST_CASE("Integration: Full pipeline") {
    // Generate WS2812 waveforms at 20MHz
    uint32_t hz = 20000000;
    uint32_t t1 = 250, t2 = 625, t3 = 375;

    fl::vector_fixed<uint8_t, 64> b0_wave, b1_wave;
    b0_wave.resize(64);
    b1_wave.resize(64);

    size_t b0_size = generateBit0Waveform(hz, t1, t2, t3, b0_wave.data(), b0_wave.size());
    size_t b1_size = generateBit1Waveform(hz, t1, t2, t3, b1_wave.data(), b1_wave.size());

    REQUIRE(b0_size == 26);
    REQUIRE(b1_size == 26);

    // Expand byte 0x81 (10000001)
    fl::vector_fixed<uint8_t, 256> expanded;
    expanded.resize(256);

    size_t written = expandByteToWaveforms(0x81, b0_wave.data(), b0_size, b1_wave.data(), b1_size, expanded.data(), expanded.size());

    CHECK(written == 208);  // 8 * 26

    // Verify bit 7 (1) and bit 0 (1) use b1 pattern
    // Bits 6-1 use b0 pattern
    for (int bitPos = 7; bitPos >= 0; bitPos--) {
        bool bitValue = (0x81 >> bitPos) & 0x01;
        size_t baseIdx = (7 - bitPos) * 26;

        const auto& expectedWave = bitValue ? b1_wave : b0_wave;
        for (size_t i = 0; i < 26; i++) {
            CHECK(expanded[baseIdx + i] == expectedWave[i]);
        }
    }
}
