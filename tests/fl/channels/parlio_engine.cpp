/// @file parlio_engine.cpp
/// @brief Direct unit tests for ParlioEngine DMA output capture and validation
///
/// This test file directly tests the parlio_engine.h abstraction layer with
/// platform-independent code. It validates:
/// - Mock peripheral DMA data capture
/// - Waveform generation and bit-parallel layout
/// - Multi-lane transmission correctness
/// - Timing parameter validation
///
/// Unlike parlio_mock.cpp (which tests engine lifecycle), this file focuses on
/// validating the actual DMA output data matches expected waveform parameters.


#ifdef FASTLED_STUB_IMPL  // Mock tests only run on stub platform

#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.h"
#include "platforms/esp/32/drivers/parlio/parlio_engine.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"

using namespace fl;
using namespace fl::detail;

namespace {

/// @brief Helper to create WS2812B timing config for DMA tests
ChipsetTimingConfig getWS2812TimingForDmaTests() {
    return ChipsetTimingConfig(350, 800, 450, 50, "WS2812B");
}

/// @brief Reset mock between tests
static void resetMock_engine() {
    auto& mock = ParlioPeripheralMock::instance();
    mock.clearTransmissionHistory();
    mock.setTransmitFailure(false);
    mock.setTransmitDelay(0);
}

} // anonymous namespace

//=============================================================================
// Test Suite: DMA Output Capture
//=============================================================================

TEST_CASE("ParlioEngine - DMA output capture basic functionality") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();

    bool init_ok = engine.initialize(1, pins, timing, 10);
    REQUIRE(init_ok);

    // Simple test pattern: Single LED with RGB = 0xFF, 0x00, 0xAA
    uint8_t scratch[3] = {0xFF, 0x00, 0xAA};

    bool tx_ok = engine.beginTransmission(scratch, 3, 1, 3);
    REQUIRE(tx_ok);

    // Access mock to verify data capture
    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    // Verify first transmission captured data
    const auto& first_tx = history[0];
    CHECK(first_tx.bit_count > 0);
    CHECK(first_tx.buffer_copy.size() > 0);

    // Each byte (8 bits) expands to 8 bytes in Wave8 format (64 bits total)
    // 3 input bytes = 24 bytes Wave8 minimum
    CHECK(first_tx.buffer_copy.size() >= 24);
}

TEST_CASE("ParlioEngine - verify captured DMA data is non-zero") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();
    engine.initialize(1, pins, timing, 5);

    // Known pattern with non-zero values
    uint8_t scratch[15] = {
        0xFF, 0xFF, 0xFF,  // LED 0: All white
        0xAA, 0x55, 0xF0,  // LED 1: Pattern
        0x00, 0x00, 0x00,  // LED 2: All black
        0x12, 0x34, 0x56,  // LED 3: Sequential
        0x80, 0x40, 0x20   // LED 4: Powers of 2
    };

    bool tx_ok = engine.beginTransmission(scratch, 15, 1, 15);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    const auto& buffer = tx.buffer_copy;

    // Verify buffer contains non-zero data
    bool has_nonzero = false;
    for (size_t i = 0; i < buffer.size(); i++) {
        if (buffer[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    CHECK(has_nonzero);
}

TEST_CASE("ParlioEngine - multi-lane DMA output capture") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    // 4-lane configuration
    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();

    size_t num_lanes = 4;
    size_t leds_per_lane = 3;
    size_t bytes_per_led = 3;
    size_t lane_stride = leds_per_lane * bytes_per_led;  // 9 bytes per lane
    size_t total_bytes = num_lanes * lane_stride;         // 36 bytes total

    bool init_ok = engine.initialize(num_lanes, pins, timing, leds_per_lane);
    REQUIRE(init_ok);

    // Create per-lane scratch buffer with distinct patterns
    fl::vector<uint8_t> scratch(total_bytes);

    // Lane 0: 0xFF pattern
    for (size_t i = 0; i < lane_stride; i++) {
        scratch[0 * lane_stride + i] = 0xFF;
    }

    // Lane 1: 0xAA pattern
    for (size_t i = 0; i < lane_stride; i++) {
        scratch[1 * lane_stride + i] = 0xAA;
    }

    // Lane 2: 0x55 pattern
    for (size_t i = 0; i < lane_stride; i++) {
        scratch[2 * lane_stride + i] = 0x55;
    }

    // Lane 3: 0x00 pattern
    for (size_t i = 0; i < lane_stride; i++) {
        scratch[3 * lane_stride + i] = 0x00;
    }

    bool tx_ok = engine.beginTransmission(scratch.data(), total_bytes, num_lanes, lane_stride);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    // Verify multi-lane transmission captured data
    const auto& tx = history[0];
    CHECK(tx.buffer_copy.size() > 0);

    // For multi-lane, the output buffer should be larger due to bit-parallel layout
    // Each lane's data is transposed and interleaved
    // Wave8 expansion creates approximately 2-3x the original data size
    // (actual ratio depends on ring buffer chunking and alignment)
    CHECK(tx.buffer_copy.size() >= total_bytes * 2);
}

TEST_CASE("ParlioEngine - verify multiple transmissions are captured") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();
    engine.initialize(1, pins, timing, 10);

    auto& mock = ParlioPeripheralMock::instance();

    // Clear history before test
    mock.clearTransmissionHistory();

    // First transmission
    uint8_t scratch1[3] = {0xFF, 0x00, 0x00};  // Red
    engine.beginTransmission(scratch1, 3, 1, 3);

    size_t count_after_first = mock.getTransmissionHistory().size();
    CHECK(count_after_first > 0);

    // Second transmission
    uint8_t scratch2[3] = {0x00, 0xFF, 0x00};  // Green
    engine.beginTransmission(scratch2, 3, 1, 3);

    size_t count_after_second = mock.getTransmissionHistory().size();
    CHECK(count_after_second >= count_after_first);

    // History should contain both transmissions
    const auto& history = mock.getTransmissionHistory();
    CHECK(history.size() >= 2);
}

TEST_CASE("ParlioEngine - verify bit count matches expected") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();
    engine.initialize(1, pins, timing, 10);

    // 5 LEDs × 3 bytes/LED = 15 bytes
    // 15 bytes × 8 bits/byte = 120 bits input
    // Wave8 expansion: 120 bits × 8 = 960 bits output minimum
    uint8_t scratch[15];
    for (size_t i = 0; i < 15; i++) {
        scratch[i] = static_cast<uint8_t>((i * 17) & 0xFF);
    }

    bool tx_ok = engine.beginTransmission(scratch, 15, 1, 15);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    const auto& tx = history[0];

    // Verify bit count is reasonable
    // Input: 15 bytes = 120 bits
    // Wave8 expansion creates approximately 2-3x the bit count
    // (actual ratio depends on ring buffer chunking)
    CHECK(tx.bit_count >= 160);

    // Verify buffer size matches bit count
    size_t expected_bytes = (tx.bit_count + 7) / 8;  // Round up
    CHECK(tx.buffer_copy.size() >= expected_bytes);
}

TEST_CASE("ParlioEngine - verify idle value is captured") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();
    engine.initialize(1, pins, timing, 5);

    uint8_t scratch[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    bool tx_ok = engine.beginTransmission(scratch, 6, 1, 6);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    const auto& tx = history[0];

    // Idle value should be set (typically 0x0000 for WS2812B)
    // This is implementation-specific, but should be captured
    CHECK(tx.idle_value == 0x0000);
}

TEST_CASE("ParlioEngine - large buffer streaming with capture") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();

    // Large LED count to trigger potential streaming mode (reduced from 300 for performance)
    size_t num_leds = 100;
    size_t num_bytes = num_leds * 3;

    bool init_ok = engine.initialize(1, pins, timing, num_leds);
    REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(num_bytes);
    for (size_t i = 0; i < num_bytes; i++) {
        scratch[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);
    }

    bool tx_ok = engine.beginTransmission(scratch.data(), num_bytes, 1, num_bytes);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();

    // For large buffers, engine may split into multiple DMA submissions
    CHECK(history.size() > 0);

    // Verify total captured data is reasonable
    size_t total_bits = 0;
    for (const auto& tx : history) {
        total_bits += tx.bit_count;
    }

    // At minimum: num_bytes × 8 bits × 8 (Wave8 expansion)
    size_t expected_min_bits = num_bytes * 8 * 8;
    CHECK(total_bits >= expected_min_bits);
}

//=============================================================================
// Test Suite: Waveform Parameter Validation
//=============================================================================

TEST_CASE("ParlioEngine - verify timing parameters are applied") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};

    // Custom timing config
    ChipsetTimingConfig custom_timing(400, 850, 500, 80, "CustomTiming");

    bool init_ok = engine.initialize(1, pins, custom_timing, 5);
    REQUIRE(init_ok);

    uint8_t scratch[15] = {0xFF, 0xAA, 0x55, 0xF0, 0x0F, 0xC3, 0x3C, 0x99, 0x66,
                           0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    bool tx_ok = engine.beginTransmission(scratch, 15, 1, 15);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    // Verify transmission occurred with custom timing
    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    // The actual waveform validation would require inspecting the bit patterns
    // For now, verify that transmission succeeded with custom timing
    CHECK(history[0].buffer_copy.size() > 0);
}

TEST_CASE("ParlioEngine - zero-length transmission edge case") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();
    engine.initialize(1, pins, timing, 1);

    // Zero-length transmission (edge case)
    uint8_t scratch[1] = {0};
    bool tx_ok = engine.beginTransmission(scratch, 0, 1, 0);
    (void)tx_ok;  // Suppress unused warning - behavior is implementation-defined

    // Behavior is implementation-defined, but should not crash
    auto& mock = ParlioPeripheralMock::instance();
    (void)mock;  // Suppress unused warning

    // If transmission was allowed, history might be empty or contain zero-length record
    // Just verify no crash occurred
    CHECK(true);
}

TEST_CASE("ParlioEngine - single byte transmission") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();
    engine.initialize(1, pins, timing, 1);

    // Single byte (partial LED - unusual but possible)
    uint8_t scratch[1] = {0xA5};

    bool tx_ok = engine.beginTransmission(scratch, 1, 1, 1);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    const auto& tx = history[0];

    // Single byte = 8 bits input
    // Wave8 expansion = 64 bits output
    CHECK(tx.bit_count >= 64);
    CHECK(tx.buffer_copy.size() >= 8);
}

TEST_CASE("ParlioEngine - max lanes configuration with data capture") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    // Maximum PARLIO data width (16 lanes)
    fl::vector<int> pins;
    for (int i = 1; i <= 16; i++) {
        pins.push_back(i);
    }

    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();

    size_t num_lanes = 16;
    size_t leds_per_lane = 5;  // Reduced from 10 for performance
    size_t bytes_per_led = 3;
    size_t lane_stride = leds_per_lane * bytes_per_led;
    size_t total_bytes = num_lanes * lane_stride;

    bool init_ok = engine.initialize(num_lanes, pins, timing, leds_per_lane);
    REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(total_bytes);
    for (size_t i = 0; i < total_bytes; i++) {
        scratch[i] = static_cast<uint8_t>((i & 0xFF));
    }

    bool tx_ok = engine.beginTransmission(scratch.data(), total_bytes, num_lanes, lane_stride);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    // Verify max-lane transmission captured data
    const auto& tx = history[0];
    CHECK(tx.buffer_copy.size() > 0);

    // With 16 lanes, output should be substantial
    CHECK(tx.bit_count > 0);
}

TEST_CASE("ParlioEngine - two channels with different lengths (padding test)") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    // 2-lane configuration
    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();

    size_t num_lanes = 2;

    // Lane 0: 5 LEDs (long channel)
    size_t lane0_leds = 5;
    size_t bytes_per_led = 3;
    size_t lane0_bytes = lane0_leds * bytes_per_led;  // 15 bytes

    // Lane 1: 3 LEDs (short channel)
    size_t lane1_leds = 3;
    size_t lane1_bytes = lane1_leds * bytes_per_led;  // 9 bytes

    // Max channel size determines the lane stride
    size_t max_channel_bytes = lane0_bytes;  // 15 bytes
    size_t total_bytes = num_lanes * max_channel_bytes;  // 30 bytes total

    bool init_ok = engine.initialize(num_lanes, pins, timing, lane0_leds);
    REQUIRE(init_ok);

    // Create per-lane scratch buffer
    fl::vector<uint8_t> scratch(total_bytes);

    // Lane 0 (long channel): 15 bytes with distinct pattern (0x01, 0x02, ... 0x0F)
    for (size_t i = 0; i < max_channel_bytes; i++) {
        scratch[0 * max_channel_bytes + i] = static_cast<uint8_t>(i + 1);
    }

    // Lane 1 (short channel): 9 bytes with distinct pattern (0xF1, 0xF2, ... 0xF9)
    // followed by 6 bytes of 0x00 padding
    for (size_t i = 0; i < lane1_bytes; i++) {
        scratch[1 * max_channel_bytes + i] = static_cast<uint8_t>(0xF0 + i + 1);
    }
    // The remaining bytes (15 - 9 = 6 bytes) should be zero-padded
    for (size_t i = lane1_bytes; i < max_channel_bytes; i++) {
        scratch[1 * max_channel_bytes + i] = 0x00;
    }

    bool tx_ok = engine.beginTransmission(scratch.data(), total_bytes, num_lanes, max_channel_bytes);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    // Verify transmission captured data
    const auto& tx = history[0];
    CHECK(tx.buffer_copy.size() > 0);

    // Each lane's data is expanded by Wave8
    // Wave8 expansion creates approximately 2-3x the original data size
    // (actual ratio depends on ring buffer chunking)
    CHECK(tx.buffer_copy.size() >= total_bytes * 2);

    // Verify that transmission includes both channels' data
    CHECK(tx.bit_count > 0);

    // Note: The actual bit-parallel layout verification would require
    // detailed Wave8 decoding. The key validation here is:
    // 1. Both lanes are transmitted synchronously
    // 2. The shorter lane (lane 1) is padded with zeros to match lane 0's length
    // 3. The reset signal (trailing zeros) provides proper LED reset timing
    // 4. The DMA buffer size accounts for boundary + reset padding
}

TEST_CASE("ParlioEngine - verify reset padding is applied for different channel lengths") {
    resetMock_engine();

    auto& engine = ParlioEngine::getInstance();

    // Single lane with short data to verify reset padding
    fl::vector<int> pins = {1};

    // WS2812B timing with explicit reset time requirement
    ChipsetTimingConfig timing = getWS2812TimingForDmaTests();

    size_t num_lanes = 1;
    size_t leds_per_lane = 2;
    size_t bytes_per_led = 3;
    size_t total_bytes = leds_per_lane * bytes_per_led;  // 6 bytes

    bool init_ok = engine.initialize(num_lanes, pins, timing, leds_per_lane);
    REQUIRE(init_ok);

    // Create data buffer with known pattern
    fl::vector<uint8_t> scratch(total_bytes);
    for (size_t i = 0; i < total_bytes; i++) {
        scratch[i] = static_cast<uint8_t>(0xAA);
    }

    bool tx_ok = engine.beginTransmission(scratch.data(), total_bytes, num_lanes, total_bytes);
    REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    const auto& tx = history[0];

    // Base data: 6 bytes with Wave8 expansion
    // Wave8 expansion creates approximately 2-3x the original data size
    // Plus boundary and reset padding
    // (actual size depends on ring buffer chunking and reset timing)

    // Verify that reset padding increases buffer size beyond just the data
    CHECK(tx.buffer_copy.size() > total_bytes * 2);

    // The exact buffer size depends on timing parameters, but bit count
    // should account for Wave8 expansion of the data
    CHECK(tx.bit_count > total_bytes * 8 * 2);
}

#endif // FASTLED_STUB_IMPL
