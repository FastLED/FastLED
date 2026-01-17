/// @file i2s_lcd_cam_mock.cpp
/// @brief Unit tests for I2S LCD_CAM mock peripheral
///
/// Tests the mock I2S LCD_CAM peripheral implementation for:
/// - Basic initialization and configuration
/// - Buffer management
/// - Transmission and callback simulation
/// - Error injection and state inspection
///
/// These tests run ONLY on stub platforms (host-based testing).

#ifdef FASTLED_STUB_IMPL  // Mock tests only run on stub platform

#include "doctest.h"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.h"
#include "fl/stl/vector.h"

using namespace fl;
using namespace fl::detail;

namespace {

/// @brief Reset mock state between tests
void resetI2sLcdCamMockState() {
    auto& mock = I2sLcdCamPeripheralMock::instance();
    mock.reset();
}

} // anonymous namespace

//=============================================================================
// Test Suite: Basic Initialization
//=============================================================================

TEST_CASE("I2sLcdCamPeripheralMock - basic initialization") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Before initialization
    CHECK_FALSE(mock.isInitialized());

    // Configure
    I2sLcdCamConfig config;
    config.num_lanes = 4;
    config.pclk_hz = 2400000;  // 2.4 MHz
    config.max_transfer_bytes = 4096;
    config.use_psram = true;

    config.data_gpios.resize(16);
    config.data_gpios[0] = 1;
    config.data_gpios[1] = 2;
    config.data_gpios[2] = 3;
    config.data_gpios[3] = 4;
    for (int i = 4; i < 16; i++) {
        config.data_gpios[i] = -1;
    }

    bool success = mock.initialize(config);
    CHECK(success);
    CHECK(mock.isInitialized());
    CHECK(mock.isEnabled());

    // Verify config stored correctly
    const auto& stored = mock.getConfig();
    CHECK(stored.pclk_hz == 2400000);
    CHECK(stored.num_lanes == 4);
    CHECK(stored.max_transfer_bytes == 4096);
}

TEST_CASE("I2sLcdCamPeripheralMock - invalid configuration") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Zero lanes should fail
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 0;  // Invalid
    config.max_transfer_bytes = 4096;

    bool success = mock.initialize(config);
    CHECK_FALSE(success);
    CHECK_FALSE(mock.isInitialized());

    // More than 16 lanes should fail
    config.num_lanes = 17;
    success = mock.initialize(config);
    CHECK_FALSE(success);
}

//=============================================================================
// Test Suite: Buffer Management
//=============================================================================

TEST_CASE("I2sLcdCamPeripheralMock - buffer allocation") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 1;
    config.max_transfer_bytes = 4096;
    REQUIRE(mock.initialize(config));

    // Allocate buffer
    size_t size = 1024;
    uint16_t* buffer = mock.allocateBuffer(size);
    REQUIRE(buffer != nullptr);

    // Write some data
    for (size_t i = 0; i < size / 2; i++) {
        buffer[i] = static_cast<uint16_t>(i);
    }

    // Read back
    for (size_t i = 0; i < size / 2; i++) {
        CHECK(buffer[i] == static_cast<uint16_t>(i));
    }

    // Free buffer
    mock.freeBuffer(buffer);
}

TEST_CASE("I2sLcdCamPeripheralMock - free null buffer is safe") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();
    mock.freeBuffer(nullptr);  // Should not crash
}

//=============================================================================
// Test Suite: Transmission
//=============================================================================

TEST_CASE("I2sLcdCamPeripheralMock - basic transmit") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 4;
    config.max_transfer_bytes = 4096;
    REQUIRE(mock.initialize(config));

    // Allocate and fill buffer
    size_t size_bytes = 1024;
    uint16_t* buffer = mock.allocateBuffer(size_bytes);
    REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < size_bytes / 2; i++) {
        buffer[i] = 0xAAAA;
    }

    // Transmit
    bool success = mock.transmit(buffer, size_bytes);
    CHECK(success);

    // Wait for completion
    bool complete = mock.waitTransmitDone(100);
    CHECK(complete);

    // Check history
    const auto& history = mock.getTransmitHistory();
    CHECK(history.size() == 1);
    CHECK(history[0].size_bytes == size_bytes);

    // Verify transmit count
    CHECK(mock.getTransmitCount() == 1);

    mock.freeBuffer(buffer);
}

TEST_CASE("I2sLcdCamPeripheralMock - multiple transmits") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 1;
    config.max_transfer_bytes = 2048;
    REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateBuffer(512);
    REQUIRE(buffer != nullptr);

    // Transmit 3 frames
    for (int i = 0; i < 3; i++) {
        for (size_t j = 0; j < 256; j++) {
            buffer[j] = static_cast<uint16_t>(i * 256 + j);
        }
        REQUIRE(mock.transmit(buffer, 512));
        REQUIRE(mock.waitTransmitDone(100));
    }

    // Check history
    const auto& history = mock.getTransmitHistory();
    CHECK(history.size() == 3);
    CHECK(mock.getTransmitCount() == 3);

    mock.freeBuffer(buffer);
}

TEST_CASE("I2sLcdCamPeripheralMock - transmit data capture") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 2;
    config.max_transfer_bytes = 1024;
    REQUIRE(mock.initialize(config));

    // Create buffer with known pattern
    size_t size_bytes = 64;
    uint16_t* buffer = mock.allocateBuffer(size_bytes);
    REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < size_bytes / 2; i++) {
        buffer[i] = static_cast<uint16_t>(0x1234 + i);
    }

    // Transmit
    REQUIRE(mock.transmit(buffer, size_bytes));
    REQUIRE(mock.waitTransmitDone(100));

    // Get last transmit data
    fl::span<const uint16_t> last_data = mock.getLastTransmitData();
    REQUIRE(last_data.size() == size_bytes / 2);

    // Verify captured data matches
    for (size_t i = 0; i < last_data.size(); i++) {
        CHECK(last_data[i] == static_cast<uint16_t>(0x1234 + i));
    }

    mock.freeBuffer(buffer);
}

//=============================================================================
// Test Suite: Error Injection
//=============================================================================

TEST_CASE("I2sLcdCamPeripheralMock - transmit failure injection") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 1;
    config.max_transfer_bytes = 1024;
    REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateBuffer(256);
    REQUIRE(buffer != nullptr);

    // Inject failure
    mock.setTransmitFailure(true);

    // Transmit should fail
    bool success = mock.transmit(buffer, 256);
    CHECK_FALSE(success);

    // Clear failure
    mock.setTransmitFailure(false);

    // Transmit should succeed now
    success = mock.transmit(buffer, 256);
    CHECK(success);

    mock.freeBuffer(buffer);
}

TEST_CASE("I2sLcdCamPeripheralMock - transmit without initialization") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Don't initialize - should fail
    CHECK_FALSE(mock.isInitialized());

    uint16_t dummy[16] = {0};
    bool success = mock.transmit(dummy, sizeof(dummy));
    CHECK_FALSE(success);
}

//=============================================================================
// Test Suite: Callback Simulation
//=============================================================================

TEST_CASE("I2sLcdCamPeripheralMock - callback registration and simulation") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 1;
    config.max_transfer_bytes = 1024;
    REQUIRE(mock.initialize(config));

    // Callback tracking
    static int callback_count = 0;
    static void* callback_ctx = nullptr;
    callback_count = 0;
    callback_ctx = nullptr;

    // Callback function
    auto callback = [](void* panel, const void* edata, void* ctx) -> bool {
        callback_count++;
        callback_ctx = ctx;
        return false;
    };

    void* user_ctx = reinterpret_cast<void*>(0x12345678);
    bool reg_success = mock.registerTransmitCallback(reinterpret_cast<void*>(+callback), user_ctx);
    CHECK(reg_success);

    // Transmit (callback will fire automatically)
    uint16_t* buffer = mock.allocateBuffer(256);
    REQUIRE(buffer != nullptr);
    REQUIRE(mock.transmit(buffer, 256));

    // Wait for completion (callback should fire)
    REQUIRE(mock.waitTransmitDone(100));

    // Verify callback was called
    CHECK(callback_count == 1);
    CHECK(callback_ctx == user_ctx);

    mock.freeBuffer(buffer);
}

TEST_CASE("I2sLcdCamPeripheralMock - manual simulate transmit complete") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 1;
    config.max_transfer_bytes = 1024;
    REQUIRE(mock.initialize(config));

    // Simulate completion without actual transmit
    mock.simulateTransmitComplete();  // Should not crash (no pending transmits)
}

//=============================================================================
// Test Suite: State Inspection
//=============================================================================

TEST_CASE("I2sLcdCamPeripheralMock - state inspection") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initial state
    CHECK_FALSE(mock.isInitialized());
    CHECK_FALSE(mock.isEnabled());
    CHECK_FALSE(mock.isBusy());
    CHECK(mock.getTransmitCount() == 0);

    // After initialization
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 2;
    config.max_transfer_bytes = 1024;
    REQUIRE(mock.initialize(config));

    CHECK(mock.isInitialized());
    CHECK(mock.isEnabled());
    CHECK_FALSE(mock.isBusy());
}

TEST_CASE("I2sLcdCamPeripheralMock - history clearing") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 1;
    config.max_transfer_bytes = 1024;
    REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateBuffer(256);
    REQUIRE(buffer != nullptr);

    // Transmit some frames
    REQUIRE(mock.transmit(buffer, 256));
    REQUIRE(mock.waitTransmitDone(100));
    REQUIRE(mock.transmit(buffer, 256));
    REQUIRE(mock.waitTransmitDone(100));

    CHECK(mock.getTransmitHistory().size() == 2);
    size_t transmit_count = mock.getTransmitCount();
    CHECK(transmit_count == 2);

    // Clear history
    mock.clearTransmitHistory();

    CHECK(mock.getTransmitHistory().size() == 0);
    // Transmit count is NOT reset by clearTransmitHistory
    CHECK(mock.getTransmitCount() == transmit_count);

    mock.freeBuffer(buffer);
}

TEST_CASE("I2sLcdCamPeripheralMock - reset clears all state") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize and transmit
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 1;
    config.max_transfer_bytes = 1024;
    REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateBuffer(256);
    REQUIRE(buffer != nullptr);
    REQUIRE(mock.transmit(buffer, 256));
    REQUIRE(mock.waitTransmitDone(100));
    mock.freeBuffer(buffer);

    // Reset
    mock.reset();

    // All state should be cleared
    CHECK_FALSE(mock.isInitialized());
    CHECK_FALSE(mock.isEnabled());
    CHECK_FALSE(mock.isBusy());
    CHECK(mock.getTransmitCount() == 0);
    CHECK(mock.getTransmitHistory().size() == 0);
}

//=============================================================================
// Test Suite: Timing Utilities
//=============================================================================

TEST_CASE("I2sLcdCamPeripheralMock - getMicroseconds") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    uint64_t t1 = mock.getMicroseconds();
    delay(1);  // Small delay
    uint64_t t2 = mock.getMicroseconds();

    // Time should advance
    CHECK(t2 >= t1);
}

TEST_CASE("I2sLcdCamPeripheralMock - delay") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    uint64_t start = mock.getMicroseconds();
    mock.delay(5);  // 5ms delay
    uint64_t end = mock.getMicroseconds();

    // Should have delayed at least 4ms (allow for timing variance)
    uint64_t elapsed_ms = (end - start) / 1000;
    CHECK(elapsed_ms >= 4);
}

//=============================================================================
// Test Suite: Deinitialize
//=============================================================================

TEST_CASE("I2sLcdCamPeripheralMock - deinitialize") {
    resetI2sLcdCamMockState();

    auto& mock = I2sLcdCamPeripheralMock::instance();

    // Initialize
    I2sLcdCamConfig config;
    config.pclk_hz = 2400000;
    config.num_lanes = 1;
    config.max_transfer_bytes = 1024;
    REQUIRE(mock.initialize(config));
    CHECK(mock.isInitialized());

    // Deinitialize
    mock.deinitialize();
    CHECK_FALSE(mock.isInitialized());
}

#endif // FASTLED_STUB_IMPL
