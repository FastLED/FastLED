/// @file i2s_channel_engine.cpp
/// @brief Unit tests for I2S LCD_CAM channel engine
///
/// Tests the ChannelEngineI2S implementation using the mock peripheral.
/// Tests:
/// - Channel engine creation and lifecycle
/// - Single and multi-channel transmission
/// - State machine transitions
/// - Error handling
///
/// These tests run ONLY on stub platforms (host-based testing).

#ifdef FASTLED_STUB_IMPL  // Tests only run on stub platform

#include "doctest.h"
#include "platforms/esp/32/drivers/i2s/channel_engine_i2s.h"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.h"
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
    auto& mock = I2sLcdCamPeripheralMock::instance();
    mock.reset();
    mock.setTransmitDelay(0);  // Instant completion for unit tests
}

/// @brief Create mock peripheral as shared_ptr
fl::shared_ptr<II2sLcdCamPeripheral> createMockPeripheral() {
    // Wrap the singleton mock in a shared_ptr
    // The wrapper delegates to the singleton without owning it
    class MockWrapper : public II2sLcdCamPeripheral {
    public:
        bool initialize(const I2sLcdCamConfig& config) override {
            return I2sLcdCamPeripheralMock::instance().initialize(config);
        }
        void deinitialize() override {
            I2sLcdCamPeripheralMock::instance().deinitialize();
        }
        bool isInitialized() const override {
            return I2sLcdCamPeripheralMock::instance().isInitialized();
        }
        uint16_t* allocateBuffer(size_t size_bytes) override {
            return I2sLcdCamPeripheralMock::instance().allocateBuffer(size_bytes);
        }
        void freeBuffer(uint16_t* buffer) override {
            I2sLcdCamPeripheralMock::instance().freeBuffer(buffer);
        }
        bool transmit(const uint16_t* buffer, size_t size_bytes) override {
            return I2sLcdCamPeripheralMock::instance().transmit(buffer, size_bytes);
        }
        bool waitTransmitDone(uint32_t timeout_ms) override {
            return I2sLcdCamPeripheralMock::instance().waitTransmitDone(timeout_ms);
        }
        bool isBusy() const override {
            return I2sLcdCamPeripheralMock::instance().isBusy();
        }
        bool registerTransmitCallback(void* callback, void* user_ctx) override {
            return I2sLcdCamPeripheralMock::instance().registerTransmitCallback(callback, user_ctx);
        }
        const I2sLcdCamConfig& getConfig() const override {
            return I2sLcdCamPeripheralMock::instance().getConfig();
        }
        uint64_t getMicroseconds() override {
            return I2sLcdCamPeripheralMock::instance().getMicroseconds();
        }
        void delay(uint32_t ms) override {
            I2sLcdCamPeripheralMock::instance().delay(ms);
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

TEST_CASE("ChannelEngineI2S - creation") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

    CHECK(engine.getName() != nullptr);
    CHECK(fl::strcmp(engine.getName(), "I2S") == 0);
}

TEST_CASE("ChannelEngineI2S - initial state is READY") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

    CHECK(engine.poll() == IChannelEngine::EngineState::READY);
}

//=============================================================================
// Test Suite: Single Channel Transmission
//=============================================================================

TEST_CASE("ChannelEngineI2S - single channel transmission") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

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
    auto& mock = I2sLcdCamPeripheralMock::instance();
    CHECK(mock.getTransmitCount() >= 1);
}

TEST_CASE("ChannelEngineI2S - empty enqueue does not transmit") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

    // Show with no enqueued data
    engine.show();

    // Should still be ready
    CHECK(engine.poll() == IChannelEngine::EngineState::READY);

    // Mock should not have been called
    auto& mock = I2sLcdCamPeripheralMock::instance();
    CHECK(mock.getTransmitCount() == 0);
}

//=============================================================================
// Test Suite: Multi-Channel Transmission
//=============================================================================

TEST_CASE("ChannelEngineI2S - multi-channel transmission") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

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
    auto& mock = I2sLcdCamPeripheralMock::instance();
    CHECK(mock.getTransmitCount() >= 1);
}

//=============================================================================
// Test Suite: State Machine
//=============================================================================

TEST_CASE("ChannelEngineI2S - state transitions") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

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

TEST_CASE("ChannelEngineI2S - transmit failure handling") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

    // Inject failure
    auto& mock = I2sLcdCamPeripheralMock::instance();
    mock.setTransmitFailure(true);

    // Create and enqueue data
    auto channelData = createTestChannelData(1, 10);
    engine.enqueue(channelData);
    engine.show();

    // Should return to READY (failed silently)
    // Note: The engine handles transmit failure by returning to READY state
    int iterations = 0;
    const int maxIterations = 100;
    while (engine.poll() != IChannelEngine::EngineState::READY && iterations < maxIterations) {
        fl::this_thread::yield();
        iterations++;
    }

    CHECK(engine.poll() == IChannelEngine::EngineState::READY);
}

//=============================================================================
// Test Suite: Multiple Show Cycles
//=============================================================================

TEST_CASE("ChannelEngineI2S - multiple show cycles") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

    // Run multiple show cycles
    for (int cycle = 0; cycle < 3; cycle++) {
        auto channelData = createTestChannelData(1, 20);
        engine.enqueue(channelData);
        engine.show();

        // Wait for completion
        while (engine.poll() != IChannelEngine::EngineState::READY) {
            fl::this_thread::yield();
        }
    }

    // Verify all transmissions occurred
    auto& mock = I2sLcdCamPeripheralMock::instance();
    CHECK(mock.getTransmitCount() >= 3);
}

//=============================================================================
// Test Suite: Varying LED Counts
//=============================================================================

TEST_CASE("ChannelEngineI2S - varying LED counts") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S engine(peripheral);

    // Test with different LED counts
    int led_counts[] = {1, 10, 50, 100};

    for (int count : led_counts) {
        auto& mock = I2sLcdCamPeripheralMock::instance();
        mock.clearTransmitHistory();

        auto channelData = createTestChannelData(1, count);
        engine.enqueue(channelData);
        engine.show();

        // Wait for completion
        while (engine.poll() != IChannelEngine::EngineState::READY) {
            fl::this_thread::yield();
        }

        // Verify transmission occurred
        CHECK(mock.getTransmitHistory().size() >= 1);
    }
}

#endif // FASTLED_STUB_IMPL
