/// @file lcd_rgb_channel_engine.cpp
/// @brief Unit tests for LCD RGB channel engine
///
/// Tests the ChannelEngineLcdRgb implementation using the mock peripheral.
/// Tests:
/// - Channel engine creation and lifecycle
/// - Single and multi-channel transmission
/// - State machine transitions
/// - Error handling
///
/// These tests run ONLY on stub platforms (host-based testing).

#ifdef FASTLED_STUB_IMPL  // Tests only run on stub platform

#include "doctest.h"
#include "platforms/esp/32/drivers/lcd_cam/channel_engine_lcd_rgb.h"
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

TEST_CASE("ChannelEngineLcdRgb - creation") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb engine(peripheral);

    CHECK(engine.getName() != nullptr);
    CHECK(fl::strcmp(engine.getName(), "LCD_RGB") == 0);
}

TEST_CASE("ChannelEngineLcdRgb - initial state is READY") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb engine(peripheral);

    CHECK(engine.poll() == IChannelEngine::EngineState::READY);
}

//=============================================================================
// Test Suite: Single Channel Transmission
//=============================================================================

TEST_CASE("ChannelEngineLcdRgb - single channel transmission") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb engine(peripheral);

    // Create channel data
    auto channelData = createTestChannelData(1, 10);

    // Enqueue and show
    engine.enqueue(channelData);
    engine.show();

    // Wait for completion - yield to allow simulation thread to process
    while (engine.poll() != IChannelEngine::EngineState::READY) {
        fl::this_thread::yield();
    }

    // Verify mock received data
    auto& mock = LcdRgbPeripheralMock::instance();
    CHECK(mock.getDrawCount() >= 1);
}

TEST_CASE("ChannelEngineLcdRgb - empty enqueue does not transmit") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb engine(peripheral);

    // Show with no enqueued data
    engine.show();

    // Should still be ready
    CHECK(engine.poll() == IChannelEngine::EngineState::READY);

    // Mock should not have been called
    auto& mock = LcdRgbPeripheralMock::instance();
    CHECK(mock.getDrawCount() == 0);
}

//=============================================================================
// Test Suite: Multi-Channel Transmission
//=============================================================================

TEST_CASE("ChannelEngineLcdRgb - multi-channel transmission") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb engine(peripheral);

    // Create multiple channels
    auto channel1 = createTestChannelData(1, 10);
    auto channel2 = createTestChannelData(2, 10);
    auto channel3 = createTestChannelData(3, 10);

    // Enqueue all
    engine.enqueue(channel1);
    engine.enqueue(channel2);
    engine.enqueue(channel3);
    engine.show();

    // Wait for completion - yield to allow simulation thread to process
    while (engine.poll() != IChannelEngine::EngineState::READY) {
        fl::this_thread::yield();
    }

    // Verify transmission occurred
    auto& mock = LcdRgbPeripheralMock::instance();
    CHECK(mock.getDrawCount() >= 1);
}

//=============================================================================
// Test Suite: State Machine
//=============================================================================

TEST_CASE("ChannelEngineLcdRgb - state transitions") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb engine(peripheral);

    // Initial state
    CHECK(engine.poll() == IChannelEngine::EngineState::READY);

    // Enqueue data
    auto channelData = createTestChannelData(1, 50);
    engine.enqueue(channelData);

    // Still ready (not transmitted yet)
    CHECK(engine.poll() == IChannelEngine::EngineState::READY);

    // Start transmission
    engine.show();

    // Wait and verify completion - yield to allow simulation thread to process
    int iterations = 0;
    const int maxIterations = 1000;
    while (engine.poll() != IChannelEngine::EngineState::READY && iterations < maxIterations) {
        fl::this_thread::yield();
        iterations++;
    }

    CHECK(iterations < maxIterations);  // Didn't timeout
    CHECK(engine.poll() == IChannelEngine::EngineState::READY);  // Back to ready
}

//=============================================================================
// Test Suite: Error Handling
//=============================================================================

TEST_CASE("ChannelEngineLcdRgb - draw failure handling") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineLcdRgb engine(peripheral);

    // Inject failure
    auto& mock = LcdRgbPeripheralMock::instance();
    mock.setDrawFailure(true);

    // Create and enqueue data
    auto channelData = createTestChannelData(1, 10);
    engine.enqueue(channelData);
    engine.show();

    // Should return to READY (failed silently)
    // Note: The engine handles draw failure by returning to READY state
    int iterations = 0;
    const int maxIterations = 100;
    while (engine.poll() != IChannelEngine::EngineState::READY && iterations < maxIterations) {
        fl::this_thread::yield();
        iterations++;
    }

    CHECK(engine.poll() == IChannelEngine::EngineState::READY);
}

#endif // FASTLED_STUB_IMPL
