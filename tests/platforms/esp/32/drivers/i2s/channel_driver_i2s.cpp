/// @file i2s_channel_engine.cpp
/// @brief Unit tests for I2S LCD_CAM channel driver
///
/// Tests the ChannelEngineI2S implementation using the mock peripheral.
/// Tests:
/// - Channel driver creation and lifecycle
/// - Single and multi-channel transmission
/// - State machine transitions
/// - Error handling
///
/// These tests run ONLY on stub platforms (host-based testing).

#ifdef FASTLED_STUB_IMPL  // Tests only run on stub platform

#include "test.h"
#include "platforms/esp/32/drivers/i2s/channel_driver_i2s.h"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/vector.h"
#include "fl/stl/thread.h"

FL_TEST_FILE(FL_FILEPATH) {

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

FL_TEST_CASE("ChannelEngineI2S - creation") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    FL_CHECK(driver.getName() == "I2S");
}

FL_TEST_CASE("ChannelEngineI2S - initial state is READY") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelEngineI2S - reset words round up to the requested interval") {
    FL_CHECK_EQ(detail::calculateI2sResetWords(0, 5000000), 0u);
    FL_CHECK_EQ(detail::calculateI2sResetWords(1, 2400001), 3u);
    FL_CHECK_EQ(detail::calculateI2sResetWords(50, 5000000), 250u);
    FL_CHECK_EQ(detail::calculateI2sResetWords(300, 5000000), 1500u);

    size_t sizeBytes = 0;
    const size_t maxBufferBytes =
        (fl::numeric_limits<size_t>::max)() - 63u;
    const u64 maxWords = static_cast<u64>(maxBufferBytes) / sizeof(u16);
    FL_CHECK(
        detail::calculateI2sBufferSize(maxWords, 0, 1000000, sizeBytes));
    FL_CHECK_EQ(sizeBytes, static_cast<size_t>(maxWords * sizeof(u16)));
    FL_CHECK_FALSE(
        detail::calculateI2sBufferSize(maxWords, 1, 1000000, sizeBytes));
}

FL_TEST_CASE("ChannelEngineI2S - DMA buffer includes chipset reset tail") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);
    ChipsetTimingConfig timing(350, 800, 450, 300, "LONG_RESET");
    fl::vector_psram<uint8_t> data;
    data.resize(3);
    auto channelData = ChannelData::create(1, timing, fl::move(data));

    driver.enqueue(channelData);
    driver.show();

    auto& mock = I2sLcdCamPeripheralMock::instance();
    const auto& config = mock.getConfig();
    const size_t dataWords = 3u * 64u;
    const size_t resetWords =
        detail::calculateI2sResetWords(timing.reset_us, config.pclk_hz);
    FL_CHECK_EQ(config.max_transfer_bytes,
                (dataWords + resetWords) * sizeof(u16));
    const fl::span<const u16> transmitted = mock.getLastTransmitData();
    FL_REQUIRE_EQ(transmitted.size(), dataWords + resetWords);
    for (size_t i = dataWords; i < transmitted.size(); ++i) {
        FL_CHECK_EQ(transmitted[i], 0u);
    }

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Test Suite: Single Channel Transmission
//=============================================================================

FL_TEST_CASE("ChannelEngineI2S - single channel transmission") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

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
    auto& mock = I2sLcdCamPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() >= 1);
}

FL_TEST_CASE("ChannelEngineI2S - empty enqueue does not transmit") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    // Show with no enqueued data
    driver.show();

    // Should still be ready
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // Mock should not have been called
    auto& mock = I2sLcdCamPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() == 0);
}

//=============================================================================
// Test Suite: Multi-Channel Transmission
//=============================================================================

FL_TEST_CASE("ChannelEngineI2S - multi-channel transmission") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

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
    auto& mock = I2sLcdCamPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() >= 1);
}

//=============================================================================
// Test Suite: State Machine
//=============================================================================

FL_TEST_CASE("ChannelEngineI2S - state transitions") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    // Initial state
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // Enqueue data
    auto channelData = createTestChannelData(1, 50);
    driver.enqueue(channelData);

    // Still ready (not transmitted yet)
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // Start transmission
    driver.show();

    // Deterministically complete transmission via mock injection
    // (don't rely on simulation thread scheduling which causes flaky tests)
    auto& mock = I2sLcdCamPeripheralMock::instance();
    mock.simulateTransmitComplete();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);  // Back to ready
}

//=============================================================================
// Test Suite: Error Handling
//=============================================================================

FL_TEST_CASE("ChannelEngineI2S - transmit failure handling") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    // Inject failure
    auto& mock = I2sLcdCamPeripheralMock::instance();
    mock.setTransmitFailure(true);

    // Create and enqueue data
    auto channelData = createTestChannelData(1, 10);
    driver.enqueue(channelData);
    driver.show();

    // Should return to READY (failed silently)
    // Note: The driver handles transmit failure by returning to READY state
    int iterations = 0;
    const int maxIterations = 100;
    while (driver.poll() != IChannelDriver::DriverState::READY && iterations < maxIterations) {
        fl::this_thread::yield();
        iterations++;
    }

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Test Suite: Multiple Show Cycles
//=============================================================================

FL_TEST_CASE("ChannelEngineI2S - multiple show cycles") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    // Run multiple show cycles
    for (int cycle = 0; cycle < 3; cycle++) {
        auto channelData = createTestChannelData(1, 20);
        driver.enqueue(channelData);
        driver.show();

        // Wait for completion
        while (driver.poll() != IChannelDriver::DriverState::READY) {
            fl::this_thread::yield();
        }
    }

    // Verify all transmissions occurred
    auto& mock = I2sLcdCamPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() >= 3);
}

//=============================================================================
// Test Suite: Varying LED Counts
//=============================================================================

FL_TEST_CASE("ChannelEngineI2S - varying LED counts") {
    resetMockState();

    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    // Test with different LED counts
    int led_counts[] = {1, 10, 50, 100};

    for (int count : led_counts) {
        auto& mock = I2sLcdCamPeripheralMock::instance();
        mock.clearTransmitHistory();

        auto channelData = createTestChannelData(1, count);
        driver.enqueue(channelData);
        driver.show();

        // Wait for completion
        while (driver.poll() != IChannelDriver::DriverState::READY) {
            fl::this_thread::yield();
        }

        // Verify transmission occurred
        FL_CHECK(mock.getTransmitHistory().size() >= 1);
    }
}

//=============================================================================
// Stress Test Helpers
//=============================================================================

namespace {

/// Different timing so we can test chipset-group splitting
ChipsetTimingConfig getSK6812Timing() {
    return ChipsetTimingConfig(300, 600, 900, 80, "SK6812");
}

/// Wait for driver to become READY; returns false on timeout
bool waitReady(ChannelEngineI2S& driver, int maxIter = 5000) {
    for (int i = 0; i < maxIter; i++) {
        if (driver.poll() == IChannelDriver::DriverState::READY)
            return true;
        fl::this_thread::yield();
    }
    return false;
}

} // anonymous namespace

//=============================================================================
// Stress: Multi-lane with varying LED lengths
//=============================================================================

FL_TEST_CASE("StressI2S - 4 lanes different lengths") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    driver.enqueue(createTestChannelData(1, 10));
    driver.enqueue(createTestChannelData(2, 50));
    driver.enqueue(createTestChannelData(3, 100));
    driver.enqueue(createTestChannelData(4, 25));
    driver.show();

    FL_REQUIRE(waitReady(driver));

    auto& mock = I2sLcdCamPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() >= 1);
    FL_CHECK(mock.getConfig().num_lanes == 4);
}

FL_TEST_CASE("StressI2S - 8 lanes ascending lengths") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    int lengths[] = {5, 15, 30, 60, 120, 200, 300, 500};
    for (int i = 0; i < 8; i++) {
        driver.enqueue(createTestChannelData(i + 1, lengths[i]));
    }
    driver.show();

    FL_REQUIRE(waitReady(driver));

    auto& mock = I2sLcdCamPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() >= 1);
    FL_CHECK(mock.getConfig().num_lanes == 8);
}

//=============================================================================
// Stress: Extreme length disparity
//=============================================================================

FL_TEST_CASE("StressI2S - extreme disparity 1 vs 1000 LEDs") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    driver.enqueue(createTestChannelData(1, 1));
    driver.enqueue(createTestChannelData(2, 1000));
    driver.show();

    FL_REQUIRE(waitReady(driver));

    auto& mock = I2sLcdCamPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() >= 1);

    // Transmitted buffer must be large enough for 1000-LED lane
    const auto& history = mock.getTransmitHistory();
    FL_REQUIRE(history.size() >= 1);
    // Wave8: 1000 LEDs * 3 bytes * 64 words * 2 bytes/word + reset
    size_t min_expected = 1000 * 3 * 64 * sizeof(uint16_t);
    FL_CHECK(history[0].size_bytes >= min_expected);
}

FL_TEST_CASE("StressI2S - extreme disparity 1 vs 500 three lanes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    driver.enqueue(createTestChannelData(1, 1));
    driver.enqueue(createTestChannelData(2, 500));
    driver.enqueue(createTestChannelData(3, 3));
    driver.show();

    FL_REQUIRE(waitReady(driver));
    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 1);
}

//=============================================================================
// Stress: Max lane count (16)
//=============================================================================

FL_TEST_CASE("StressI2S - 16 lanes varying lengths") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    for (int i = 0; i < 16; i++) {
        driver.enqueue(createTestChannelData(i + 1, (i + 1) * 10));
    }
    driver.show();

    FL_REQUIRE(waitReady(driver));

    auto& mock = I2sLcdCamPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() >= 1);
    FL_CHECK(mock.getConfig().num_lanes == 16);
}

FL_TEST_CASE("StressI2S - 16 lanes all 1 LED") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    for (int i = 0; i < 16; i++) {
        driver.enqueue(createTestChannelData(i + 1, 1));
    }
    driver.show();

    FL_REQUIRE(waitReady(driver));
    FL_CHECK(I2sLcdCamPeripheralMock::instance().getConfig().num_lanes == 16);
}

FL_TEST_CASE("StressI2S - 16 lanes all 300 LEDs") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    for (int i = 0; i < 16; i++) {
        driver.enqueue(createTestChannelData(i + 1, 300));
    }
    driver.show();

    FL_REQUIRE(waitReady(driver));
    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 1);
}

//=============================================================================
// Stress: Rapid consecutive show cycles with geometry changes
//=============================================================================

FL_TEST_CASE("StressI2S - 20 cycles changing lane count") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    for (int cycle = 0; cycle < 20; cycle++) {
        int numLanes = (cycle % 8) + 1;
        int numLeds = 10 + cycle * 5;

        for (int lane = 0; lane < numLanes; lane++) {
            driver.enqueue(createTestChannelData(lane + 1, numLeds));
        }
        driver.show();
        FL_REQUIRE(waitReady(driver));
    }

    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 20);
}

FL_TEST_CASE("StressI2S - 20 cycles changing LED counts per lane") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    for (int cycle = 0; cycle < 20; cycle++) {
        driver.enqueue(createTestChannelData(1, 10 + cycle));
        driver.enqueue(createTestChannelData(2, 50 + cycle * 3));
        driver.enqueue(createTestChannelData(3, 5 + cycle * 2));
        driver.enqueue(createTestChannelData(4, 100 - cycle));
        driver.show();
        FL_REQUIRE(waitReady(driver));
    }

    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 20);
}

//=============================================================================
// Stress: Buffer reallocation (lane count + LED count changes)
//=============================================================================

FL_TEST_CASE("StressI2S - realloc small to large to small") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    // Small: 1 lane, 5 LEDs
    driver.enqueue(createTestChannelData(1, 5));
    driver.show();
    FL_REQUIRE(waitReady(driver));

    // Large: 8 lanes, 500 LEDs
    for (int i = 0; i < 8; i++) {
        driver.enqueue(createTestChannelData(i + 1, 500));
    }
    driver.show();
    FL_REQUIRE(waitReady(driver));

    // Small again: 2 lanes, 10 LEDs
    driver.enqueue(createTestChannelData(1, 10));
    driver.enqueue(createTestChannelData(2, 10));
    driver.show();
    FL_REQUIRE(waitReady(driver));

    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 3);
}

FL_TEST_CASE("StressI2S - realloc alternating sizes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    for (int cycle = 0; cycle < 10; cycle++) {
        int numLeds = (cycle % 2 == 0) ? 10 : 500;
        int numLanes = (cycle % 2 == 0) ? 2 : 8;

        for (int lane = 0; lane < numLanes; lane++) {
            driver.enqueue(createTestChannelData(lane + 1, numLeds));
        }
        driver.show();
        FL_REQUIRE(waitReady(driver));
    }

    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 10);
}

//=============================================================================
// Stress: Single lane at large LED counts
//=============================================================================

FL_TEST_CASE("StressI2S - single lane 1000 LEDs") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    driver.enqueue(createTestChannelData(1, 1000));
    driver.show();
    FL_REQUIRE(waitReady(driver));
    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 1);
}

FL_TEST_CASE("StressI2S - single lane 2000 LEDs") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    driver.enqueue(createTestChannelData(1, 2000));
    driver.show();
    FL_REQUIRE(waitReady(driver));
    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 1);
}

//=============================================================================
// Stress: Chipset group splitting (mixed timing configs)
//=============================================================================

FL_TEST_CASE("StressI2S - two chipset timing groups") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    auto ws = getWS2812Timing();
    auto sk = getSK6812Timing();

    // WS2812 timing on lanes 1-2
    driver.enqueue(ChannelData::create(1, ws, []{
        fl::vector_psram<uint8_t> d; d.resize(50 * 3); return d;
    }()));
    driver.enqueue(ChannelData::create(2, ws, []{
        fl::vector_psram<uint8_t> d; d.resize(100 * 3); return d;
    }()));
    // SK6812 timing on lanes 3-4
    driver.enqueue(ChannelData::create(3, sk, []{
        fl::vector_psram<uint8_t> d; d.resize(75 * 3); return d;
    }()));
    driver.enqueue(ChannelData::create(4, sk, []{
        fl::vector_psram<uint8_t> d; d.resize(30 * 3); return d;
    }()));
    driver.show();

    FL_REQUIRE(waitReady(driver));

    auto& mock = I2sLcdCamPeripheralMock::instance();
    // Two groups → at least 2 transmissions (one per timing group)
    FL_CHECK(mock.getTransmitCount() >= 2);
}

FL_TEST_CASE("StressI2S - three chipset timing groups sequential") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    ChipsetTimingConfig t1(350, 800, 450, 50, "T1");
    ChipsetTimingConfig t2(300, 600, 900, 80, "T2");
    ChipsetTimingConfig t3(400, 850, 400, 50, "T3");

    driver.enqueue(ChannelData::create(1, t1, []{
        fl::vector_psram<uint8_t> d; d.resize(100 * 3); return d;
    }()));
    driver.enqueue(ChannelData::create(2, t2, []{
        fl::vector_psram<uint8_t> d; d.resize(200 * 3); return d;
    }()));
    driver.enqueue(ChannelData::create(3, t3, []{
        fl::vector_psram<uint8_t> d; d.resize(150 * 3); return d;
    }()));
    driver.show();

    FL_REQUIRE(waitReady(driver));

    // Three different timings → 3 sequential transmissions
    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 3);
}

//=============================================================================
// Stress: Rapid frames with same geometry (steady-state)
//=============================================================================

FL_TEST_CASE("StressI2S - 50 frames 4 lanes steady geometry") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    for (int frame = 0; frame < 50; frame++) {
        driver.enqueue(createTestChannelData(1, 60));
        driver.enqueue(createTestChannelData(2, 60));
        driver.enqueue(createTestChannelData(3, 60));
        driver.enqueue(createTestChannelData(4, 60));
        driver.show();
        FL_REQUIRE(waitReady(driver));
    }

    FL_CHECK(I2sLcdCamPeripheralMock::instance().getTransmitCount() >= 50);
}

//=============================================================================
// Stress: Zero-length lane mixed with populated lanes
//=============================================================================

FL_TEST_CASE("StressI2S - zero-length lane mixed with populated") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    // 0-LED channel has 0-byte data
    driver.enqueue(createTestChannelData(1, 0));
    driver.enqueue(createTestChannelData(2, 100));
    driver.show();

    // Should not crash; driver may skip or handle gracefully
    waitReady(driver);
}

//=============================================================================
// Stress: Transmit failure recovery with many lanes
//=============================================================================

FL_TEST_CASE("StressI2S - failure recovery with 8 lanes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);

    auto& mock = I2sLcdCamPeripheralMock::instance();
    mock.setTransmitFailure(true);

    for (int i = 0; i < 8; i++) {
        driver.enqueue(createTestChannelData(i + 1, 50 + i * 10));
    }
    driver.show();
    FL_REQUIRE(waitReady(driver));

    // Recover
    mock.setTransmitFailure(false);

    for (int i = 0; i < 4; i++) {
        driver.enqueue(createTestChannelData(i + 1, 30));
    }
    driver.show();
    FL_REQUIRE(waitReady(driver));

    FL_CHECK(mock.getTransmitCount() >= 1);
}

//=============================================================================
// Stress: Buffer size scales with max lane length
//=============================================================================

FL_TEST_CASE("StressI2S - buffer size scales with max lane") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelEngineI2S driver(peripheral);
    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Frame 1: max lane is 50 LEDs
    driver.enqueue(createTestChannelData(1, 10));
    driver.enqueue(createTestChannelData(2, 50));
    driver.show();
    FL_REQUIRE(waitReady(driver));

    const auto& h1 = mock.getTransmitHistory();
    FL_REQUIRE(h1.size() >= 1);
    size_t size1 = h1.back().size_bytes;

    mock.clearTransmitHistory();

    // Frame 2: max lane is 200 LEDs → buffer must grow
    driver.enqueue(createTestChannelData(1, 10));
    driver.enqueue(createTestChannelData(2, 200));
    driver.show();
    FL_REQUIRE(waitReady(driver));

    const auto& h2 = mock.getTransmitHistory();
    FL_REQUIRE(h2.size() >= 1);
    size_t size2 = h2.back().size_bytes;

    FL_CHECK(size2 > size1);
}

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
