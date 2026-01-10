/// @file parlio_mock.cpp
/// @brief Mock-based unit tests for ParlioEngine
///
/// Tests ParlioEngine behavior using the mock peripheral implementation.
/// Unlike parlio.cpp (which tests encoding logic), this file tests:
/// - Engine initialization and configuration
/// - Transmission lifecycle management
/// - ISR callback coordination
/// - Ring buffer streaming
/// - Error handling and injection
///
/// These tests run ONLY on stub platforms (host-based testing).


#ifdef FASTLED_STUB_IMPL  // Mock tests only run on stub platform

#include "platforms/shared/mock/esp/32/drivers/parlio_peripheral_mock.h"
#include "platforms/esp/32/drivers/parlio/parlio_engine.h"
#include <stddef.h>
#include <stdint.h>
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/channels/wave8.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/slice.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/unordered_map.h"
#include "led_sysdefs_stub_generic.h"
#include "platforms/esp/32/drivers/parlio/iparlio_peripheral.h"

using namespace fl;
using namespace fl::detail;

namespace {

/// @brief Helper to create default timing config for WS2812
static ChipsetTimingConfig getWS2812Timing_mock() {
    return ChipsetTimingConfig(350, 800, 450, 50, "WS2812B");
}

/// @brief Reset mock state between tests
void resetMockState() {
    auto& mock = ParlioPeripheralMock::instance();
    {
        // Full reset instead of partial - resets all state including initialization
        mock.reset();
    }
}

} // anonymous namespace

//=============================================================================
// Test Suite: ParlioEngine Mock Initialization
//=============================================================================

TEST_CASE("ParlioEngine mock - basic initialization") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    // Single lane configuration
    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    bool success = engine.initialize(1, pins, timing, 10);
    CHECK(success);

    // Verify mock received correct config
    auto& mock = ParlioPeripheralMock::instance();
    CHECK(mock.isInitialized());
    CHECK(mock.getConfig().data_width == 1);
    CHECK(mock.getConfig().gpio_pins[0] == 1);
    CHECK(mock.getConfig().gpio_pins[1] == -1);  // Unused lanes marked -1
}

TEST_CASE("ParlioEngine mock - two-lane initialization") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    // Two lane configuration
    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    bool success = engine.initialize(2, pins, timing, 100);
    CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    CHECK(mock.isInitialized());

    const auto& config = mock.getConfig();
    CHECK(config.data_width == 2);
    CHECK(config.gpio_pins[0] == 1);
    CHECK(config.gpio_pins[1] == 2);
    CHECK(config.gpio_pins[2] == -1);  // Unused lanes
}

TEST_CASE("ParlioEngine mock - multi-lane initialization") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    // Four lane configuration
    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    bool success = engine.initialize(4, pins, timing, 100);
    CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    CHECK(mock.isInitialized());

    const auto& config = mock.getConfig();
    CHECK(config.data_width == 4);
    CHECK(config.gpio_pins[0] == 1);
    CHECK(config.gpio_pins[1] == 2);
    CHECK(config.gpio_pins[2] == 4);
    CHECK(config.gpio_pins[3] == 8);
    CHECK(config.gpio_pins[4] == -1);  // Unused lanes
}

//=============================================================================
// Test Suite: Basic Transmission
//=============================================================================

TEST_CASE("ParlioEngine mock - single LED transmission") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();
    engine.initialize(1, pins, timing, 1);

    // Single LED: RGB = 3 bytes
    uint8_t scratch[3] = {0xFF, 0x00, 0xAA};

    bool success = engine.beginTransmission(scratch, 3, 1, 3);
    CHECK(success);

    // Verify mock recorded transmission
    auto& mock = ParlioPeripheralMock::instance();

    // Verify peripheral was enabled
    CHECK(mock.isEnabled());

    // Verify at least one transmission occurred
    CHECK(mock.getTransmitCount() > 0);

    // Check transmission history
    const auto& history = mock.getTransmissionHistory();
    CHECK(history.size() > 0);

    if (history.size() > 0) {
        // First transmission should have non-zero data
        CHECK(history[0].bit_count > 0);
        CHECK(history[0].buffer_copy.size() > 0);
    }
}

TEST_CASE("ParlioEngine mock - multiple LEDs transmission") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    // 10 LEDs = 30 bytes
    size_t num_leds = 10;
    engine.initialize(1, pins, timing, num_leds);

    fl::vector<uint8_t> scratch(num_leds * 3);
    for (size_t i = 0; i < scratch.size(); i++) {
        scratch[i] = static_cast<uint8_t>(i & 0xFF);
    }

    bool success = engine.beginTransmission(scratch.data(), scratch.size(), 1, scratch.size());
    CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    CHECK(mock.getTransmitCount() > 0);

    // Poll until transmission completes (async execution) - reduced from 5000 to 200 for performance
    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = engine.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);  // Minimal delay to allow background thread to progress
        }
    }
    CHECK(state == ParlioEngineState::READY);
}

TEST_CASE("ParlioEngine mock - two-lane transmission") {
    // Clear mock transmission history (don't fully reset - engine may be initialized)
    auto& mock = ParlioPeripheralMock::instance();
    mock.clearTransmissionHistory();

    auto& engine = ParlioEngine::getInstance();

    // Two lane configuration
    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    // 10 LEDs per lane = 60 bytes total (2 lanes × 10 LEDs × 3 bytes/LED)
    size_t num_leds = 10;
    size_t num_lanes = 2;

    // Initialize engine (may already be initialized - that's OK for transmission test)
    bool init_result = engine.initialize(num_lanes, pins, timing, num_leds);
    REQUIRE(init_result);  // Ensure initialization succeeded

    // Prepare scratch buffer with per-lane layout
    // [lane0_data (30 bytes)][lane1_data (30 bytes)]
    fl::vector<uint8_t> scratch(num_leds * num_lanes * 3);
    for (size_t lane = 0; lane < num_lanes; lane++) {
        for (size_t led = 0; led < num_leds; led++) {
            size_t base_idx = lane * num_leds * 3 + led * 3;
            scratch[base_idx + 0] = static_cast<uint8_t>((lane * 100 + led) & 0xFF);  // R
            scratch[base_idx + 1] = static_cast<uint8_t>((lane * 50 + led) & 0xFF);   // G
            scratch[base_idx + 2] = static_cast<uint8_t>((lane * 25 + led) & 0xFF);   // B
        }
    }

    size_t lane_stride = num_leds * 3;  // 30 bytes per lane
    bool success = engine.beginTransmission(scratch.data(), scratch.size(), num_lanes, lane_stride);
    CHECK(success);

    // mock already declared at top of test
    CHECK(mock.getTransmitCount() > 0);

    // Poll until transmission completes (async execution via stub timer thread) - reduced from 5000 to 200 for performance
    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = engine.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);  // Minimal delay to allow background thread to progress
        }
    }
    CHECK(state == ParlioEngineState::READY);

    // Verify mock recorded transmissions
    const auto& history = mock.getTransmissionHistory();
    CHECK(history.size() > 0);

    if (history.size() > 0) {
        // Verify first transmission has non-zero data
        CHECK(history[0].bit_count > 0);
        CHECK(history[0].buffer_copy.size() > 0);
    }
}

//=============================================================================
// Test Suite: ISR Simulation
//=============================================================================

TEST_CASE("ParlioEngine mock - ISR callback simulation") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();
    engine.initialize(1, pins, timing, 10);

    uint8_t scratch[30];
    for (size_t i = 0; i < 30; i++) {
        scratch[i] = static_cast<uint8_t>(i);
    }

    // Start transmission
    bool success = engine.beginTransmission(scratch, 30, 1, 30);
    CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();

    // At this point, transmission should be in progress or complete
    // The mock should have recorded transmissions
    size_t initial_count = mock.getTransmitCount();
    CHECK(initial_count > 0);

    // Poll engine status - should be READY since transmission is synchronous
    ParlioEngineState state = engine.poll();
    CHECK(state == ParlioEngineState::READY);
}

//=============================================================================
// Test Suite: Error Injection
//=============================================================================

TEST_CASE("ParlioEngine mock - transmit failure injection") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();
    engine.initialize(1, pins, timing, 10);

    uint8_t scratch[30];

    auto& mock = ParlioPeripheralMock::instance();

    // Inject transmit failure
    mock.setTransmitFailure(true);

    bool success = engine.beginTransmission(scratch, 30, 1, 30);
    CHECK_FALSE(success);  // Should fail

    // Verify engine detected error
    CHECK(engine.poll() == ParlioEngineState::ERROR);

    // Clear failure and reinitialize for next transmission
    mock.setTransmitFailure(false);

    // Note: After error, engine might need reinitialization
    // This tests error detection, not recovery
}

//=============================================================================
// Test Suite: Ring Buffer Streaming
//=============================================================================

TEST_CASE("ParlioEngine mock - large buffer streaming") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    // Use a large LED count to potentially trigger streaming mode
    // (actual streaming depends on buffer size limits)
    size_t num_leds = 500;
    engine.initialize(1, pins, timing, num_leds);

    fl::vector<uint8_t> scratch(num_leds * 3);
    for (size_t i = 0; i < scratch.size(); i++) {
        scratch[i] = static_cast<uint8_t>(i & 0xFF);
    }

    bool success = engine.beginTransmission(scratch.data(), scratch.size(), 1, scratch.size());
    CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();

    // Large transmissions may require multiple DMA buffer submissions
    // Verify at least one transmission occurred
    CHECK(mock.getTransmitCount() > 0);

    // Poll until transmission completes (reduced from 1000 to 200 for performance)
    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = engine.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);  // Minimal delay to allow background thread to progress
        }
    }
    CHECK(state == ParlioEngineState::READY);
}

TEST_CASE("ParlioEngine mock - multi-lane streaming") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    // Test with 4 lanes
    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    size_t num_leds = 200;  // 200 LEDs per lane
    size_t num_lanes = 4;
    engine.initialize(num_lanes, pins, timing, num_leds);

    // Total data: 200 LEDs × 4 lanes × 3 bytes/LED = 2400 bytes
    fl::vector<uint8_t> scratch(num_leds * num_lanes * 3);
    for (size_t i = 0; i < scratch.size(); i++) {
        scratch[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);  // Pseudo-random pattern
    }

    size_t lane_stride = num_leds * 3;  // 600 bytes per lane
    bool success = engine.beginTransmission(
        scratch.data(),
        scratch.size(),
        num_lanes,
        lane_stride
    );
    CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    CHECK(mock.getTransmitCount() > 0);

    // Poll until transmission completes (reduced from 1000 to 200 for performance)
    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = engine.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);  // Minimal delay to allow background thread to progress
        }
    }
    CHECK(state == ParlioEngineState::READY);
}

//=============================================================================
// Test Suite: State Inspection
//=============================================================================

TEST_CASE("ParlioEngine mock - state inspection") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    auto& mock = ParlioPeripheralMock::instance();

    // Before initialization
    CHECK_FALSE(mock.isInitialized());
    CHECK_FALSE(mock.isEnabled());
    CHECK_FALSE(mock.isTransmitting());
    CHECK(mock.getTransmitCount() == 0);

    // After initialization
    engine.initialize(2, pins, timing, 50);
    CHECK(mock.isInitialized());
    CHECK_FALSE(mock.isEnabled());  // Not enabled until transmission

    // After transmission
    fl::vector<uint8_t> scratch(50 * 2 * 3);  // 50 LEDs × 2 lanes × 3 bytes
    engine.beginTransmission(scratch.data(), scratch.size(), 2, scratch.size());

    CHECK(mock.isEnabled());
    CHECK(mock.getTransmitCount() > 0);

    // Poll until transmission completes (reduced from 1000 to 200 for performance)
    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = engine.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);  // Minimal delay to allow background thread to progress
        }
    }
    CHECK(state == ParlioEngineState::READY);
}

//=============================================================================
// Test Suite: Waveform Data Capture
//=============================================================================

TEST_CASE("ParlioEngine mock - waveform data capture") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();
    engine.initialize(1, pins, timing, 3);

    // Three LEDs with known pattern
    uint8_t scratch[9] = {
        0xFF, 0x00, 0xAA,  // LED 0
        0x55, 0xF0, 0x0F,  // LED 1
        0xC3, 0x3C, 0x99   // LED 2
    };

    engine.beginTransmission(scratch, 9, 1, 9);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() > 0);

    // Verify first transmission captured data
    const auto& first_tx = history[0];
    CHECK(first_tx.bit_count > 0);
    CHECK(first_tx.buffer_copy.size() > 0);

    // Note: Detailed waveform bit pattern validation would require
    // understanding the Wave8 encoding and transposition logic.
    // For now, we verify that data was captured successfully.
    // Future enhancement: Add detailed encoding verification.
}

TEST_CASE("ParlioEngine mock - transmission history clearing") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();
    engine.initialize(1, pins, timing, 5);

    uint8_t scratch[15] = {0};

    auto& mock = ParlioPeripheralMock::instance();

    // First transmission
    engine.beginTransmission(scratch, 15, 1, 15);
    size_t count1 = mock.getTransmissionHistory().size();
    CHECK(count1 > 0);

    // Clear history
    mock.clearTransmissionHistory();
    CHECK(mock.getTransmissionHistory().size() == 0);
    CHECK(mock.getTransmitCount() == count1);  // Counter not reset

    // Second transmission
    engine.beginTransmission(scratch, 15, 1, 15);
    size_t count2 = mock.getTransmissionHistory().size();
    CHECK(count2 > 0);
    CHECK(mock.getTransmitCount() > count1);  // Counter incremented
}

//=============================================================================
// Test Suite: Edge Cases
//=============================================================================

TEST_CASE("ParlioEngine mock - zero LEDs") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing_mock();
    engine.initialize(1, pins, timing, 1);

    // Empty transmission (edge case)
    uint8_t scratch[1] = {0};
    bool success = engine.beginTransmission(scratch, 0, 1, 0);
    (void)success;  // Suppress unused warning - behavior is implementation-defined

    // Behavior depends on implementation - either succeeds with no-op
    // or fails gracefully. Both are acceptable.
    // Just verify no crash occurs by accessing the mock.
    auto& mock = ParlioPeripheralMock::instance();
    (void)mock;  // Suppress unused warning
}

TEST_CASE("ParlioEngine mock - maximum data width") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    // Test maximum PARLIO data width (16 lanes)
    fl::vector<int> pins = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    ChipsetTimingConfig timing = getWS2812Timing_mock();

    bool success = engine.initialize(16, pins, timing, 10);
    CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    CHECK(mock.getConfig().data_width == 16);

    // Verify all pins configured
    for (size_t i = 0; i < 16; i++) {
        CHECK(mock.getConfig().gpio_pins[i] == static_cast<int>(i + 1));
    }
}

//=============================================================================
// Test Suite: Parlio Mock Untransposition
//=============================================================================

TEST_CASE("parlio_mock_untransposition") {
    // This test validates that the mock parlio peripheral correctly untransposes
    // transposed waveform data back to per-pin waveforms

    // Build LUT where bit0 = all LOW, bit1 = all HIGH
    ChipsetTiming timing;
    timing.T1 = 1;   // bit0: ~0 HIGH pulses (rounds to 0)
    timing.T2 = 999; // bit1: ~8 HIGH pulses (rounds to 8)
    timing.T3 = 1;   // period = 1001ns

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Prepare test data: 2 lanes with different patterns
    // lane0: 0xff (all bits set)
    // lane1: 0x00 (all bits clear)
    uint8_t lanes[2] = {0xff, 0x00};
    uint8_t transposed_output[2 * sizeof(Wave8Byte)]; // 16 bytes

    // Transpose the data (simulates what the DMA engine would send)
    wave8Transpose_2(lanes, lut, transposed_output);

    // Verify transposed output is 0xAA pattern (sanity check)
    for (int i = 0; i < 16; i++) {
        REQUIRE(transposed_output[i] == 0xAA);
    }

    // Now test the mock peripheral's untransposition
    auto& mock = fl::detail::ParlioPeripheralMock::instance();

    // Reset mock to clean state
    mock.reset();

    // Initialize with 2-lane configuration (using MSB packing to match original test expectations)
    fl::vector<int> pins = {1, 2};  // GPIO pin numbers: 1 and 2
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
    REQUIRE(mock.initialize(config));
    REQUIRE(mock.enable());

    // Transmit the transposed data
    size_t bit_count = 16 * 8;  // 16 bytes * 8 bits/byte = 128 bits
    REQUIRE(mock.transmit(transposed_output, bit_count, 0));

    // Wait for background thread to complete transmission
    delay(5);

    // Get per-pin data using the convenience function (use actual GPIO pin numbers)
    fl::span<const uint8_t> pin1_data = mock.getTransmissionDataForPin(1);
    fl::span<const uint8_t> pin2_data = mock.getTransmissionDataForPin(2);

    // Each pin should have 8 bytes (64 bits per pin, since 128 bits / 2 pins = 64 bits)
    REQUIRE(pin1_data.size() == 8);
    REQUIRE(pin2_data.size() == 8);

    // GPIO pin 1 should have all 0xFF (Lane 0 data)
    for (size_t i = 0; i < pin1_data.size(); i++) {
        REQUIRE(pin1_data[i] == 0xFF);
    }

    // GPIO pin 2 should have all 0x00 (Lane 1 data)
    for (size_t i = 0; i < pin2_data.size(); i++) {
        REQUIRE(pin2_data[i] == 0x00);
    }
}

TEST_CASE("parlio_mock_untransposition_complex_pattern") {
    // Test with more complex bit patterns to ensure untransposition works correctly

    ChipsetTiming timing;
    timing.T1 = 1;
    timing.T2 = 999;
    timing.T3 = 1;

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with different patterns
    // lane0: 0xAA = 0b10101010
    // lane1: 0x55 = 0b01010101
    uint8_t lanes[2] = {0xAA, 0x55};
    uint8_t transposed_output[2 * sizeof(Wave8Byte)]; // 16 bytes

    wave8Transpose_2(lanes, lut, transposed_output);

    // Setup mock (using MSB packing to match original test expectations)
    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    fl::vector<int> pins = {1, 2};  // GPIO pin numbers: 1 and 2
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
    REQUIRE(mock.initialize(config));
    REQUIRE(mock.enable());

    // Transmit
    size_t bit_count = 16 * 8;  // 128 bits
    REQUIRE(mock.transmit(transposed_output, bit_count, 0));

    // Wait for background thread to complete transmission
    delay(5);

    // Get per-pin data using the convenience function (use actual GPIO pin numbers)
    fl::span<const uint8_t> pin1_data = mock.getTransmissionDataForPin(1);
    fl::span<const uint8_t> pin2_data = mock.getTransmissionDataForPin(2);

    // Verify size
    REQUIRE(pin1_data.size() == 8);
    REQUIRE(pin2_data.size() == 8);

    // GPIO pin 1 should reconstruct waveform for Lane 0 (0xAA)
    // With the LUT (bit0=0x00, bit1=0xFF), 0xAA (10101010) expands to:
    // [0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00]
    uint8_t expected_pin1[8] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};

    // Check values with CAPTURE
    CAPTURE((int)pin1_data[0]);
    CAPTURE((int)pin1_data[1]);
    CAPTURE((int)expected_pin1[0]);
    CAPTURE((int)expected_pin1[1]);

    for (size_t i = 0; i < pin1_data.size(); i++) {
        REQUIRE(pin1_data[i] == expected_pin1[i]);
    }

    // GPIO pin 2 should reconstruct waveform for Lane 1 (0x55)
    // With the LUT (bit0=0x00, bit1=0xFF), 0x55 (01010101) expands to:
    // [0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF]
    uint8_t expected_pin2[8] = {0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF};
    for (size_t i = 0; i < pin2_data.size(); i++) {
        REQUIRE(pin2_data[i] == expected_pin2[i]);
    }
}

TEST_CASE("parlio_mock_untransposition_with_span_api") {
    // Test the new span-based untransposition API with pin mapping

    ChipsetTiming timing;
    timing.T1 = 1;
    timing.T2 = 999;
    timing.T3 = 1;

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    // Test with different patterns
    // lane0: 0xAA = 0b10101010
    // lane1: 0x55 = 0b01010101
    uint8_t lanes[2] = {0xAA, 0x55};
    uint8_t transposed_output[2 * sizeof(Wave8Byte)]; // 16 bytes

    wave8Transpose_2(lanes, lut, transposed_output);

    // Use the new static API with span inputs
    fl::vector<int> pins = {10, 20};  // Use different GPIO pin numbers
    fl::span<const uint8_t> transposed_span(transposed_output, 16);
    fl::span<const int> pins_span(pins);

    // Call the new static function
    fl::unordered_map<int, fl::vector<uint8_t>> result =
        fl::detail::ParlioPeripheralMock::untransposeParlioBitstream(transposed_span, pins_span);

    // Verify we have data for both pins
    REQUIRE(result.size() == 2);
    REQUIRE(result.find(10) != result.end());
    REQUIRE(result.find(20) != result.end());

    // Verify size
    REQUIRE(result[10].size() == 8);
    REQUIRE(result[20].size() == 8);

    // GPIO pin 10 should reconstruct waveform for Lane 0 (0xAA)
    // With the LUT (bit0=0x00, bit1=0xFF), 0xAA (10101010) expands to:
    // [0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00]
    uint8_t expected_pin10[8] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    for (size_t i = 0; i < result[10].size(); i++) {
        REQUIRE(result[10][i] == expected_pin10[i]);
    }

    // GPIO pin 20 should reconstruct waveform for Lane 1 (0x55)
    // With the LUT (bit0=0x00, bit1=0xFF), 0x55 (01010101) expands to:
    // [0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF]
    uint8_t expected_pin20[8] = {0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF};
    for (size_t i = 0; i < result[20].size(); i++) {
        REQUIRE(result[20][i] == expected_pin20[i]);
    }
}

TEST_CASE("parlio_mock_untransposition_empty_inputs") {
    // Test edge cases with empty inputs

    // Empty transposed data
    fl::vector<uint8_t> empty_data;
    fl::vector<int> pins = {1, 2};
    fl::span<const uint8_t> empty_span(empty_data);
    fl::span<const int> pins_span(pins);

    auto result = fl::detail::ParlioPeripheralMock::untransposeParlioBitstream(empty_span, pins_span);
    REQUIRE(result.empty());

    // Empty pins
    fl::vector<uint8_t> data = {0xAA, 0x55};
    fl::vector<int> empty_pins;
    fl::span<const uint8_t> data_span(data);
    fl::span<const int> empty_pins_span(empty_pins);

    result = fl::detail::ParlioPeripheralMock::untransposeParlioBitstream(data_span, empty_pins_span);
    REQUIRE(result.empty());
}

//=============================================================================
// Test Suite: LSB vs MSB Bit Packing Modes
//=============================================================================

TEST_CASE("parlio_mock_lsb_packing") {
    // Test LSB bit packing mode
    // LSB packing: bits sent in order [0,1,2,3,4,5,6,7] (forward in time)

    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    // Initialize with LSB packing
    fl::vector<int> pins = {1, 2};
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB);
    REQUIRE(mock.initialize(config));
    REQUIRE(mock.enable());

    // Test data: simple bit pattern
    // Byte 0xAA = 0b10101010
    // With LSB packing: bit0 sent first, bit7 sent last
    uint8_t test_data[2] = {0xAA, 0x55};
    size_t bit_count = 2 * 8;  // 2 bytes * 8 bits/byte = 16 bits

    REQUIRE(mock.transmit(test_data, bit_count, 0));

    // Wait for background thread to complete transmission
    delay(5);

    // Verify the packing mode was correctly set
    REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB);

    // Get transmission history
    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() == 1);
    REQUIRE(history[0].bit_count == 16);
}

TEST_CASE("parlio_mock_msb_packing") {
    // Test MSB bit packing mode
    // MSB packing: bits sent in order [7,6,5,4,3,2,1,0] (reversed in time)

    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    // Initialize with MSB packing
    fl::vector<int> pins = {1, 2};
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
    REQUIRE(mock.initialize(config));
    REQUIRE(mock.enable());

    // Test data: simple bit pattern
    uint8_t test_data[2] = {0xAA, 0x55};
    size_t bit_count = 2 * 8;  // 16 bits

    REQUIRE(mock.transmit(test_data, bit_count, 0));

    // Wait for background thread to complete transmission
    delay(5);

    // Verify the packing mode was correctly set
    REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);

    // Get transmission history
    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() == 1);
    REQUIRE(history[0].bit_count == 16);
}

TEST_CASE("parlio_mock_default_packing_is_msb") {
    // Verify that default packing mode is MSB (required for Wave8 format)

    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    // Initialize without specifying packing (should default to MSB)
    fl::vector<int> pins = {1};
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2);
    REQUIRE(mock.initialize(config));

    // Verify default is MSB (Wave8 format requires MSB bit packing)
    REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
}

TEST_CASE("parlio_mock_packing_mode_persistence") {
    // Verify that packing mode persists correctly through initialization

    auto& mock = fl::detail::ParlioPeripheralMock::instance();

    // Test MSB packing
    {
        mock.reset();
        fl::vector<int> pins = {1, 2};
        fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
        REQUIRE(mock.initialize(config));
        REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
    }

    // Test LSB packing
    {
        mock.reset();
        fl::vector<int> pins = {1, 2};
        fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB);
        REQUIRE(mock.initialize(config));
        REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB);
    }
}

#endif // FASTLED_STUB_IMPL
