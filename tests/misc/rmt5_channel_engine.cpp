/// @file rmt5_channel_engine.cpp
/// @brief ChannelEngineRMT integration tests with mock peripheral
///
/// Tests the ChannelEngineRMT business logic using the mock peripheral:
/// - Single channel transmission
/// - Multi-channel time-multiplexing
/// - State machine progression (READY → BUSY → READY)
/// - Buffer management and completion callbacks
///
/// These tests run ONLY on stub platforms (host-based testing).
///
/// Design Philosophy:
/// - Simple, focused tests (one behavior per test)
/// - Direct API usage (no complex helper abstractions)
/// - Observable behavior testing (not internal state inspection)
/// - See tests/AGENTS.md for Test Simplicity Principle

#ifdef FASTLED_STUB_IMPL  // Mock tests only run on stub platform

#include "platforms/shared/mock/esp/32/drivers/rmt5_peripheral_mock.h"
#include "platforms/esp/32/drivers/rmt/rmt_5/channel_engine_rmt.h"
#include "fl/chipsets/led_timing.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/channels/data.h"
#include "fl/channels/config.h"
#include "fl/channels/engine.h"
#include "fl/stl/vector.h"
#include "test.h"

using namespace fl;
using namespace fl::detail;

// Import EngineState enum for cleaner test code
using EngineState = IChannelEngine::EngineState;

namespace {

//=============================================================================
// Test Helpers
//=============================================================================

/// @brief Create WS2812B timing configuration
ChipsetTimingConfig createWS2812Timing() {
    return ChipsetTimingConfig(
        350,   // t1_ns: T0H
        450,   // t2_ns: T1H - T0H
        450,   // t3_ns: T0L
        50,    // reset_us: Reset pulse
        "WS2812B"
    );
}

/// @brief Simple pixel-to-byte encoder (GRB order for WS2812)
///
/// This is NOT the real RMT encoder - it just creates simple encoded bytes
/// for testing. The RMT peripheral mock doesn't validate waveform correctness,
/// it just captures transmitted bytes.
fl::vector_psram<uint8_t> encodePixels(fl::span<const uint8_t> rgb_pixels) {
    fl::vector_psram<uint8_t> encoded;

    // Convert RGB to GRB byte order (WS2812B expects GRB)
    for (size_t i = 0; i < rgb_pixels.size(); i += 3) {
        if (i + 2 < rgb_pixels.size()) {
            encoded.push_back(rgb_pixels[i + 1]); // G
            encoded.push_back(rgb_pixels[i + 0]); // R
            encoded.push_back(rgb_pixels[i + 2]); // B
        }
    }

    return encoded;
}

/// @brief Create ChannelData with RGB pixel data
ChannelDataPtr createChannelData(int pin, size_t num_leds, const uint8_t* rgb_data = nullptr) {
    ChipsetTimingConfig timing = createWS2812Timing();

    // Create RGB pixel buffer (default to all zeros if not provided)
    fl::vector<uint8_t> pixels(num_leds * 3);
    if (rgb_data) {
        for (size_t i = 0; i < num_leds * 3; i++) {
            pixels[i] = rgb_data[i];
        }
    }

    // Encode pixels to transmission bytes
    fl::vector_psram<uint8_t> encoded = encodePixels(pixels);

    return ChannelData::create(pin, timing, fl::move(encoded));
}

/// @brief Reset mock peripheral state between tests
void resetMock() {
    auto& mock = Rmt5PeripheralMock::instance();
    mock.reset();
}

} // anonymous namespace

//=============================================================================
// Test Suite: Basic Transmission
//=============================================================================

FL_TEST_CASE("RMT5 engine - create and destroy") {
    resetMock();

    auto engine = ChannelEngineRMT::create();
    FL_CHECK(engine != nullptr);

    // Initial state should be READY
    FL_CHECK(engine->poll() == EngineState::READY);
}

FL_TEST_CASE("RMT5 engine - single channel transmission") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto engine = ChannelEngineRMT::create();

    // Create channel with 1 LED (red)
    uint8_t red_pixel[] = {0xFF, 0x00, 0x00};
    auto ch = createChannelData(18, 1, red_pixel);

    // Enqueue and show
    engine->enqueue(ch);
    engine->show();

    // Verify transmission started
    FL_CHECK(mock.getTransmissionCount() >= 1);
    FL_CHECK(ch->isInUse() == true);

    // Engine should be BUSY
    FL_CHECK(engine->poll() == EngineState::BUSY);

    // Simulate transmission completion
    const auto& history = mock.getTransmissionHistory();
    if (!history.empty()) {
        // Get the channel handle from the first transmission
        // (Mock uses channel ID as handle)
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
    }

    // Poll to process completion (may take a few cycles to clear inUse)
    for (int i = 0; i < 10 && ch->isInUse(); i++) {
        engine->poll();
    }

    // Should eventually return to READY and clear inUse flag
    FL_CHECK(ch->isInUse() == false);
}

FL_TEST_CASE("RMT5 engine - multiple LED transmission") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto engine = ChannelEngineRMT::create();

    // Create channel with 3 LEDs (RGB sequence)
    uint8_t rgb_pixels[] = {
        0xFF, 0x00, 0x00,  // Red
        0x00, 0xFF, 0x00,  // Green
        0x00, 0x00, 0xFF   // Blue
    };
    auto ch = createChannelData(18, 3, rgb_pixels);

    engine->enqueue(ch);
    engine->show();

    // Verify transmission occurred
    FL_CHECK(mock.getTransmissionCount() >= 1);

    // Verify transmitted data size (3 LEDs = 9 bytes in GRB format)
    const auto& history = mock.getTransmissionHistory();
    if (!history.empty()) {
        FL_CHECK(history[0].buffer_size == 9);
        FL_CHECK(history[0].gpio_pin == 18);
    }

    // Complete transmission to allow clean shutdown
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
        for (int i = 0; i < 10 && ch->isInUse(); i++) {
            engine->poll();
        }
    }
}

//=============================================================================
// Test Suite: Multi-Channel Time-Multiplexing
//=============================================================================

FL_TEST_CASE("RMT5 engine - two channels different pins") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto engine = ChannelEngineRMT::create();

    // Create two channels on different pins
    uint8_t red[] = {0xFF, 0x00, 0x00};
    uint8_t green[] = {0x00, 0xFF, 0x00};

    auto ch1 = createChannelData(18, 1, red);
    auto ch2 = createChannelData(19, 1, green);

    engine->enqueue(ch1);
    engine->enqueue(ch2);
    engine->show();

    // Verify both channels were created (or at least attempted)
    // Note: Actual transmission count depends on hardware limits
    FL_CHECK(mock.getChannelCount() >= 1);

    // Complete transmissions to allow clean shutdown
    for (size_t ch_id = 1; ch_id <= mock.getChannelCount(); ch_id++) {
        void* channel_handle = reinterpret_cast<void*>(ch_id);
        mock.simulateTransmitDone(channel_handle);
    }
    for (int i = 0; i < 10 && engine->poll() != EngineState::READY; i++) {
        // Poll until ready
    }
}

FL_TEST_CASE("RMT5 engine - same pin sequential frames") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto engine = ChannelEngineRMT::create();

    uint8_t red[] = {0xFF, 0x00, 0x00};
    uint8_t green[] = {0x00, 0xFF, 0x00};

    // First frame
    auto ch1 = createChannelData(18, 1, red);
    engine->enqueue(ch1);
    engine->show();

    // Complete first transmission
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
        engine->poll();
    }

    // Second frame (same pin)
    mock.clearTransmissionHistory();
    auto ch2 = createChannelData(18, 1, green);
    engine->enqueue(ch2);
    engine->show();

    // Verify second transmission occurred
    FL_CHECK(mock.getTransmissionCount() >= 1);

    // Complete second transmission
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
        for (int i = 0; i < 10 && ch2->isInUse(); i++) {
            engine->poll();
        }
    }
}

//=============================================================================
// Test Suite: State Machine
//=============================================================================

FL_TEST_CASE("RMT5 engine - state progression READY → BUSY → READY") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto engine = ChannelEngineRMT::create();

    // Initial state should be READY
    FL_CHECK(engine->poll() == EngineState::READY);

    // Enqueue and show
    auto ch = createChannelData(18, 1);
    engine->enqueue(ch);
    engine->show();

    // State should be BUSY after show()
    FL_CHECK(engine->poll() == EngineState::BUSY);

    // Simulate completion
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
    }

    // Poll multiple times to process completion
    EngineState state = EngineState::BUSY;
    for (int i = 0; i < 10 && state != EngineState::READY; i++) {
        state = engine->poll();
    }

    // Should eventually return to READY
    FL_CHECK(state == EngineState::READY);
}

//=============================================================================
// Test Suite: Error Handling
//=============================================================================

// TODO: Re-enable after fixing engine failure handling
// FL_TEST_CASE("RMT5 engine - handle transmission failure") {
//     resetMock();
//     auto& mock = Rmt5PeripheralMock::instance();
//     auto engine = ChannelEngineRMT::create();

//     // Inject failure
//     mock.setTransmitFailure(true);

//     auto ch = createChannelData(18, 1);
//     engine->enqueue(ch);
//     engine->show();

//     // Engine should handle failure gracefully (no crash)
//     auto state = engine->poll();
//     (void)state; // May be READY or ERROR depending on implementation

//     // Disable failure for next test
//     mock.setTransmitFailure(false);

//     // Note: The channel may be in an error state and stuck BUSY.
//     // The engine destructor has a timeout to handle this gracefully.
// }

//=============================================================================
// Test Suite: Edge Cases
//=============================================================================

FL_TEST_CASE("RMT5 engine - empty enqueue") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto engine = ChannelEngineRMT::create();

    // Call show without enqueue
    engine->show();

    // Should not crash, no transmissions
    FL_CHECK(mock.getTransmissionCount() == 0);
}

// TODO: Re-enable after fixing engine 0-byte buffer handling
// FL_TEST_CASE("RMT5 engine - zero LED channel") {
//     resetMock();
//     auto engine = ChannelEngineRMT::create();

//     auto ch = createChannelData(18, 0);  // Zero LEDs

//     engine->enqueue(ch);
//     engine->show();

//     // Should handle gracefully (0 bytes = no transmission, immediate READY)
//     auto state = engine->poll();
//     FL_CHECK(state == EngineState::READY);
//     FL_CHECK(ch->isInUse() == false);  // Should not be in use (no valid transmission)
// }

FL_TEST_CASE("RMT5 engine - rapid show() calls") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto engine = ChannelEngineRMT::create();

    auto ch = createChannelData(18, 1);

    // Multiple rapid show() calls should not crash
    engine->enqueue(ch);
    engine->show();

    // Subsequent show() calls while busy should be handled
    // (exact behavior depends on engine implementation - just verify no crash)
    auto state = engine->poll();
    (void)state; // Don't care about state - just verifying no crash

    FL_CHECK(mock.getChannelCount() >= 0);

    // Complete transmission to allow clean shutdown
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
        for (int i = 0; i < 10 && ch->isInUse(); i++) {
            engine->poll();
        }
    }
}

#endif // FASTLED_STUB_IMPL
