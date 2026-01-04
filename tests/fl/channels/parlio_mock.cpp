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

#include "test.h"
#include "FastLED.h"

#ifdef FASTLED_STUB  // Mock tests only run on stub platform

#include "platforms/shared/mock/esp/32/drivers/parlio_peripheral_mock.h"
#include "platforms/esp/32/drivers/parlio/parlio_engine.h"

using namespace fl;
using namespace fl::detail;

namespace {

/// @brief Helper to create default timing config for WS2812
ChipsetTimingConfig getWS2812Timing() {
    ChipsetTimingConfig timing = {};
    timing.t1_ns = 350;   // T1H: 350ns ± 150ns
    timing.t2_ns = 800;   // T2H: 800ns ± 150ns
    timing.t3_ns = 450;   // T0L + T1L total
    timing.reset_us = 50; // Reset: 50µs
    return timing;
}

/// @brief Reset mock state between tests
void resetMockState() {
    auto* mock = getParlioMockInstance();
    if (mock) {
        mock->clearTransmissionHistory();
        mock->setTransmitFailure(false);
        mock->setTransmitDelay(0);
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
    ChipsetTimingConfig timing = getWS2812Timing();

    bool success = engine.initialize(1, pins, timing, 10);
    CHECK(success);

    // Verify mock received correct config
    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);
    CHECK(mock->isInitialized());
    CHECK(mock->getConfig().data_width == 1);
    CHECK(mock->getConfig().gpio_pins[0] == 1);
    CHECK(mock->getConfig().gpio_pins[1] == -1);  // Unused lanes marked -1
}

TEST_CASE("ParlioEngine mock - multi-lane initialization") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    // Four lane configuration
    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812Timing();

    bool success = engine.initialize(4, pins, timing, 100);
    CHECK(success);

    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);
    CHECK(mock->isInitialized());

    const auto& config = mock->getConfig();
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
    ChipsetTimingConfig timing = getWS2812Timing();
    engine.initialize(1, pins, timing, 1);

    // Single LED: RGB = 3 bytes
    uint8_t scratch[3] = {0xFF, 0x00, 0xAA};

    bool success = engine.beginTransmission(scratch, 3, 1, 3);
    CHECK(success);

    // Verify mock recorded transmission
    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);

    // Verify peripheral was enabled
    CHECK(mock->isEnabled());

    // Verify at least one transmission occurred
    CHECK(mock->getTransmitCount() > 0);

    // Check transmission history
    const auto& history = mock->getTransmissionHistory();
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
    ChipsetTimingConfig timing = getWS2812Timing();

    // 10 LEDs = 30 bytes
    size_t num_leds = 10;
    engine.initialize(1, pins, timing, num_leds);

    fl::vector<uint8_t> scratch(num_leds * 3);
    for (size_t i = 0; i < scratch.size(); i++) {
        scratch[i] = static_cast<uint8_t>(i & 0xFF);
    }

    bool success = engine.beginTransmission(scratch.data(), scratch.size(), 1, scratch.size());
    CHECK(success);

    auto* mock = getParlioMockInstance();
    CHECK(mock->getTransmitCount() > 0);

    // Verify transmission completed (should be in READY state)
    CHECK(engine.poll() == ParlioEngineState::READY);
}

//=============================================================================
// Test Suite: ISR Simulation
//=============================================================================

TEST_CASE("ParlioEngine mock - ISR callback simulation") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    engine.initialize(1, pins, timing, 10);

    uint8_t scratch[30];
    for (size_t i = 0; i < 30; i++) {
        scratch[i] = static_cast<uint8_t>(i);
    }

    // Start transmission
    bool success = engine.beginTransmission(scratch, 30, 1, 30);
    CHECK(success);

    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);

    // At this point, transmission should be in progress or complete
    // The mock should have recorded transmissions
    size_t initial_count = mock->getTransmitCount();
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
    ChipsetTimingConfig timing = getWS2812Timing();
    engine.initialize(1, pins, timing, 10);

    uint8_t scratch[30];

    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);

    // Inject transmit failure
    mock->setTransmitFailure(true);

    bool success = engine.beginTransmission(scratch, 30, 1, 30);
    CHECK_FALSE(success);  // Should fail

    // Verify engine detected error
    CHECK(engine.poll() == ParlioEngineState::ERROR);

    // Clear failure and reinitialize for next transmission
    mock->setTransmitFailure(false);

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
    ChipsetTimingConfig timing = getWS2812Timing();

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

    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);

    // Large transmissions may require multiple DMA buffer submissions
    // Verify at least one transmission occurred
    CHECK(mock->getTransmitCount() > 0);

    // Verify final state
    CHECK(engine.poll() == ParlioEngineState::READY);
}

TEST_CASE("ParlioEngine mock - multi-lane streaming") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    // Test with 4 lanes
    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_leds = 200;  // 200 LEDs per lane
    size_t num_lanes = 4;
    engine.initialize(num_lanes, pins, timing, num_leds);

    // Total data: 200 LEDs × 4 lanes × 3 bytes/LED = 2400 bytes
    fl::vector<uint8_t> scratch(num_leds * num_lanes * 3);
    for (size_t i = 0; i < scratch.size(); i++) {
        scratch[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);  // Pseudo-random pattern
    }

    bool success = engine.beginTransmission(
        scratch.data(),
        scratch.size(),
        num_lanes,
        scratch.size()
    );
    CHECK(success);

    auto* mock = getParlioMockInstance();
    CHECK(mock->getTransmitCount() > 0);

    // Verify completion
    CHECK(engine.poll() == ParlioEngineState::READY);
}

//=============================================================================
// Test Suite: State Inspection
//=============================================================================

TEST_CASE("ParlioEngine mock - state inspection") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812Timing();

    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);

    // Before initialization
    CHECK_FALSE(mock->isInitialized());
    CHECK_FALSE(mock->isEnabled());
    CHECK_FALSE(mock->isTransmitting());
    CHECK(mock->getTransmitCount() == 0);

    // After initialization
    engine.initialize(2, pins, timing, 50);
    CHECK(mock->isInitialized());
    CHECK_FALSE(mock->isEnabled());  // Not enabled until transmission

    // After transmission
    fl::vector<uint8_t> scratch(50 * 2 * 3);  // 50 LEDs × 2 lanes × 3 bytes
    engine.beginTransmission(scratch.data(), scratch.size(), 2, scratch.size());

    CHECK(mock->isEnabled());
    CHECK(mock->getTransmitCount() > 0);

    // After completion
    CHECK(engine.poll() == ParlioEngineState::READY);
}

//=============================================================================
// Test Suite: Waveform Data Capture
//=============================================================================

TEST_CASE("ParlioEngine mock - waveform data capture") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    engine.initialize(1, pins, timing, 3);

    // Three LEDs with known pattern
    uint8_t scratch[9] = {
        0xFF, 0x00, 0xAA,  // LED 0
        0x55, 0xF0, 0x0F,  // LED 1
        0xC3, 0x3C, 0x99   // LED 2
    };

    engine.beginTransmission(scratch, 9, 1, 9);

    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);

    const auto& history = mock->getTransmissionHistory();
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
    ChipsetTimingConfig timing = getWS2812Timing();
    engine.initialize(1, pins, timing, 5);

    uint8_t scratch[15] = {0};

    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);

    // First transmission
    engine.beginTransmission(scratch, 15, 1, 15);
    size_t count1 = mock->getTransmissionHistory().size();
    CHECK(count1 > 0);

    // Clear history
    mock->clearTransmissionHistory();
    CHECK(mock->getTransmissionHistory().size() == 0);
    CHECK(mock->getTransmitCount() == count1);  // Counter not reset

    // Second transmission
    engine.beginTransmission(scratch, 15, 1, 15);
    size_t count2 = mock->getTransmissionHistory().size();
    CHECK(count2 > 0);
    CHECK(mock->getTransmitCount() > count1);  // Counter incremented
}

//=============================================================================
// Test Suite: Edge Cases
//=============================================================================

TEST_CASE("ParlioEngine mock - zero LEDs") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    engine.initialize(1, pins, timing, 1);

    // Empty transmission (edge case)
    uint8_t scratch[1] = {0};
    bool success = engine.beginTransmission(scratch, 0, 1, 0);

    // Behavior depends on implementation - either succeeds with no-op
    // or fails gracefully. Both are acceptable.
    // Just verify no crash occurs.
    auto* mock = getParlioMockInstance();
    CHECK(mock != nullptr);  // Mock should still exist
}

TEST_CASE("ParlioEngine mock - maximum data width") {
    resetMockState();

    auto& engine = ParlioEngine::getInstance();

    // Test maximum PARLIO data width (16 lanes)
    fl::vector<int> pins = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    ChipsetTimingConfig timing = getWS2812Timing();

    bool success = engine.initialize(16, pins, timing, 10);
    CHECK(success);

    auto* mock = getParlioMockInstance();
    REQUIRE(mock != nullptr);
    CHECK(mock->getConfig().data_width == 16);

    // Verify all pins configured
    for (size_t i = 0; i < 16; i++) {
        CHECK(mock->getConfig().gpio_pins[i] == static_cast<int>(i + 1));
    }
}

#endif // FASTLED_STUB
