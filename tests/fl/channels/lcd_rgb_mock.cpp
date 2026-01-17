/// @file lcd_rgb_mock.cpp
/// @brief Unit tests for LCD RGB mock peripheral
///
/// Tests the mock LCD RGB peripheral implementation for:
/// - Basic initialization and configuration
/// - Frame buffer management
/// - Frame transmission and callback simulation
/// - Error injection and state inspection
///
/// These tests run ONLY on stub platforms (host-based testing).

#ifdef FASTLED_STUB_IMPL  // Mock tests only run on stub platform

#include "doctest.h"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_mock.h"
#include "fl/stl/vector.h"

using namespace fl;
using namespace fl::detail;

namespace {

/// @brief Reset mock state between tests
void resetLcdRgbMockState() {
    auto& mock = LcdRgbPeripheralMock::instance();
    mock.reset();
}

} // anonymous namespace

//=============================================================================
// Test Suite: Basic Initialization
//=============================================================================

TEST_CASE("LcdRgbPeripheralMock - basic initialization") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Before initialization
    CHECK_FALSE(mock.isInitialized());

    // Configure
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;  // 3.2 MHz
    config.num_lanes = 4;
    config.h_res = 1920;  // 80 LEDs * 24 bits
    config.v_res = 1;
    config.use_psram = false;

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
    CHECK(stored.pclk_gpio == 10);
    CHECK(stored.pclk_hz == 3200000);
    CHECK(stored.num_lanes == 4);
    CHECK(stored.h_res == 1920);
}

TEST_CASE("LcdRgbPeripheralMock - invalid configuration") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Zero lanes should fail
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 0;  // Invalid
    config.h_res = 1920;

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

TEST_CASE("LcdRgbPeripheralMock - buffer allocation") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 960;
    REQUIRE(mock.initialize(config));

    // Allocate buffer
    size_t size = 1024;
    uint16_t* buffer = mock.allocateFrameBuffer(size);
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
    mock.freeFrameBuffer(buffer);
}

TEST_CASE("LcdRgbPeripheralMock - free null buffer is safe") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();
    mock.freeFrameBuffer(nullptr);  // Should not crash
}

//=============================================================================
// Test Suite: Frame Transmission
//=============================================================================

TEST_CASE("LcdRgbPeripheralMock - basic frame draw") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 4;
    config.h_res = 1920;
    REQUIRE(mock.initialize(config));

    // Allocate and fill buffer
    size_t size_bytes = 1024;
    uint16_t* buffer = mock.allocateFrameBuffer(size_bytes);
    REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < size_bytes / 2; i++) {
        buffer[i] = 0xAAAA;
    }

    // Draw frame
    bool success = mock.drawFrame(buffer, size_bytes);
    CHECK(success);

    // Wait for completion
    bool complete = mock.waitFrameDone(100);
    CHECK(complete);

    // Check history
    const auto& history = mock.getFrameHistory();
    CHECK(history.size() == 1);
    CHECK(history[0].size_bytes == size_bytes);

    // Verify draw count
    CHECK(mock.getDrawCount() == 1);

    mock.freeFrameBuffer(buffer);
}

TEST_CASE("LcdRgbPeripheralMock - multiple draws") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateFrameBuffer(512);
    REQUIRE(buffer != nullptr);

    // Draw 3 frames
    for (int i = 0; i < 3; i++) {
        for (size_t j = 0; j < 256; j++) {
            buffer[j] = static_cast<uint16_t>(i * 256 + j);
        }
        REQUIRE(mock.drawFrame(buffer, 512));
        REQUIRE(mock.waitFrameDone(100));
    }

    // Check history
    const auto& history = mock.getFrameHistory();
    CHECK(history.size() == 3);
    CHECK(mock.getDrawCount() == 3);

    mock.freeFrameBuffer(buffer);
}

TEST_CASE("LcdRgbPeripheralMock - frame data capture") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 2;
    config.h_res = 960;
    REQUIRE(mock.initialize(config));

    // Create buffer with known pattern
    size_t size_bytes = 64;
    uint16_t* buffer = mock.allocateFrameBuffer(size_bytes);
    REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < size_bytes / 2; i++) {
        buffer[i] = static_cast<uint16_t>(0x1234 + i);
    }

    // Draw
    REQUIRE(mock.drawFrame(buffer, size_bytes));
    REQUIRE(mock.waitFrameDone(100));

    // Get last frame data
    fl::span<const uint16_t> last_frame = mock.getLastFrameData();
    REQUIRE(last_frame.size() == size_bytes / 2);

    // Verify captured data matches
    for (size_t i = 0; i < last_frame.size(); i++) {
        CHECK(last_frame[i] == static_cast<uint16_t>(0x1234 + i));
    }

    mock.freeFrameBuffer(buffer);
}

//=============================================================================
// Test Suite: Error Injection
//=============================================================================

TEST_CASE("LcdRgbPeripheralMock - draw failure injection") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateFrameBuffer(256);
    REQUIRE(buffer != nullptr);

    // Inject failure
    mock.setDrawFailure(true);

    // Draw should fail
    bool success = mock.drawFrame(buffer, 256);
    CHECK_FALSE(success);

    // Clear failure
    mock.setDrawFailure(false);

    // Draw should succeed now
    success = mock.drawFrame(buffer, 256);
    CHECK(success);

    mock.freeFrameBuffer(buffer);
}

TEST_CASE("LcdRgbPeripheralMock - draw without initialization") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Don't initialize - should fail
    CHECK_FALSE(mock.isInitialized());

    uint16_t dummy[16] = {0};
    bool success = mock.drawFrame(dummy, sizeof(dummy));
    CHECK_FALSE(success);
}

//=============================================================================
// Test Suite: Callback Simulation
//=============================================================================

TEST_CASE("LcdRgbPeripheralMock - callback registration and simulation") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
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
    bool reg_success = mock.registerDrawCallback(reinterpret_cast<void*>(+callback), user_ctx);
    CHECK(reg_success);

    // Draw frame (callback will fire automatically)
    uint16_t* buffer = mock.allocateFrameBuffer(256);
    REQUIRE(buffer != nullptr);
    REQUIRE(mock.drawFrame(buffer, 256));

    // Wait for completion (callback should fire)
    REQUIRE(mock.waitFrameDone(100));

    // Verify callback was called
    CHECK(callback_count == 1);
    CHECK(callback_ctx == user_ctx);

    mock.freeFrameBuffer(buffer);
}

TEST_CASE("LcdRgbPeripheralMock - manual simulate draw complete") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    REQUIRE(mock.initialize(config));

    // Simulate completion without actual draw
    mock.simulateDrawComplete();  // Should not crash (no pending draws)
}

//=============================================================================
// Test Suite: State Inspection
//=============================================================================

TEST_CASE("LcdRgbPeripheralMock - state inspection") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initial state
    CHECK_FALSE(mock.isInitialized());
    CHECK_FALSE(mock.isEnabled());
    CHECK_FALSE(mock.isBusy());
    CHECK(mock.getDrawCount() == 0);

    // After initialization
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 2;
    config.h_res = 960;
    REQUIRE(mock.initialize(config));

    CHECK(mock.isInitialized());
    CHECK(mock.isEnabled());
    CHECK_FALSE(mock.isBusy());
}

TEST_CASE("LcdRgbPeripheralMock - history clearing") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateFrameBuffer(256);
    REQUIRE(buffer != nullptr);

    // Draw some frames
    REQUIRE(mock.drawFrame(buffer, 256));
    REQUIRE(mock.waitFrameDone(100));
    REQUIRE(mock.drawFrame(buffer, 256));
    REQUIRE(mock.waitFrameDone(100));

    CHECK(mock.getFrameHistory().size() == 2);
    size_t draw_count = mock.getDrawCount();
    CHECK(draw_count == 2);

    // Clear history
    mock.clearFrameHistory();

    CHECK(mock.getFrameHistory().size() == 0);
    // Draw count is NOT reset by clearFrameHistory
    CHECK(mock.getDrawCount() == draw_count);

    mock.freeFrameBuffer(buffer);
}

TEST_CASE("LcdRgbPeripheralMock - reset clears all state") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize and draw
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateFrameBuffer(256);
    REQUIRE(buffer != nullptr);
    REQUIRE(mock.drawFrame(buffer, 256));
    REQUIRE(mock.waitFrameDone(100));
    mock.freeFrameBuffer(buffer);

    // Reset
    mock.reset();

    // All state should be cleared
    CHECK_FALSE(mock.isInitialized());
    CHECK_FALSE(mock.isEnabled());
    CHECK_FALSE(mock.isBusy());
    CHECK(mock.getDrawCount() == 0);
    CHECK(mock.getFrameHistory().size() == 0);
}

//=============================================================================
// Test Suite: Timing Utilities
//=============================================================================

TEST_CASE("LcdRgbPeripheralMock - getMicroseconds") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    uint64_t t1 = mock.getMicroseconds();
    delay(1);  // Small delay
    uint64_t t2 = mock.getMicroseconds();

    // Time should advance
    CHECK(t2 >= t1);
}

TEST_CASE("LcdRgbPeripheralMock - delay") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

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

TEST_CASE("LcdRgbPeripheralMock - deinitialize") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    REQUIRE(mock.initialize(config));
    CHECK(mock.isInitialized());

    // Deinitialize
    mock.deinitialize();
    CHECK_FALSE(mock.isInitialized());
}

#endif // FASTLED_STUB_IMPL
