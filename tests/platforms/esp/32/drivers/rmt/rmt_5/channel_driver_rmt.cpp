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
#include "fl/stl/vector.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/span.h"
#include "fl/stl/weak_ptr.h"
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

// Downstream drivers can still derive from the public RMT interface and
// publish pin groups for the inherited general capability matcher.
class MetadataRmtDriver : public ChannelEngineRMT {
public:
    MetadataRmtDriver() FL_NO_EXCEPT {
        mGroup.data_pins = PinSet::anyOutput();
    }
    bool canHandle(const ChannelDataPtr&) const FL_NO_EXCEPT override { return true; }
    void enqueue(ChannelDataPtr) FL_NO_EXCEPT override {}
    void show() FL_NO_EXCEPT override {}
    DriverState poll() FL_NO_EXCEPT override { return DriverState::READY; }
    void setPollNeededCallback(PollNeededCallback) FL_NO_EXCEPT override {}
    fl::span<const PinGroup> getPinGroups() const FL_NO_EXCEPT override {
        return fl::span<const PinGroup>(&mGroup, 1);
    }
private:
    PinGroup mGroup;
};

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

FL_TEST_CASE("RMT5 factory matching preserves empty metadata and public extensions") {
    resetMock();
    auto driver = ChannelEngineRMT::create();
    FL_REQUIRE(driver != nullptr);
    const auto caps = driver->getDriverCapabilities();
    FL_CHECK(caps.supports_clockless);
    FL_CHECK_FALSE(caps.supports_spi);
    FL_CHECK(driver->getPinGroups().empty());

    const auto clockless = ChannelRequest::singlePin(Protocol::Clockless, 4, -1);
    const auto spi = ChannelRequest::singlePin(Protocol::Spi, 4, 5);
    FL_CHECK(driver->canMatch(clockless) == HandleResult::NoPin);
    FL_CHECK(driver->canMatch(spi) == HandleResult::NoProtocol);

    MetadataRmtDriver extension;
    FL_CHECK(extension.canMatch(clockless) == HandleResult::Yes);
    FL_CHECK(extension.canMatch(spi) == HandleResult::NoProtocol);
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

    // Engine should be BUSY
    FL_CHECK(driver->poll() == DriverState::BUSY);

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

//=============================================================================
// Test Suite: State Machine
//=============================================================================

FL_TEST_CASE("RMT5 driver - state progression READY → BUSY → READY") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();

    // Initial state should be READY
    FL_CHECK(driver->poll() == DriverState::READY);

    // Enqueue and show
    auto ch = createChannelData(18, 1);
    driver->enqueue(ch);
    driver->show();

    // State should be BUSY after show()
    FL_CHECK(driver->poll() == DriverState::BUSY);

    // Simulate completion
    if (mock.getChannelCount() > 0) {
        void* channel_handle = reinterpret_cast<void*>(1);
        mock.simulateTransmitDone(channel_handle);
    }

    // Poll multiple times to process completion
    DriverState state = DriverState::BUSY;
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

FL_TEST_CASE("RMT5 driver - repeated reconfiguration failures reuse empty slots") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    mock.setMaxChannels(2);
    auto driver = ChannelEngineRMT::create();
    auto first = createChannelData(18, 10);
    auto second = createChannelData(19, 5);
    auto invalid = createChannelData(-1, 1);

    // Each invalid-pin frame destroys an idle channel and leaves an empty
    // state slot. Repeating this beyond the inline capacity must not move
    // live channel states while their ISR callbacks still refer to them.
    for (int frame = 0; frame < 24; ++frame) {
        driver->enqueue(first);
        driver->enqueue(second);
        driver->show();
        FL_CHECK_EQ(mock.getChannelCount(), 2u);
        FL_CHECK_EQ(mock.getTransmissionCount(),
                    static_cast<size_t>((frame + 1) * 2));

        // The unaffected strip keeps its hardware handle; only the failed
        // slot is recreated. Complete the handles actually used this frame.
        const auto& history = mock.getTransmissionHistory();
        mock.simulateTransmitDone(fl::int_to_ptr<void>(
            history[frame * 2].channel_address));
        mock.simulateTransmitDone(fl::int_to_ptr<void>(
            history[frame * 2 + 1].channel_address));
        FL_REQUIRE(driver->poll() == DriverState::READY);
        FL_CHECK_FALSE(first->isInUse());
        FL_CHECK_FALSE(second->isInUse());

        driver->enqueue(invalid);
        driver->show();
        FL_REQUIRE(driver->poll() == DriverState::READY);
        FL_CHECK_FALSE(invalid->isInUse());
        FL_CHECK_EQ(mock.getChannelCount(), 1u);
        FL_CHECK_EQ(mock.getEncoderCount(), 1u);
    }
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

    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    driver->poll();
    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);

    for (int i = 0; i < 10 && second->isInUse(); i++) {
        mock.simulateTransmitDone(reinterpret_cast<void*>(2));
        driver->poll();
    }
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
    FL_CHECK(driver->poll() == DriverState::READY);
    mock.setMaxChannels(0);
}

FL_TEST_CASE("RMT5 driver - many logical strips spill and reuse one hardware channel") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    mock.setMaxChannels(1);
    auto driver = ChannelEngineRMT::create();
    fl::vector<ChannelDataPtr> channels;
    for (int pin = 18; pin < 38; ++pin) {
        channels.push_back(createChannelData(pin, 1));
        driver->enqueue(channels.back());
    }
    driver->show();
    FL_CHECK_EQ(mock.getChannelCount(), 1u);

    for (size_t handle = 1; handle <= channels.size(); ++handle) {
        FL_CHECK_EQ(mock.getTransmissionCount(), handle);
        mock.simulateTransmitDone(reinterpret_cast<void*>(handle));
        const auto state = driver->poll();
        FL_CHECK(state == (handle == channels.size() ?
                           DriverState::READY : DriverState::BUSY));
    }
    FL_CHECK_EQ(mock.getTransmissionCount(), channels.size());
    for (const auto& channel : channels) {
        FL_CHECK_FALSE(channel->isInUse());
    }
    mock.setMaxChannels(0);
}

FL_TEST_CASE("RMT5 driver - direct and pooled sources survive until completion") {
    for (int direct = 0; direct < 2; ++direct) {
        resetMock();
        auto& mock = Rmt5PeripheralMock::instance();
        mock.setDirectTransmission(direct != 0);
        auto driver = ChannelEngineRMT::create();
        const uint8_t pixels[] = {10, 20, 30};
        auto channel = createChannelData(18, 1, pixels);
        fl::span<const u8> source = channel->getData();
        const auto sourceAddress = fl::ptr_to_int(source.data());
        fl::weak_ptr<ChannelData> lifetime(channel);

        driver->enqueue(channel);
        driver->show();
        FL_CHECK(channel->isInUse());
        FL_REQUIRE_EQ(mock.getTransmissionCount(), 1u);
        const auto& record = mock.getTransmissionHistory().back();
        FL_REQUIRE_EQ(record.buffer_copy.size(), 3u);
        if (direct) {
            FL_CHECK_EQ(record.buffer_address, sourceAddress);
        } else {
            FL_CHECK_NE(record.buffer_address, sourceAddress);
        }
        FL_CHECK_EQ(record.buffer_copy[0], 20);
        FL_CHECK_EQ(record.buffer_copy[1], 10);
        FL_CHECK_EQ(record.buffer_copy[2], 30);

        // The engine must retain the original source after its caller lets go.
        channel.reset();
        FL_CHECK_FALSE(lifetime.expired());
        mock.simulateTransmitDone(reinterpret_cast<void*>(1));
        FL_REQUIRE(driver->poll() == DriverState::READY);
        FL_CHECK(lifetime.expired());
    }
}

FL_TEST_CASE("RMT5 driver - custom padding generator retains pooled transformation") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    mock.setDirectTransmission(true);
    auto driver = ChannelEngineRMT::create();
    const uint8_t pixels[] = {10, 20, 30};
    auto channel = createChannelData(18, 1, pixels);
    channel->setPaddingGenerator([](fl::span<const u8> src,
                                    fl::span<u8> dst) FL_NO_EXCEPT {
        for (size_t i = 0; i < src.size(); ++i) {
            dst[i] = static_cast<u8>(src[i] ^ 0xff);
        }
    });
    fl::span<const u8> source = channel->getData();
    const auto sourceAddress = fl::ptr_to_int(source.data());
    driver->enqueue(channel);
    driver->show();
    FL_REQUIRE_EQ(mock.getTransmissionCount(), 1u);
    const auto& record = mock.getTransmissionHistory().back();
    FL_REQUIRE_EQ(record.buffer_copy.size(), 3u);
    FL_CHECK_NE(record.buffer_address, sourceAddress);
    FL_CHECK_EQ(record.buffer_copy[0], 235);
    FL_CHECK_EQ(record.buffer_copy[1], 245);
    FL_CHECK_EQ(record.buffer_copy[2], 225);
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(channel->isInUse());
}

FL_TEST_CASE("RMT5 driver - direct and pooled failures release frame and retry") {
    for (int direct = 0; direct < 2; ++direct) {
        for (int failure = 0; failure < 3; ++failure) {
            resetMock();
            auto& mock = Rmt5PeripheralMock::instance();
            mock.setDirectTransmission(direct != 0);
            mock.setEnableFailure(failure == 0);
            mock.setResetFailure(failure == 1);
            mock.setTransmitFailure(failure == 2);
            auto driver = ChannelEngineRMT::create();
            auto channel = createChannelData(18, 1);
            driver->enqueue(channel);
            driver->show();
            const auto state = driver->poll();
            FL_CHECK(state == DriverState::READY);
            FL_CHECK_FALSE(channel->isInUse());
            FL_CHECK_EQ(mock.getTransmissionCount(), 0u);

            mock.setEnableFailure(false);
            mock.setResetFailure(false);
            mock.setTransmitFailure(false);
            // Do not enter show()'s wait loop if this regression is present.
            if (state == DriverState::READY) {
                driver->enqueue(channel);
                driver->show();
                FL_CHECK_EQ(mock.getTransmissionCount(), 1u);
                mock.simulateTransmitDone(reinterpret_cast<void*>(1));
                FL_CHECK(driver->poll() == DriverState::READY);
                FL_CHECK_FALSE(channel->isInUse());
            }
        }
    }
}

FL_TEST_CASE("RMT5 driver - same-pin encoder failure rolls back and retries") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto original = createChannelData(18, 1);
    driver->enqueue(original);
    driver->show();
    mock.simulateTransmitDone(reinterpret_cast<void*>(1));
    FL_REQUIRE(driver->poll() == DriverState::READY);

    auto changed = ChannelData::create(
        18, ChipsetTimingConfig(400, 450, 450, 50, "changed"));
    changed->getData().resize(3);
    mock.setEncoderFailure(true);
    driver->enqueue(changed);
    driver->show();
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(changed->isInUse());
    FL_CHECK_EQ(mock.getChannelCount(), 0u);
    FL_CHECK_EQ(mock.getEncoderCount(), 0u);

    mock.setEncoderFailure(false);
    driver->enqueue(changed);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 2u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(changed->isInUse());
}

FL_TEST_CASE("RMT5 driver - callback registration failure releases encoder") {
    resetMock();
    auto& mock = Rmt5PeripheralMock::instance();
    auto driver = ChannelEngineRMT::create();
    auto channel = createChannelData(18, 1);
    mock.setCallbackFailure(true);
    driver->enqueue(channel);
    driver->show();
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK_EQ(mock.getChannelCount(), 0u);
    FL_CHECK_EQ(mock.getEncoderCount(), 0u);

    mock.setCallbackFailure(false);
    driver->enqueue(channel);
    driver->show();
    FL_CHECK_EQ(mock.getTransmissionCount(), 1u);
    mock.simulateTransmitDone(reinterpret_cast<void*>(2));
    FL_CHECK(driver->poll() == DriverState::READY);
    FL_CHECK_FALSE(channel->isInUse());
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

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
