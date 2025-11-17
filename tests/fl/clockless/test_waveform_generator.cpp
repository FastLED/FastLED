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

    size_t size = generateBit0Waveform(hz, t1, t2, t3, b0_wave);

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

    size_t size = generateBit1Waveform(hz, t1, t2, t3, b1_wave);

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

TEST_CASE("generateWaveforms: convenience function") {
    uint32_t hz = 20000000;
    uint32_t t1 = 250, t2 = 625, t3 = 375;

    fl::vector_fixed<uint8_t, 64> b0_wave, b1_wave;
    b0_wave.resize(64);
    b1_wave.resize(64);

    size_t b0_size, b1_size;
    bool success = generateWaveforms(hz, t1, t2, t3, b0_wave, b0_size, b1_wave, b1_size);

    CHECK(success);
    CHECK(b0_size == 26);
    CHECK(b1_size == 26);

    // Verify they're different
    bool different = false;
    for (size_t i = 0; i < 26; i++) {
        if (b0_wave[i] != b1_wave[i]) {
            different = true;
            break;
        }
    }
    CHECK(different);
}

TEST_CASE("generateWaveforms: buffer too small error") {
    uint32_t hz = 20000000;
    uint32_t t1 = 250, t2 = 625, t3 = 375;

    // Create buffers that are too small (need 26, provide 10)
    fl::vector_fixed<uint8_t, 10> b0_wave, b1_wave;
    b0_wave.resize(10);
    b1_wave.resize(10);

    size_t b0_size, b1_size;
    bool success = generateWaveforms(hz, t1, t2, t3, b0_wave, b0_size, b1_wave, b1_size);

    CHECK_FALSE(success);
    CHECK(b0_size == 0);
    CHECK(b1_size == 0);
}

TEST_CASE("generateWaveforms: invalid inputs error") {
    fl::vector_fixed<uint8_t, 64> b0_wave, b1_wave;
    b0_wave.resize(64);
    b1_wave.resize(64);

    size_t b0_size, b1_size;

    // Zero frequency
    bool success = generateWaveforms(0, 250, 625, 375, b0_wave, b0_size, b1_wave, b1_size);
    CHECK_FALSE(success);

    // Zero timing
    success = generateWaveforms(20000000, 0, 625, 375, b0_wave, b0_size, b1_wave, b1_size);
    CHECK_FALSE(success);
}

TEST_CASE("expandByteToWaveforms: simple expansion") {
    // Generate simple 4-pulse waveforms
    uint32_t hz = 250000000;  // 250MHz -> 4ns resolution
    uint32_t t1 = 4, t2 = 8, t3 = 4;  // Will give 1+2+1 = 4 pulses

    fl::vector_fixed<uint8_t, 16> b0_wave, b1_wave;
    b0_wave.resize(16);
    b1_wave.resize(16);

    size_t b0_size = generateBit0Waveform(hz, t1, t2, t3, b0_wave);
    size_t b1_size = generateBit1Waveform(hz, t1, t2, t3, b1_wave);

    CHECK(b0_size == 4);
    CHECK(b1_size == 4);

    // Expand byte 0xAA (10101010)
    fl::vector_fixed<uint8_t, 64> output;
    output.resize(64);

    fl::span<const uint8_t> b0_span(b0_wave.data(), b0_size);
    fl::span<const uint8_t> b1_span(b1_wave.data(), b1_size);

    size_t written = expandByteToWaveforms(0xAA, b0_span, b1_span, output);

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

TEST_CASE("transposeLanes: 2 lanes simple") {
    fl::vector_fixed<uint8_t, 8> lane0 = {0x01, 0x02, 0x03};
    fl::vector_fixed<uint8_t, 8> lane1 = {0x04, 0x05, 0x06};

    fl::span<const uint8_t> laneData[2] = {lane0, lane1};

    fl::vector_fixed<uint8_t, 16> output;
    output.resize(6);

    size_t written = transposeLanes(laneData, 2, 3, output);

    CHECK(written == 6);
    // Expected: [L0[0], L1[0], L0[1], L1[1], L0[2], L1[2]]
    CHECK(output[0] == 0x01);
    CHECK(output[1] == 0x04);
    CHECK(output[2] == 0x02);
    CHECK(output[3] == 0x05);
    CHECK(output[4] == 0x03);
    CHECK(output[5] == 0x06);
}

TEST_CASE("transpose8Lanes: optimized algorithm") {
    fl::vector_fixed<uint8_t, 8> laneData[8];
    for (int i = 0; i < 8; i++) {
        laneData[i].push_back(static_cast<uint8_t>(1 << i));
    }

    fl::span<const uint8_t> laneSpans[8];
    for (int i = 0; i < 8; i++) {
        laneSpans[i] = laneData[i];
    }

    fl::vector_fixed<uint8_t, 8> output;
    output.resize(8);

    transpose8Lanes(laneSpans, 0, output);

    // Each output byte should contain the same bit position from all lanes
    CHECK(output[0] == 0x01);  // Bit 0 from all lanes
    CHECK(output[1] == 0x02);  // Bit 1 from all lanes
    CHECK(output[2] == 0x04);
    CHECK(output[3] == 0x08);
    CHECK(output[4] == 0x10);
    CHECK(output[5] == 0x20);
    CHECK(output[6] == 0x40);
    CHECK(output[7] == 0x80);
}

TEST_CASE("ISRState: init and reset") {
    ISRState state;

    // WS2812 @ 20MHz
    state.init(20000000, 250, 625, 375, 4, 100, 25);

    CHECK(state.numLanes == 4);
    CHECK(state.bytesPerLane == 100);
    CHECK(state.segmentSize == 25);
    CHECK(state.pulsesPerBit == 26);
    CHECK(state.totalSegments == 4);
    CHECK(state.currentSegment == 0);
    CHECK_FALSE(state.isComplete);

    state.currentSegment = 2;
    state.isComplete = true;

    state.reset();
    CHECK(state.currentSegment == 0);
    CHECK_FALSE(state.isComplete);
}

TEST_CASE("processNextSegment: basic operation") {
    ISRState state;

    // WS2812 @ 20MHz: will generate 26 pulse waveforms
    state.init(20000000, 250, 625, 375, 2, 4, 2);

    // Setup source data
    fl::vector_fixed<uint8_t, 8> lane0 = {0xFF, 0x00, 0xAA, 0x55};
    fl::vector_fixed<uint8_t, 8> lane1 = {0x11, 0x22, 0x33, 0x44};

    state.laneDataSources[0] = lane0;
    state.laneDataSources[1] = lane1;

    // Allocate working buffers
    // 2 bytes * 8 bits * 26 pulses = 416 bytes per lane per segment
    fl::vector_fixed<uint8_t, 512> lane0Wave, lane1Wave;
    lane0Wave.resize(416);
    lane1Wave.resize(416);
    state.laneWaveformBuffers[0] = lane0Wave;
    state.laneWaveformBuffers[1] = lane1Wave;

    // Allocate DMA buffer (416 * 2 lanes)
    fl::vector_fixed<uint8_t, 1024> dma;
    dma.resize(832);
    state.dmaBuffer = dma;

    // Process first segment
    bool hasMore = processNextSegment(state);
    CHECK(hasMore);
    CHECK(state.currentSegment == 1);
    CHECK_FALSE(state.isComplete);

    // Process second segment
    hasMore = processNextSegment(state);
    CHECK(hasMore);
    CHECK(state.currentSegment == 2);
    CHECK(state.isComplete);

    // No more segments
    hasMore = processNextSegment(state);
    CHECK_FALSE(hasMore);
}

TEST_CASE("Integration: Full pipeline") {
    // Generate WS2812 waveforms at 20MHz
    uint32_t hz = 20000000;
    uint32_t t1 = 250, t2 = 625, t3 = 375;

    fl::vector_fixed<uint8_t, 64> b0_wave, b1_wave;
    b0_wave.resize(64);
    b1_wave.resize(64);

    size_t b0_size, b1_size;
    bool success = generateWaveforms(hz, t1, t2, t3, b0_wave, b0_size, b1_wave, b1_size);

    REQUIRE(success);
    REQUIRE(b0_size == 26);
    REQUIRE(b1_size == 26);

    // Expand byte 0x81 (10000001)
    fl::vector_fixed<uint8_t, 256> expanded;
    expanded.resize(256);

    fl::span<const uint8_t> b0_span(b0_wave.data(), b0_size);
    fl::span<const uint8_t> b1_span(b1_wave.data(), b1_size);

    size_t written = expandByteToWaveforms(0x81, b0_span, b1_span, expanded);

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
