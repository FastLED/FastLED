/// @file lcd_rgb_channel_engine.cpp
/// @brief Unit tests for LCD RGB channel driver
///
/// Tests the ChannelEngineLcdRgb implementation using the mock peripheral.
/// Tests:
/// - Channel driver creation and lifecycle
/// - Single and multi-channel transmission
/// - State machine transitions
/// - Error handling
///
/// These tests run ONLY on stub platforms (host-based testing).

#ifdef FASTLED_STUB_IMPL  // Tests only run on stub platform

#include "test.h"
#include "platforms/esp/32/drivers/lcd_cam/channel_driver_lcd_rgb.h"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_mock.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/vector.h"
#include "fl/stl/thread.h"

using namespace fl;
using namespace fl::detail;

namespace {

/// @brief Helper to create default timing config for WS2812
ChipsetTimingConfig getWS2812Timing() {
    return ChipsetTimingConfig(350, 800, 450, 50, "WS2812B");
}

/// @brief Reset mock state between tests
void resetMockState() {
    auto& mock = LcdRgbPeripheralMock::instance();
    mock.reset();
    mock.setDrawDelay(0);  // Instant completion for unit tests
}

/// @brief Create mock peripheral as shared_ptr
fl::shared_ptr<ILcdRgbPeripheral> createMockPeripheral() {
    // Wrap the singleton mock in a shared_ptr
    // The wrapper delegates to the singleton without owning it
    class MockWrapper : public ILcdRgbPeripheral {
    public:
        bool initialize(const LcdRgbPeripheralConfig& config) override {
            return LcdRgbPeripheralMock::instance().initialize(config);
        }
        void deinitialize() override {
            LcdRgbPeripheralMock::instance().deinitialize();
        }
        bool isInitialized() const override {
            return LcdRgbPeripheralMock::instance().isInitialized();
        }
        uint16_t* allocateFrameBuffer(size_t size_bytes) override {
            return LcdRgbPeripheralMock::instance().allocateFrameBuffer(size_bytes);
        }
        void freeFrameBuffer(uint16_t* buffer) override {
            LcdRgbPeripheralMock::instance().freeFrameBuffer(buffer);
        }
        bool drawFrame(const uint16_t* buffer, size_t size_bytes) override {
            return LcdRgbPeripheralMock::instance().drawFrame(buffer, size_bytes);
        }
        bool waitFrameDone(uint32_t timeout_ms) override {
            return LcdRgbPeripheralMock::instance().waitFrameDone(timeout_ms);
        }
        bool isBusy() const override {
            return LcdRgbPeripheralMock::instance().isBusy();
        }
        bool registerDrawCallback(void* callback, void* user_ctx) override {
            return LcdRgbPeripheralMock::instance().registerDrawCallback(callback, user_ctx);
        }
        const LcdRgbPeripheralConfig& getConfig() const override {
            return LcdRgbPeripheralMock::instance().getConfig();
        }
        uint64_t getMicroseconds() override {
            return LcdRgbPeripheralMock::instance().getMicroseconds();
        }
        void delay(uint32_t ms) override {
            LcdRgbPeripheralMock::instance().delay(ms);
        }
    };

    return fl::make_shared<MockWrapper>();
}

/// @brief Create channel data with RGB pattern
ChannelDataPtr createTestChannelData(int pin, size_t numLeds) {
    fl::vector_psram<uint8_t> data;
    data.resize(numLeds * 3);  // RGB

    // Fill with test pattern
    for (size_t i = 0; i < numLeds; i++) {
        data[i * 3 + 0] = static_cast<uint8_t>(i % 256);        // R
        data[i * 3 + 1] = static_cast<uint8_t>((i * 2) % 256);  // G
        data[i * 3 + 2] = static_cast<uint8_t>((i * 3) % 256);  // B
    }

    return ChannelData::create(pin, getWS2812Timing(), fl::move(data));
}

} // anonymous namespace

//=============================================================================
// Test Suite: Channel Engine Creation
//=============================================================================

FL_TEST_CASE("ChannelEngineLcdRgb - creation") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb driver(peripheral);

    FL_CHECK(driver.getName() == "LCD_RGB");
}

FL_TEST_CASE("ChannelEngineLcdRgb - initial state is READY") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb driver(peripheral);

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Test Suite: Single Channel Transmission
//=============================================================================

FL_TEST_CASE("ChannelEngineLcdRgb - single channel transmission") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb driver(peripheral);

    // Create channel data
    auto channelData = createTestChannelData(1, 10);

    // Enqueue and show
    driver.enqueue(channelData);
    driver.show();

    // Wait for completion - yield to allow simulation thread to process
    while (driver.poll() != IChannelDriver::DriverState::READY) {
        fl::this_thread::yield();
    }

    // Verify mock received data
    auto& mock = LcdRgbPeripheralMock::instance();
    FL_CHECK(mock.getDrawCount() >= 1);
}

FL_TEST_CASE("ChannelEngineLcdRgb - empty enqueue does not transmit") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb driver(peripheral);

    // Show with no enqueued data
    driver.show();

    // Should still be ready
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // Mock should not have been called
    auto& mock = LcdRgbPeripheralMock::instance();
    FL_CHECK(mock.getDrawCount() == 0);
}

//=============================================================================
// Test Suite: Multi-Channel Transmission
//=============================================================================

FL_TEST_CASE("ChannelEngineLcdRgb - multi-channel transmission") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb driver(peripheral);

    // Create multiple channels
    auto channel1 = createTestChannelData(1, 10);
    auto channel2 = createTestChannelData(2, 10);
    auto channel3 = createTestChannelData(3, 10);

    // Enqueue all
    driver.enqueue(channel1);
    driver.enqueue(channel2);
    driver.enqueue(channel3);
    driver.show();

    // Wait for completion - yield to allow simulation thread to process
    while (driver.poll() != IChannelDriver::DriverState::READY) {
        fl::this_thread::yield();
    }

    // Verify transmission occurred
    auto& mock = LcdRgbPeripheralMock::instance();
    FL_CHECK(mock.getDrawCount() >= 1);
}

//=============================================================================
// Test Suite: State Machine
//=============================================================================

FL_TEST_CASE("ChannelEngineLcdRgb - state transitions") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb driver(peripheral);

    // Initial state
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // Enqueue data
    auto channelData = createTestChannelData(1, 50);
    driver.enqueue(channelData);

    // Still ready (not transmitted yet)
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // Start transmission
    driver.show();

    // Wait and verify completion - yield to allow simulation thread to process
    int iterations = 0;
    const int maxIterations = 1000;
    while (driver.poll() != IChannelDriver::DriverState::READY && iterations < maxIterations) {
        fl::this_thread::yield();
        iterations++;
    }

    FL_CHECK(iterations < maxIterations);  // Didn't timeout
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);  // Back to ready
}

//=============================================================================
// Test Suite: Error Handling
//=============================================================================

FL_TEST_CASE("ChannelEngineLcdRgb - draw failure handling") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb driver(peripheral);

    // Inject failure
    auto& mock = LcdRgbPeripheralMock::instance();
    mock.setDrawFailure(true);

    // Create and enqueue data
    auto channelData = createTestChannelData(1, 10);
    driver.enqueue(channelData);
    driver.show();

    // Should return to READY (failed silently)
    // Note: The driver handles draw failure by returning to READY state
    int iterations = 0;
    const int maxIterations = 100;
    while (driver.poll() != IChannelDriver::DriverState::READY && iterations < maxIterations) {
        fl::this_thread::yield();
        iterations++;
    }

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

#endif // FASTLED_STUB_IMPL
