/// @file rmt5_channel_engine.cpp
/// @brief ChannelEngineRMT integration tests with mock peripheral
///
/// Tests the ChannelEngineRMT business logic using the mock peripheral:
/// - Single channel transmission
/// - Multi-channel time-multiplexing
/// - State machine progression (READY → BUSY/DRAINING → READY)
/// - Buffer management and completion callbacks
///
/// These tests run ONLY on stub platforms (host-based testing).
///
/// Design Philosophy:
/// - Simple, focused tests (one behavior per test)
/// - Direct API usage (no complex helper abstractions)
/// - Observable behavior testing (not internal state inspection)
/// - See agents/tests.md for Test Simplicity Principle

#ifdef FASTLED_STUB_IMPL  // Mock tests only run on stub platform

#include "platforms/shared/mock/esp/32/drivers/rmt5_peripheral_mock.h"
#include "platforms/shared/mock/esp/32/drivers/rmt5_support_stubs.h"
#include "platforms/esp/32/drivers/rmt/rmt_5/channel_driver_rmt.h"
#include "fl/chipsets/led_timing.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/channels/data.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/stl/chrono.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::detail;

// Import DriverState enum for cleaner test code
using DriverState = IChannelDriver::DriverState;

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

FL_TEST_CASE("RMT5 driver - create and destroy") {
    resetMock();

    auto driver = ChannelEngineRMT::create();
    FL_CHECK(driver != nullptr);

    // Initial state should be READY
    FL_CHECK(driver->poll() == DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - single channel transmission") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    // Create channel with 1 LED (red)
    uint8_t red_pixel[] = {0xFF, 0x00, 0x00};
    auto ch = createChannelData(18, 1, red_pixel);

    // Enqueue and show
    driver->enqueue(ch);
    driver->show();

    // Verify transmission started
    FL_CHECK(mock.getTransmissionCount() >= 1);
    FL_CHECK(ch->isInUse() == true);

    // Every strip is on the wire and its bytes live in the driver's pool
    // buffer, so the engine reports DRAINING (#2994).
    FL_CHECK(driver->poll() == DriverState::DRAINING);

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
        driver->poll();
    }

    // Should eventually return to READY and clear inUse flag
    FL_CHECK(ch->isInUse() == false);
}

FL_TEST_CASE("RMT5 driver - single channel submits without temporary owners") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    uint8_t pixel[] = {0x12, 0x34, 0x56};
    auto ch = createChannelData(18, 1, pixel);
    long ownersDuringWrite = 0;
    ch->setPaddingGenerator([&](fl::span<const u8> src, fl::span<u8> dst) {
        ownersDuringWrite = ch.use_count();
        for (size_t i = 0; i < src.size(); ++i) {
            dst[i] = src[i];
        }
    });

    // Exercise both initial hardware acquisition and reuse of the idle channel.
    for (int frame = 0; frame < 2; ++frame) {
        driver->enqueue(ch);
        const long ownersBeforeShow = ch.use_count();
        driver->show();

        // Sorting and pending-queue copies must not add owners on this path.
        FL_CHECK_EQ(ownersDuringWrite, ownersBeforeShow);
        const auto& history = mock.getTransmissionHistory();
        FL_REQUIRE_EQ(history.size(), static_cast<size_t>(frame + 1));
        FL_CHECK_EQ(history.back().gpio_pin, 18);
        FL_REQUIRE_EQ(history.back().buffer_copy.size(), 3u);
        FL_CHECK_EQ(history.back().buffer_copy[0], 0x34);
        FL_CHECK_EQ(history.back().buffer_copy[1], 0x12);
        FL_CHECK_EQ(history.back().buffer_copy[2], 0x56);
        FL_CHECK(driver->poll() == DriverState::DRAINING);

        mock.simulateTransmitDone(reinterpret_cast<void*>(1));
        FL_CHECK(driver->poll() == DriverState::READY);
        FL_CHECK_FALSE(ch->isInUse());
    }
}

FL_TEST_CASE("RMT5 driver - multiple LED transmission") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    // Create channel with 3 LEDs (RGB sequence)
    uint8_t rgb_pixels[] = {
        0xFF, 0x00, 0x00,  // Red
        0x00, 0xFF, 0x00,  // Green
        0x00, 0x00, 0xFF   // Blue
    };
    auto ch = createChannelData(18, 3, rgb_pixels);

    driver->enqueue(ch);
    driver->show();

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
            driver->poll();
        }
    }
}

//=============================================================================
// Test Suite: Multi-Channel Time-Multiplexing
//=============================================================================

FL_TEST_CASE("RMT5 driver - two channels different pins") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    // Create two channels on different pins
    uint8_t red[] = {0xFF, 0x00, 0x00};
    uint8_t green[] = {0x00, 0xFF, 0x00};

    auto ch1 = createChannelData(18, 1, red);
    auto ch2 = createChannelData(19, 1, green);

    driver->enqueue(ch1);
    driver->enqueue(ch2);
    driver->show();

    // Verify both channels were created (or at least attempted)
    // Note: Actual transmission count depends on hardware limits
    FL_CHECK(mock.getChannelCount() >= 1);

    // Complete transmissions to allow clean shutdown
    for (size_t ch_id = 1; ch_id <= mock.getChannelCount(); ch_id++) {
        void* channel_handle = reinterpret_cast<void*>(ch_id);
        mock.simulateTransmitDone(channel_handle);
    }
    for (int i = 0; i < 10 && driver->poll() != DriverState::READY; i++) {
        // Poll until ready
    }
}

FL_TEST_CASE("RMT5 driver - same pin sequential frames") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    uint8_t red[] = {0xFF, 0x00, 0x00};
    uint8_t green[] = {0x00, 0xFF, 0x00};

    // First frame
    auto ch1 = createChannelData(18, 1, red);
    driver->enqueue(ch1);
    driver->show();

    // Complete first transmission
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
        driver->poll();
    }

    // Second frame (same pin)
    mock.clearTransmissionHistory();
    auto ch2 = createChannelData(18, 1, green);
    driver->enqueue(ch2);
    driver->show();

    // Verify second transmission occurred
    FL_CHECK(mock.getTransmissionCount() >= 1);

    // Complete second transmission
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
        for (int i = 0; i < 10 && ch2->isInUse(); i++) {
            driver->poll();
        }
    }
}

FL_TEST_CASE("RMT5 driver - changing pin and timing leaves one encoder") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    driver->enqueue(createChannelData(18, 1));
    driver->show();
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    FL_REQUIRE(driver->poll() == DriverState::READY);

    const ChipsetTimingConfig timing(400, 400, 450, 80, "alternate timing");
    driver->enqueue(ChannelData::create(19, timing, fl::vector_psram<uint8_t>{0, 0, 0}));
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);
    FL_CHECK_EQ(mock.getEncoderCount(), 1u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_CHECK(driver->poll() == DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - cached pin skips routing query until changed") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto showFrame = [&](int pin, int handle, size_t numLeds = 1) {
        auto data = createChannelData(pin, numLeds);
        const auto before = mock.getTransmissionHistory().size();
        driver->enqueue(data);
        driver->show();
        FL_REQUIRE_EQ(mock.getTransmissionHistory().size(), before + 1);
        FL_CHECK_EQ(driver->poll().state, DriverState::DRAINING);
        mock.simulateTransmitDone(reinterpret_cast<void*>(static_cast<uintptr_t>(handle)));
        FL_CHECK_EQ(driver->poll().state, DriverState::READY);
    };

    showFrame(18, 1);
    const auto initialQueries = mock.getPinRoutingQueryCount();
    FL_REQUIRE_GT(initialQueries, 0u);
    showFrame(18, 1); // The same hardware handle must complete this frame too.
    FL_CHECK_EQ(mock.getPinRoutingQueryCount(), initialQueries);
    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getPinRebindCount(), 0u);

    // Payload buffers can change size without changing the bound hardware.
    showFrame(18, 1, 2);
    FL_CHECK_EQ(mock.getPinRoutingQueryCount(), initialQueries);
    FL_CHECK_EQ(mock.getTransmissionHistory().back().buffer_size, 6u);

    auto timing = createWS2812Timing();
    timing.t1_ns += 50;
    fl::vector_psram<uint8_t> bytes(3, 0x12);
    auto retimed = ChannelData::create(18, timing, fl::move(bytes));
    const auto beforeRetime = mock.getTransmissionHistory().size();
    driver->enqueue(retimed);
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionHistory().size(), beforeRetime + 1);
    FL_CHECK_EQ(mock.getTransmissionHistory().back().timing.T1, timing.t1_ns);
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    FL_CHECK_EQ(driver->poll().state, DriverState::READY);
    FL_CHECK_EQ(mock.getPinRoutingQueryCount(), initialQueries);

    mock.simulatePinReroute(18);
    showFrame(18, 1);
    FL_CHECK_GT(mock.getPinRoutingQueryCount(), initialQueries);
    FL_CHECK_EQ(mock.getPinRebindCount(), 1u);
    const auto reclaimedQueries = mock.getPinRoutingQueryCount();
    showFrame(18, 1);
    FL_CHECK_EQ(mock.getPinRoutingQueryCount(), reclaimedQueries);

    showFrame(19, 2);
    FL_CHECK_GT(mock.getPinRoutingQueryCount(), reclaimedQueries);
    FL_CHECK_EQ(mock.getTransmissionHistory().back().gpio_pin, 19);
}

FL_TEST_CASE("RMT5 driver - failed timing change preserves the existing encoder") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    void* channel_handle = reinterpret_cast<void*>(1);
    auto original = createChannelData(18, 1);
    auto timing = createWS2812Timing();
    timing.t1_ns += 50;
    auto retimed = ChannelData::create(18, timing, fl::vector_psram<uint8_t>{0, 0, 0});

    driver->enqueue(original);
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 1u);
    mock.simulateTransmitDone(channel_handle);
    FL_REQUIRE_EQ(driver->poll().state, DriverState::READY);

    mock.setEncoderCreationFailure(true);
    driver->enqueue(retimed);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 1u);
    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getEncoderCount(), 1u);

    driver->enqueue(original);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);
    FL_CHECK_EQ(mock.getTransmissionHistory().back().timing.T1, createWS2812Timing().t1_ns);
    mock.simulateTransmitDone(channel_handle);
    FL_CHECK_EQ(driver->poll().state, DriverState::READY);

    mock.setEncoderCreationFailure(false);
    driver->enqueue(retimed);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 3u);
    FL_CHECK_EQ(mock.getTransmissionHistory().back().timing.T1, timing.t1_ns);
    FL_CHECK_EQ(mock.getEncoderCount(), 1u);
    mock.simulateTransmitDone(channel_handle);
    FL_CHECK_EQ(driver->poll().state, DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - encoder allocation failure leaves no hardware channel") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto data = createChannelData(18, 1);
    mock.setMaxChannels(1);
    mock.setEncoderCreationFailure(true);
    mock.setChannelDeletionFailure(true);

    driver->enqueue(data);
    driver->show();
    FL_CHECK_EQ(driver->poll().state, DriverState::READY);
    FL_CHECK_EQ(mock.getChannelCount(), 0u);

    mock.setEncoderCreationFailure(false);
    mock.setChannelDeletionFailure(false);
    driver->enqueue(data);
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 1u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    FL_CHECK_EQ(driver->poll().state, DriverState::READY);
}

// FastLED#4905: another peripheral (or pinMode) can take the pin of an idle
// cached channel. Reusing that channel must route the pin back to it, and
// must leave an untouched pin alone.
FL_TEST_CASE("RMT5 driver - reused channel reclaims a rerouted pin") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    uint8_t red[] = {0xFF, 0x00, 0x00};
    void* channel_handle = reinterpret_cast<void*>(1);

    auto showFrame = [&]() {
        auto ch = createChannelData(18, 1, red);
        driver->enqueue(ch);
        driver->show();
        mock.simulateTransmitDone(channel_handle);
        for (int i = 0; i < 10 && ch->isInUse(); i++) {
            driver->poll();
        }
    };

    showFrame();
    FL_REQUIRE(mock.getChannelCount() == 1);
    showFrame();
    FL_CHECK(mock.getPinRebindCount() == 0);

    mock.simulatePinReroute(18);
    const size_t txBeforeReclaim = mock.getTransmissionCount();
    showFrame();
    FL_CHECK(mock.getPinRebindCount() == 1);
    FL_CHECK(mock.getChannelCount() == 1);
    // The rebind disables the channel; the frame must still go out (#5359).
    FL_CHECK(mock.getTransmissionCount() == txBeforeReclaim + 1);

    showFrame();
    FL_CHECK(mock.getPinRebindCount() == 1);
}

FL_TEST_CASE("RMT5 driver - failed pin rebind skips submission and retries next frame") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto data = createChannelData(18, 1);
    void* handle = reinterpret_cast<void*>(1);

    driver->enqueue(data);
    driver->show();
    mock.simulateTransmitDone(handle);
    FL_REQUIRE(driver->poll() == DriverState::READY);

    mock.simulatePinReroute(18);
    mock.setPinRebindFailure(true);
    driver->enqueue(data);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 1u);
    if (mock.getTransmissionCount() > 1u) {
        mock.simulateTransmitDone(handle);
    }
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(data->isInUse());

    mock.setPinRebindFailure(false);
    driver->enqueue(data);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);
    FL_CHECK_EQ(mock.getPinRebindCount(), 1u);
    mock.simulateTransmitDone(handle);
    FL_CHECK(driver->poll() == DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - failed disable during pin reclaim preserves channel ownership") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto data = createChannelData(18, 1);
    void* handle = reinterpret_cast<void*>(1);

    driver->enqueue(data);
    driver->show();
    mock.simulateTransmitDone(handle);
    FL_REQUIRE(driver->poll() == DriverState::READY);
    FL_REQUIRE(mock.isChannelEnabled(handle));

    mock.simulatePinReroute(18);
    mock.setPinRebindDisableFailure(true);
    driver->enqueue(data);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 1u);
    FL_CHECK(driver->poll() == DriverState::READY);

    mock.setPinRebindDisableFailure(false);
    driver.reset();
    FL_CHECK_EQ(mock.getChannelCount(), 0u);
}

// FastLED#5356: enabling and disabling the RMT channel around every frame
// cost ~45 us of show() latency on an ESP32-C6. A channel reused on the same
// pin stays enabled between frames, so it is enabled exactly once.
FL_TEST_CASE("RMT5 driver - reused channel stays enabled across frames") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    uint8_t red[] = {0xFF, 0x00, 0x00};
    void* channel_handle = reinterpret_cast<void*>(1);

    for (int frame = 0; frame < 4; ++frame) {
        auto ch = createChannelData(18, 1, red);
        driver->enqueue(ch);
        driver->show();
        mock.simulateTransmitDone(channel_handle);
        for (int i = 0; i < 10 && ch->isInUse(); i++) {
            driver->poll();
        }
        FL_CHECK_FALSE(ch->isInUse());
    }

    FL_CHECK(mock.getTransmissionCount() == 4);
    FL_CHECK(mock.getChannelCount() == 1);
    FL_CHECK(mock.getEnableCount() == 1);
}

// FastLED#5356: the transmit-done ISR already proves the hardware finished,
// so completing a frame must not block in rmt_tx_wait_all_done() again
// (~2.1k instructions per show() on an ESP32-C6). rmt_transmit() recycles
// the finished transaction descriptor by itself.
FL_TEST_CASE("RMT5 driver - frame completion does not re-wait on the hardware") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    uint8_t red[] = {0xFF, 0x00, 0x00};
    void* channel_handle = reinterpret_cast<void*>(1);

    for (int frame = 0; frame < 4; ++frame) {
        auto ch = createChannelData(18, 1, red);
        driver->enqueue(ch);
        driver->show();
        mock.simulateTransmitDone(channel_handle);
        for (int i = 0; i < 10 && ch->isInUse(); i++) {
            driver->poll();
        }
        FL_CHECK_FALSE(ch->isInUse());
    }

    FL_CHECK(mock.getTransmissionCount() == 4);
    FL_CHECK(mock.getWaitAllDoneCount() == 0);
}

//=============================================================================
// Test Suite: State Machine
//=============================================================================

FL_TEST_CASE("RMT5 driver - state progression READY → DRAINING → READY") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    // Initial state should be READY
    FL_CHECK(driver->poll() == DriverState::READY);

    // Enqueue and show
    auto ch = createChannelData(18, 1);
    driver->enqueue(ch);
    driver->show();

    // Nothing is pending once show() has submitted the only strip, so the
    // driver is DRAINING, not BUSY. ChannelManager::onEndFrame() returns on
    // DRAINING; reporting BUSY made FastLED.show() block for the whole wire
    // time and the reset latch on every frame (#2994).
    FL_CHECK(driver->poll() == DriverState::DRAINING);

    // Simulate completion
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
    }

    // Poll multiple times to process completion
    DriverState state = DriverState::DRAINING;
    for (int i = 0; i < 10 && state != DriverState::READY; i++) {
        state = driver->poll();
    }

    // Should eventually return to READY
    FL_CHECK(state == DriverState::READY);
}

//=============================================================================
// Test Suite: Error Handling
//=============================================================================

FL_TEST_CASE("RMT5 driver - failed channel setup rolls back and allows retry") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto& memMgr = RmtMemoryManager::instance();
    const size_t rollbacksBefore = memMgr.rollbackAllocationCount();
    auto driver = ChannelEngineRMT::create();

    // The mock rejects a negative GPIO after the driver has reserved RMT
    // memory, exercising the production createChannel() failure cleanup.
    auto invalid = createChannelData(-1, 1);
    driver->enqueue(invalid);
    driver->show();

    FL_CHECK_EQ(mock.getChannelCount(), 0u);
    FL_CHECK_GT(memMgr.rollbackAllocationCount(), rollbacksBefore);
    FL_REQUIRE(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(invalid->isInUse());

    // A fresh frame must be able to reserve memory and create a valid channel.
    auto valid = createChannelData(18, 1);
    driver->enqueue(valid);
    driver->show();

    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getTransmissionCount(), 1u);

    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    for (int i = 0; i < 10 && valid->isInUse(); i++) {
        driver->poll();
    }
    FL_CHECK_FALSE(valid->isInUse());
}

FL_TEST_CASE("RMT5 driver - callback registration failure releases the new channel and encoder") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto frame = createChannelData(18, 1);

    mock.setCallbackRegistrationFailure(true);
    driver->enqueue(frame);
    driver->show();

    FL_CHECK_EQ(mock.getChannelCount(), 0u);
    FL_CHECK_EQ(mock.getEncoderCount(), 0u);
    FL_CHECK_EQ(mock.getTransmissionCount(), 0u);
    FL_REQUIRE(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(frame->isInUse());

    mock.setCallbackRegistrationFailure(false);
    driver->enqueue(frame);
    driver->show();

    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getEncoderCount(), 1u);
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 1u);

    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(frame->isInUse());
}

FL_TEST_CASE("RMT5 driver - learned allocation reduction updates later ledger entries") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto& memMgr = RmtMemoryManager::instance();
    auto driver = ChannelEngineRMT::create();
    const auto recoveries = memMgr.recoveryAllocationCount();

    // The first channel learns the smaller limit. The next channel fits on
    // its first hardware attempt but must also record the smaller allocation.
    mock.setMaxSymbolsPerChannel(SOC_RMT_MEM_WORDS_PER_CHANNEL);
    memMgr.setTxAllocationWords(2 * SOC_RMT_MEM_WORDS_PER_CHANNEL);
    driver->enqueue(createChannelData(18, 2));
    driver->enqueue(createChannelData(19, 1));
    driver->show();
    memMgr.setTxAllocationWords(SOC_RMT_MEM_WORDS_PER_CHANNEL);

    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);
    FL_CHECK_EQ(memMgr.recoveryAllocationCount(), recoveries + 2);
    FL_CHECK_EQ(memMgr.lastRecoveryAllocationId(), 1u);
    FL_CHECK_EQ(memMgr.lastRecoveryAllocationWords(), SOC_RMT_MEM_WORDS_PER_CHANNEL);
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_CHECK(driver->poll() == DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - failed callback cleanup retains the channel for retry") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto frame = createChannelData(18, 1);

    mock.setCallbackRegistrationFailure(true);
    mock.setChannelDeletionFailure(true);
    driver->enqueue(frame);
    driver->show();
    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getEncoderCount(), 1u);
    FL_CHECK_EQ(mock.getTransmissionCount(), 0u);
    FL_CHECK(driver->poll() == DriverState::READY);

    mock.setCallbackRegistrationFailure(false);
    mock.setChannelDeletionFailure(false);
    driver->enqueue(frame);
    driver->show();
    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getTransmissionCount(), 1u);

    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    FL_CHECK(driver->poll() == DriverState::READY);
    driver.reset();
    FL_CHECK_EQ(mock.getChannelCount(), 0u);
}

FL_TEST_CASE("RMT5 driver - failed pin changes leave a reusable slot") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto& memMgr = RmtMemoryManager::instance();
    auto driver = ChannelEngineRMT::create();
    auto valid = createChannelData(18, 1);
    auto invalid = createChannelData(-1, 1);

    for (size_t frame = 0; frame < 3; ++frame) {
        driver->enqueue(valid);
        driver->show();
        FL_REQUIRE_EQ(mock.getTransmissionCount(), frame + 1);
        FL_CHECK_EQ(memMgr.lastTxAllocationId(), 0u);
        mock.simulateTransmitDone(
            reinterpret_cast<void*>(static_cast<uintptr_t>(frame + 1)));
        FL_REQUIRE(driver->poll() == DriverState::READY);

        driver->enqueue(invalid);
        driver->show();
        FL_REQUIRE(driver->poll() == DriverState::READY);
        FL_CHECK_FALSE(invalid->isInUse());
        FL_CHECK_EQ(mock.getChannelCount(), 0u);
    }
}

FL_TEST_CASE("RMT5 driver - failed pin change keeps the original pin channel reusable") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto original = createChannelData(18, 1);
    auto replacement = createChannelData(19, 1);
    void* originalHandle = reinterpret_cast<void*>(1);

    driver->enqueue(original);
    driver->show();
    mock.simulateTransmitDone(originalHandle);
    FL_REQUIRE(driver->poll() == DriverState::READY);

    FL_SUBCASE("channel wait times out") {
        mock.setWaitFailure(true);
        driver->enqueue(replacement);
        driver->show();
        mock.setWaitFailure(false);
    }
    FL_SUBCASE("hardware deletion fails") {
        mock.setChannelDeletionFailure(true);
        driver->enqueue(replacement);
        driver->show();
        mock.setChannelDeletionFailure(false);
    }
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 1u);
    FL_REQUIRE(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(replacement->isInUse());
    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getEncoderCount(), 1u);

    driver->enqueue(original);
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 2u);
    mock.simulateTransmitDone(originalHandle);
    FL_REQUIRE(driver->poll() == DriverState::READY);

    driver->enqueue(replacement);
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 3u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_REQUIRE(driver->poll() == DriverState::READY);
    driver.reset();
    FL_CHECK_EQ(mock.getChannelCount(), 0u);
}

FL_TEST_CASE("RMT5 driver - network reconfiguration preserves a channel that cannot be deleted") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto& network = fl::detail::NetworkStateTracker::instance();
    auto driver = ChannelEngineRMT::create();
    auto data = createChannelData(18, 1);
    void* originalHandle = reinterpret_cast<void*>(1);

    driver->enqueue(data);
    driver->show();
    mock.simulateTransmitDone(originalHandle);
    FL_REQUIRE(driver->poll() == DriverState::READY);

    const auto rollbacks = fl::detail::RmtMemoryManager::instance().rollbackAllocationCount();
    network.setActive(true);
    mock.setChannelDeletionFailure(true);
    driver->enqueue(data);
    driver->show();
    mock.setChannelDeletionFailure(false);

    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getEncoderCount(), 1u);
    FL_CHECK_EQ(fl::detail::RmtMemoryManager::instance().rollbackAllocationCount(), rollbacks);
    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);
    mock.simulateTransmitDone(originalHandle);
    FL_REQUIRE(driver->poll() == DriverState::READY);

    driver->enqueue(data);
    driver->show();
    network.setActive(false);
    network.hasChanged();

    FL_CHECK_EQ(fl::detail::RmtMemoryManager::instance().rollbackAllocationCount(), rollbacks + 1);
    FL_CHECK_EQ(mock.getTransmissionCount(), 3u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_CHECK(driver->poll() == DriverState::READY);
    driver.reset();
    FL_CHECK_EQ(mock.getChannelCount(), 0u);
}

FL_TEST_CASE("RMT5 driver - network channel reduction retries an undeletable slot") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto& network = fl::detail::NetworkStateTracker::instance();
    auto driver = ChannelEngineRMT::create();
    auto first = createChannelData(18, 2);
    auto second = createChannelData(19, 1);
    void* firstHandle = reinterpret_cast<void*>(1);
    void* secondHandle = reinterpret_cast<void*>(2);

    driver->enqueue(first);
    driver->enqueue(second);
    driver->show();
    mock.simulateTransmitDone(firstHandle);
    mock.simulateTransmitDone(secondHandle);
    FL_REQUIRE(driver->poll() == DriverState::READY);

    const auto waits = mock.getWaitAllDoneCount();
    network.setActive(true);
    mock.setChannelDeletionFailure(true);
    driver->enqueue(second);
    driver->show();
    mock.setChannelDeletionFailure(false);

    FL_CHECK_EQ(mock.getWaitAllDoneCount(), waits + 1);
    FL_CHECK_EQ(mock.getChannelCount(), 2u);
    FL_CHECK_EQ(mock.getEncoderCount(), 2u);
    FL_CHECK_EQ(mock.getTransmissionCount(), 3u);
    mock.simulateTransmitDone(secondHandle);
    FL_CHECK(driver->poll() == DriverState::READY);

    driver->enqueue(first);
    driver->show();
    network.setActive(false);
    network.hasChanged();

    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getEncoderCount(), 1u);
    FL_CHECK_EQ(mock.getTransmissionCount(), 4u);
    mock.simulateTransmitDone(firstHandle);
    mock.simulateTransmitDone(reinterpret_cast<void*>(3));
    FL_CHECK(driver->poll() == DriverState::READY);
    driver.reset();
    FL_CHECK_EQ(mock.getChannelCount(), 0u);
}

FL_TEST_CASE("RMT5 driver - idle hardware takes priority over a vacant slot") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    driver->enqueue(createChannelData(18, 2));
    driver->enqueue(createChannelData(19, 1));
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 2u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_REQUIRE(driver->poll() == DriverState::READY);

    driver->enqueue(createChannelData(-1, 1));
    driver->show();
    FL_REQUIRE(driver->poll() == DriverState::READY);
    FL_REQUIRE_EQ(mock.getChannelCount(), 1u);
    mock.setMaxChannels(1);

    driver->enqueue(createChannelData(20, 1));
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 3u);
    FL_CHECK_EQ(mock.getTransmissionHistory().back().gpio_pin, 20);
    mock.simulateTransmitDone(reinterpret_cast<void*>(3));
    FL_CHECK(driver->poll() == DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - failed strip setup does not drop a valid strip") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    // The larger invalid strip is attempted first; its failure must not stop
    // the valid strip in the same frame from getting a channel.
    auto invalid = createChannelData(-1, 10);
    auto valid = createChannelData(18, 1);
    driver->enqueue(invalid);
    driver->enqueue(valid);
    driver->show();

    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getTransmissionCount(), 1u);

    // Finish the transmission so the driver destructor does not block
    // waiting for READY.
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    for (int i = 0; i < 10 && valid->isInUse(); i++) {
        driver->poll();
    }
    FL_CHECK_FALSE(valid->isInUse());
    FL_CHECK(driver->poll() == DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - strips beyond channel limit wait for a free channel") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    mock.setMaxChannels(1);
    auto driver = ChannelEngineRMT::create();

    // Two strips, one hardware channel: the second must wait for the first
    // to finish and then reuse its channel, not be dropped.
    auto first = createChannelData(18, 10);
    auto second = createChannelData(19, 5);
    driver->enqueue(first);
    driver->enqueue(second);
    driver->show();

    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    FL_CHECK_EQ(mock.getTransmissionCount(), 1u);
    FL_CHECK(driver->poll() == DriverState::BUSY);
    FL_REQUIRE_EQ(mock.getTransmissionHistory().size(), 1u);
    FL_CHECK_EQ(mock.getTransmissionHistory()[0].gpio_pin, 18);
    FL_CHECK_EQ(mock.getTransmissionHistory()[0].buffer_size, 30u);

    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    // The second strip is now on the wire and nothing is pending.
    FL_CHECK(driver->poll() == DriverState::DRAINING);
    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);
    FL_REQUIRE_EQ(mock.getTransmissionHistory().size(), 2u);
    FL_CHECK_EQ(mock.getTransmissionHistory()[1].gpio_pin, 19);
    FL_CHECK_EQ(mock.getTransmissionHistory()[1].buffer_size, 15u);

    for (int i = 0; i < 10 && second->isInUse(); i++) {
        mock.simulateTransmitDone(reinterpret_cast<void*>(2));
        driver->poll();
    }
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
    FL_CHECK(driver->poll() == DriverState::READY);
    mock.setMaxChannels(0);
}

FL_TEST_CASE("RMT5 driver - failed submission and disable preserve channel ownership") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto data = createChannelData(18, 1);
    void* handle = reinterpret_cast<void*>(1);

    mock.setTransmitFailure(true);
    mock.setDisableFailure(true);
    driver->enqueue(data);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 0u);
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(data->isInUse());
    FL_CHECK(mock.isChannelEnabled(handle));

    mock.setTransmitFailure(false);
    mock.setDisableFailure(false);
    driver.reset();
    FL_CHECK_EQ(mock.getChannelCount(), 0u);
}

FL_TEST_CASE("RMT5 driver - failed submission releases the frame") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto first = createChannelData(18, 10);
    auto second = createChannelData(19, 1);
    auto retry = first;
    uintptr_t retryHandle = 1;

    mock.setTransmitFailure(true);
    driver->enqueue(first);
    FL_SUBCASE("single strip") {}
    FL_SUBCASE("multiple strips") {
        driver->enqueue(second);
        // The mock assigns a new handle when the idle channel changes pins.
        retry = second;
        retryHandle = 2;
    }
    driver->show();
    mock.setTransmitFailure(false);

    FL_CHECK_EQ(mock.getTransmissionCount(), 0u);
    FL_REQUIRE(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());

    driver->enqueue(retry);
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 1u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(retryHandle));
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(retry->isInUse());
}

//=============================================================================
// Test Suite: Edge Cases
//=============================================================================

FL_TEST_CASE("RMT5 driver - empty enqueue") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    // Call show without enqueue
    driver->show();

    // Should not crash, no transmissions
    FL_CHECK(mock.getTransmissionCount() == 0);
}

// TODO: Re-enable after fixing driver 0-byte buffer handling
// FL_TEST_CASE("RMT5 driver - zero LED channel") {
//     resetMock();
//     auto driver = ChannelEngineRMT::create();

//     auto ch = createChannelData(18, 0);  // Zero LEDs

//     driver->enqueue(ch);
//     driver->show();

//     // Should handle gracefully (0 bytes = no transmission, immediate READY)
//     auto state = driver->poll();
//     FL_CHECK(state == DriverState::READY);
//     FL_CHECK(ch->isInUse() == false);  // Should not be in use (no valid transmission)
// }

FL_TEST_CASE("RMT5 driver - show timeout preserves the active frame") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto active = createChannelData(18, 1);
    auto next = createChannelData(19, 1);

    driver->enqueue(active);
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 1u);
    FL_CHECK(active->isInUse());

    driver->enqueue(next);
    fl::u32 nowMs = 0;
    fl::inject_time_provider([&nowMs]() { return nowMs += 1000; });
    driver->show();
    fl::clear_time_provider();

    const auto submitted = mock.getTransmissionCount();
    FL_CHECK_EQ(submitted, 1u);
    FL_CHECK(active->isInUse());
    FL_CHECK_FALSE(next->isInUse());

    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    if (submitted > 1) {
        // Drain the unexpected transfer so the regression can fail immediately.
        mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    }
    FL_REQUIRE(driver->poll() == DriverState::READY);
    FL_REQUIRE_FALSE(active->isInUse());

    driver->enqueue(next);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(next->isInUse());
}

FL_TEST_CASE("RMT5 driver - rapid show() calls") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    auto ch = createChannelData(18, 1);

    // Multiple rapid show() calls should not crash
    driver->enqueue(ch);
    driver->show();

    // Subsequent show() calls while busy should be handled
    // (exact behavior depends on driver implementation - just verify no crash)
    auto state = driver->poll();
    (void)state; // Don't care about state - just verifying no crash

    FL_CHECK(mock.getChannelCount() >= 0);

    // Complete transmission to allow clean shutdown
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
        for (int i = 0; i < 10 && ch->isInUse(); i++) {
            driver->poll();
        }
    }
}

//=============================================================================
// Test Suite: DMA sizing policy (#5161 follow-up)
//=============================================================================

namespace {

/// @brief Run one frame through a fresh driver and report whether the channel
/// that carried it was created with DMA. Resets the mock and the memory
/// manager's fake DMA capability on the way out so tests stay isolated.
bool frameUsedDma(size_t num_leds, bool dma_hardware_available) {
    resetMock();
    auto& memMgr = RmtMemoryManager::instance();
    memMgr.setDMAHardwareAvailable(dma_hardware_available);

    auto driver = ChannelEngineRMT::create();
    auto ch = createChannelData(18, num_leds);

    driver->enqueue(ch);
    driver->show();

    auto& mock = Rmt5PeripheralMock::instance();
    bool used_dma = false;
    const auto& history = mock.getTransmissionHistory();
    if (!history.empty()) {
        used_dma = history.back().used_dma;
        mock.simulateTransmitDone(reinterpret_cast<void*>(1));
        for (int i = 0; i < 10 && ch->isInUse(); i++) {
            driver->poll();
        }
    }

    memMgr.setDMAHardwareAvailable(false);
    return used_dma;
}

} // anonymous namespace

FL_TEST_CASE("RMT5 driver - short strip stays on on-chip RMT memory") {
    // 12 LEDs = 36 bytes = 288 RMT symbols, well under the 1024-symbol DMA
    // block. Even with DMA hardware present the driver must not take the DMA
    // channel: it would cost a 4 KB heap block and stream nothing.
    FL_CHECK(frameUsedDma(12, true) == false);
}

FL_TEST_CASE("RMT5 driver - long strip still uses DMA when available") {
    // 300 LEDs = 900 bytes = 7200 RMT symbols, past the DMA block size, so the
    // streaming backend is still worth its heap.
    FL_CHECK(frameUsedDma(300, true) == true);
}

FL_TEST_CASE("RMT5 driver - network reconfiguration keeps the DMA channel") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto& network = fl::detail::NetworkStateTracker::instance();
    auto& memMgr = RmtMemoryManager::instance();
    memMgr.setDMAHardwareAvailable(true);
    auto driver = ChannelEngineRMT::create();
    auto data = createChannelData(18, 300);

    driver->enqueue(data);
    driver->show();
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    FL_CHECK(driver->poll() == DriverState::READY);

    network.setActive(true);
    driver->enqueue(data);
    driver->show();
    network.setActive(false);
    network.hasChanged();
    memMgr.setDMAHardwareAvailable(false);

    const auto& history = mock.getTransmissionHistory();
    FL_CHECK_EQ(history.size(), 2u);
    if (history.size() == 2) {
        FL_CHECK(history.front().used_dma);
        FL_CHECK(history.back().used_dma);
    }
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_CHECK(driver->poll() == DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - network channel reduction evicts on-chip hardware before DMA") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto& network = fl::detail::NetworkStateTracker::instance();
    auto& memMgr = RmtMemoryManager::instance();
    auto driver = ChannelEngineRMT::create();
    auto onChip = createChannelData(18, 301);
    auto dma = createChannelData(19, 300);
    void* onChipHandle = reinterpret_cast<void*>(1);
    void* dmaHandle = reinterpret_cast<void*>(2);

    // Seed an on-chip channel, then put the DMA channel in the last slot.
    driver->enqueue(onChip);
    driver->show();
    mock.simulateTransmitDone(onChipHandle);
    FL_REQUIRE(driver->poll() == DriverState::READY);
    memMgr.setDMAHardwareAvailable(true);
    driver->enqueue(onChip);
    driver->enqueue(dma);
    driver->show();
    mock.simulateTransmitDone(onChipHandle);
    mock.simulateTransmitDone(dmaHandle);
    FL_CHECK(driver->poll() == DriverState::READY);

    network.setActive(true);
    driver->enqueue(dma);
    driver->show();
    network.setActive(false);
    network.hasChanged();
    memMgr.setDMAHardwareAvailable(false);

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE_EQ(history.size(), 4u);
    FL_CHECK(history[2].used_dma);
    FL_CHECK(history.back().used_dma);
    FL_CHECK_EQ(mock.getChannelCount(), 1u);
    mock.simulateTransmitDone(dmaHandle);
    FL_CHECK(driver->poll() == DriverState::READY);
}

FL_TEST_CASE("RMT5 driver - no DMA hardware means no DMA channel") {
    // Sanity check on the stub hook itself: with no DMA hardware the channel is
    // always on-chip, however long the strip.
    FL_CHECK(frameUsedDma(300, false) == false);
}

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
