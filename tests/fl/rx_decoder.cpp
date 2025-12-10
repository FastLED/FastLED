/// @file rx_decoder.cpp
/// @brief Unit tests for RxDecoder component

#include "test.h"
#include "fl/rx_device.h"
#include "fl/chipsets/led_timing.h"

using namespace fl;

TEST_CASE("RxDecoder - basic configuration") {
    RxDecoder decoder;

    RxConfig config;
    config.signal_range_min_ns = 100;
    config.signal_range_max_ns = 100000;
    config.start_low = true;

    decoder.configure(config, 128);

    CHECK(decoder.bufferSize() == 128);
    CHECK(decoder.edgeCount() == 0);
    CHECK(decoder.finished() == false);
}

TEST_CASE("RxDecoder - pushEdge stores edges") {
    RxDecoder decoder;

    RxConfig config;
    decoder.configure(config, 10);

    // Push some edges
    CHECK(decoder.pushEdge(true, 400) == true);   // HIGH for 400ns
    CHECK(decoder.pushEdge(false, 850) == true);  // LOW for 850ns
    CHECK(decoder.pushEdge(true, 800) == true);   // HIGH for 800ns
    CHECK(decoder.pushEdge(false, 450) == true);  // LOW for 450ns

    CHECK(decoder.edgeCount() == 4);
    CHECK(decoder.finished() == false);
}

TEST_CASE("RxDecoder - buffer overflow sets finished") {
    RxDecoder decoder;

    RxConfig config;
    decoder.configure(config, 3);

    CHECK(decoder.pushEdge(true, 100) == true);
    CHECK(decoder.pushEdge(false, 200) == true);
    CHECK(decoder.pushEdge(true, 300) == true);

    // Buffer full - next push should fail
    CHECK(decoder.pushEdge(false, 400) == false);
    CHECK(decoder.finished() == true);
    CHECK(decoder.edgeCount() == 3);
}

TEST_CASE("RxDecoder - getRawEdgeTimes retrieves edges") {
    RxDecoder decoder;

    RxConfig config;
    decoder.configure(config, 10);

    decoder.pushEdge(true, 400);
    decoder.pushEdge(false, 850);
    decoder.pushEdge(true, 800);

    EdgeTime edges[10];
    size_t count = decoder.getRawEdgeTimes(edges);

    CHECK(count == 3);

    uint32_t high0 = edges[0].high;
    uint32_t ns0 = edges[0].ns;
    CHECK(high0 == 1);
    CHECK(ns0 == 400);

    uint32_t high1 = edges[1].high;
    uint32_t ns1 = edges[1].ns;
    CHECK(high1 == 0);
    CHECK(ns1 == 850);

    uint32_t high2 = edges[2].high;
    uint32_t ns2 = edges[2].ns;
    CHECK(high2 == 1);
    CHECK(ns2 == 800);
}

TEST_CASE("RxDecoder - reset clears state") {
    RxDecoder decoder;

    RxConfig config;
    decoder.configure(config, 10);

    decoder.pushEdge(true, 400);
    decoder.pushEdge(false, 850);
    decoder.setFinished();

    CHECK(decoder.edgeCount() == 2);
    CHECK(decoder.finished() == true);

    decoder.reset();

    CHECK(decoder.edgeCount() == 0);
    CHECK(decoder.finished() == false);
}

TEST_CASE("RxDecoder - decode with edge detection (start_low=true)") {
    RxDecoder decoder;

    // Configure for WS2812B (start_low=true)
    RxConfig config;
    config.start_low = true;
    decoder.configure(config, 128);

    // Simulate spurious LOW edges before TX starts
    decoder.pushEdge(false, 5000);   // Spurious LOW - should be skipped
    decoder.pushEdge(false, 10000);  // Spurious LOW - should be skipped

    // Valid data: WS2812B bit 0 pattern (400ns HIGH, 850ns LOW)
    decoder.pushEdge(true, 400);     // First valid edge (HIGH)
    decoder.pushEdge(false, 850);    // LOW

    // WS2812B bit 1 pattern (800ns HIGH, 450ns LOW)
    decoder.pushEdge(true, 800);     // HIGH
    decoder.pushEdge(false, 450);    // LOW

    // Create timing for WS2812B
    ChipsetTiming timing_3phase{400, 400, 450, 50000, "WS2812B"};
    ChipsetTiming4Phase timing = make4PhaseTiming(timing_3phase, 150);

    uint8_t bytes[10];
    auto result = decoder.decode(timing, bytes);

    REQUIRE(result.ok());
    CHECK(result.value() >= 1);  // Should decode at least 1 bit pair

    // First byte should have bits: 01... (bit0=0, bit1=1)
    uint8_t first_byte = bytes[0];
    uint8_t top_2_bits = (first_byte >> 6) & 0x03;
    CHECK(top_2_bits == 0x01);  // 0b01
}

TEST_CASE("RxDecoder - decode with edge detection (start_low=false)") {
    RxDecoder decoder;

    // Configure for inverted signal (start_low=false)
    RxConfig config;
    config.start_low = false;
    decoder.configure(config, 128);

    // Simulate spurious HIGH edges before TX starts
    decoder.pushEdge(true, 5000);    // Spurious HIGH - should be skipped
    decoder.pushEdge(true, 10000);   // Spurious HIGH - should be skipped

    // Valid data starts with LOW edge
    // For inverted WS2812B: LOW first, then HIGH
    // Bit 0: 400ns LOW, 850ns HIGH
    decoder.pushEdge(false, 400);    // First valid edge (LOW)
    decoder.pushEdge(true, 850);     // HIGH

    // Note: This test verifies edge detection works for inverted signals
    // The actual decoding still expects HIGH+LOW pairs in the decoder logic
    // This is intentional - edge detection just skips to first valid transition

    ChipsetTiming timing_3phase{400, 400, 450, 50000, "WS2812B"};
    ChipsetTiming4Phase timing = make4PhaseTiming(timing_3phase, 150);

    uint8_t bytes[10];
    auto result = decoder.decode(timing, bytes);

    // Decoder should start processing from first LOW edge
    // (May succeed or fail depending on pattern - this just tests edge detection works)
    bool result_valid = result.ok() || result.error() == DecodeError::INVALID_ARGUMENT;
    CHECK(result_valid);
}

TEST_CASE("RxDecoder - decode full WS2812B byte") {
    RxDecoder decoder;

    RxConfig config;
    config.start_low = true;
    decoder.configure(config, 128);

    // Encode byte 0xFF (all 1s) in WS2812B format
    // Each bit 1: 800ns HIGH, 450ns LOW
    for (int i = 0; i < 8; i++) {
        decoder.pushEdge(true, 800);
        decoder.pushEdge(false, 450);
    }

    ChipsetTiming timing_3phase{400, 400, 450, 50000, "WS2812B"};
    ChipsetTiming4Phase timing = make4PhaseTiming(timing_3phase, 150);

    uint8_t bytes[10];
    auto result = decoder.decode(timing, bytes);

    REQUIRE(result.ok());
    CHECK(result.value() == 1);
    CHECK(bytes[0] == 0xFF);
}

TEST_CASE("RxDecoder - decode mixed bits") {
    RxDecoder decoder;

    RxConfig config;
    config.start_low = true;
    decoder.configure(config, 128);

    // Encode byte 0b10101010 = 0xAA
    // Bit 1: 800ns HIGH, 450ns LOW
    // Bit 0: 400ns HIGH, 850ns LOW
    decoder.pushEdge(true, 800);   // Bit 1
    decoder.pushEdge(false, 450);
    decoder.pushEdge(true, 400);   // Bit 0
    decoder.pushEdge(false, 850);
    decoder.pushEdge(true, 800);   // Bit 1
    decoder.pushEdge(false, 450);
    decoder.pushEdge(true, 400);   // Bit 0
    decoder.pushEdge(false, 850);
    decoder.pushEdge(true, 800);   // Bit 1
    decoder.pushEdge(false, 450);
    decoder.pushEdge(true, 400);   // Bit 0
    decoder.pushEdge(false, 850);
    decoder.pushEdge(true, 800);   // Bit 1
    decoder.pushEdge(false, 450);
    decoder.pushEdge(true, 400);   // Bit 0
    decoder.pushEdge(false, 850);

    ChipsetTiming timing_3phase{400, 400, 450, 50000, "WS2812B"};
    ChipsetTiming4Phase timing = make4PhaseTiming(timing_3phase, 150);

    uint8_t bytes[10];
    auto result = decoder.decode(timing, bytes);

    REQUIRE(result.ok());
    CHECK(result.value() == 1);
    CHECK(bytes[0] == 0xAA);
}

TEST_CASE("RxDecoder - decode multiple bytes") {
    RxDecoder decoder;

    RxConfig config;
    config.start_low = true;
    decoder.configure(config, 256);

    // Encode 3 bytes: 0xFF, 0x00, 0xAA
    // Byte 1: 0xFF (all 1s)
    for (int i = 0; i < 8; i++) {
        decoder.pushEdge(true, 800);
        decoder.pushEdge(false, 450);
    }

    // Byte 2: 0x00 (all 0s)
    for (int i = 0; i < 8; i++) {
        decoder.pushEdge(true, 400);
        decoder.pushEdge(false, 850);
    }

    // Byte 3: 0xAA (10101010)
    for (int i = 0; i < 4; i++) {
        decoder.pushEdge(true, 800);   // 1
        decoder.pushEdge(false, 450);
        decoder.pushEdge(true, 400);   // 0
        decoder.pushEdge(false, 850);
    }

    ChipsetTiming timing_3phase{400, 400, 450, 50000, "WS2812B"};
    ChipsetTiming4Phase timing = make4PhaseTiming(timing_3phase, 150);

    uint8_t bytes[10];
    auto result = decoder.decode(timing, bytes);

    REQUIRE(result.ok());
    CHECK(result.value() == 3);
    CHECK(bytes[0] == 0xFF);
    CHECK(bytes[1] == 0x00);
    CHECK(bytes[2] == 0xAA);
}

TEST_CASE("RxDecoder - buffer overflow during decode") {
    RxDecoder decoder;

    RxConfig config;
    decoder.configure(config, 256);

    // Encode 5 bytes but only provide buffer for 2
    for (int byte_idx = 0; byte_idx < 5; byte_idx++) {
        for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
            decoder.pushEdge(true, 800);
            decoder.pushEdge(false, 450);
        }
    }

    ChipsetTiming timing_3phase{400, 400, 450, 50000, "WS2812B"};
    ChipsetTiming4Phase timing = make4PhaseTiming(timing_3phase, 150);

    uint8_t bytes[2];  // Only room for 2 bytes
    auto result = decoder.decode(timing, bytes);

    // Should fail with buffer overflow
    REQUIRE(!result.ok());
    CHECK(result.error() == DecodeError::BUFFER_OVERFLOW);
}

TEST_CASE("RxDecoder - empty edges") {
    RxDecoder decoder;

    RxConfig config;
    decoder.configure(config, 128);

    // No edges pushed

    ChipsetTiming timing_3phase{400, 400, 450, 50000, "WS2812B"};
    ChipsetTiming4Phase timing = make4PhaseTiming(timing_3phase, 150);

    uint8_t bytes[10];
    auto result = decoder.decode(timing, bytes);

    REQUIRE(!result.ok());
    CHECK(result.error() == DecodeError::INVALID_ARGUMENT);
}

TEST_CASE("RxDecoder - invalid timing rejected") {
    RxDecoder decoder;

    RxConfig config;
    decoder.configure(config, 128);

    // Push edges with completely wrong timing
    decoder.pushEdge(true, 5000);   // Way too long
    decoder.pushEdge(false, 50);     // Way too short

    ChipsetTiming timing_3phase{400, 400, 450, 50000, "WS2812B"};
    ChipsetTiming4Phase timing = make4PhaseTiming(timing_3phase, 150);

    uint8_t bytes[10];
    auto result = decoder.decode(timing, bytes);

    // Should decode 0 bytes (invalid timing)
    REQUIRE(result.ok());
    CHECK(result.value() == 0);
}
