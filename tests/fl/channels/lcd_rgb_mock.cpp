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

#include "test.h"
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

FL_TEST_CASE("LcdRgbPeripheralMock - basic initialization") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Before initialization
    FL_CHECK_FALSE(mock.isInitialized());

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
    FL_CHECK(success);
    FL_CHECK(mock.isInitialized());
    FL_CHECK(mock.isEnabled());

    // Verify config stored correctly
    const auto& stored = mock.getConfig();
    FL_CHECK(stored.pclk_gpio == 10);
    FL_CHECK(stored.pclk_hz == 3200000);
    FL_CHECK(stored.num_lanes == 4);
    FL_CHECK(stored.h_res == 1920);
}

FL_TEST_CASE("LcdRgbPeripheralMock - invalid configuration") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Zero lanes should fail
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 0;  // Invalid
    config.h_res = 1920;

    bool success = mock.initialize(config);
    FL_CHECK_FALSE(success);
    FL_CHECK_FALSE(mock.isInitialized());

    // More than 16 lanes should fail
    config.num_lanes = 17;
    success = mock.initialize(config);
    FL_CHECK_FALSE(success);
}

//=============================================================================
// Test Suite: Buffer Management
//=============================================================================

FL_TEST_CASE("LcdRgbPeripheralMock - buffer allocation") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 960;
    FL_REQUIRE(mock.initialize(config));

    // Allocate buffer
    size_t size = 1024;
    uint16_t* buffer = mock.allocateFrameBuffer(size);
    FL_REQUIRE(buffer != nullptr);

    // Write some data
    for (size_t i = 0; i < size / 2; i++) {
        buffer[i] = static_cast<uint16_t>(i);
    }

    // Read back
    for (size_t i = 0; i < size / 2; i++) {
        FL_CHECK(buffer[i] == static_cast<uint16_t>(i));
    }

    // Free buffer
    mock.freeFrameBuffer(buffer);
}

FL_TEST_CASE("LcdRgbPeripheralMock - free null buffer is safe") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();
    mock.freeFrameBuffer(nullptr);  // Should not crash
}

//=============================================================================
// Test Suite: Frame Transmission
//=============================================================================

FL_TEST_CASE("LcdRgbPeripheralMock - basic frame draw") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 4;
    config.h_res = 1920;
    FL_REQUIRE(mock.initialize(config));

    // Allocate and fill buffer
    size_t size_bytes = 1024;
    uint16_t* buffer = mock.allocateFrameBuffer(size_bytes);
    FL_REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < size_bytes / 2; i++) {
        buffer[i] = 0xAAAA;
    }

    // Draw frame
    bool success = mock.drawFrame(buffer, size_bytes);
    FL_CHECK(success);

    // Wait for completion
    bool complete = mock.waitFrameDone(100);
    FL_CHECK(complete);

    // Check history
    const auto& history = mock.getFrameHistory();
    FL_CHECK(history.size() == 1);
    FL_CHECK(history[0].size_bytes == size_bytes);

    // Verify draw count
    FL_CHECK(mock.getDrawCount() == 1);

    mock.freeFrameBuffer(buffer);
}

FL_TEST_CASE("LcdRgbPeripheralMock - multiple draws") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    FL_REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateFrameBuffer(512);
    FL_REQUIRE(buffer != nullptr);

    // Draw 3 frames
    for (int i = 0; i < 3; i++) {
        for (size_t j = 0; j < 256; j++) {
            buffer[j] = static_cast<uint16_t>(i * 256 + j);
        }
        FL_REQUIRE(mock.drawFrame(buffer, 512));
        FL_REQUIRE(mock.waitFrameDone(100));
    }

    // Check history
    const auto& history = mock.getFrameHistory();
    FL_CHECK(history.size() == 3);
    FL_CHECK(mock.getDrawCount() == 3);

    mock.freeFrameBuffer(buffer);
}

FL_TEST_CASE("LcdRgbPeripheralMock - frame data capture") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 2;
    config.h_res = 960;
    FL_REQUIRE(mock.initialize(config));

    // Create buffer with known pattern
    size_t size_bytes = 64;
    uint16_t* buffer = mock.allocateFrameBuffer(size_bytes);
    FL_REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < size_bytes / 2; i++) {
        buffer[i] = static_cast<uint16_t>(0x1234 + i);
    }

    // Draw
    FL_REQUIRE(mock.drawFrame(buffer, size_bytes));
    FL_REQUIRE(mock.waitFrameDone(100));

    // Get last frame data
    fl::span<const uint16_t> last_frame = mock.getLastFrameData();
    FL_REQUIRE(last_frame.size() == size_bytes / 2);

    // Verify captured data matches
    for (size_t i = 0; i < last_frame.size(); i++) {
        FL_CHECK(last_frame[i] == static_cast<uint16_t>(0x1234 + i));
    }

    mock.freeFrameBuffer(buffer);
}

//=============================================================================
// Test Suite: Error Injection
//=============================================================================

FL_TEST_CASE("LcdRgbPeripheralMock - draw failure injection") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    FL_REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateFrameBuffer(256);
    FL_REQUIRE(buffer != nullptr);

    // Inject failure
    mock.setDrawFailure(true);

    // Draw should fail
    bool success = mock.drawFrame(buffer, 256);
    FL_CHECK_FALSE(success);

    // Clear failure
    mock.setDrawFailure(false);

    // Draw should succeed now
    success = mock.drawFrame(buffer, 256);
    FL_CHECK(success);

    mock.freeFrameBuffer(buffer);
}

FL_TEST_CASE("LcdRgbPeripheralMock - draw without initialization") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Don't initialize - should fail
    FL_CHECK_FALSE(mock.isInitialized());

    uint16_t dummy[16] = {0};
    bool success = mock.drawFrame(dummy, sizeof(dummy));
    FL_CHECK_FALSE(success);
}

//=============================================================================
// Test Suite: Callback Simulation
//=============================================================================

FL_TEST_CASE("LcdRgbPeripheralMock - callback registration and simulation") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    FL_REQUIRE(mock.initialize(config));

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
    FL_CHECK(reg_success);

    // Draw frame (callback will fire automatically)
    uint16_t* buffer = mock.allocateFrameBuffer(256);
    FL_REQUIRE(buffer != nullptr);
    FL_REQUIRE(mock.drawFrame(buffer, 256));

    // Wait for completion (callback should fire)
    FL_REQUIRE(mock.waitFrameDone(100));

    // Verify callback was called
    FL_CHECK(callback_count == 1);
    FL_CHECK(callback_ctx == user_ctx);

    mock.freeFrameBuffer(buffer);
}

FL_TEST_CASE("LcdRgbPeripheralMock - manual simulate draw complete") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    FL_REQUIRE(mock.initialize(config));

    // Simulate completion without actual draw
    mock.simulateDrawComplete();  // Should not crash (no pending draws)
}

//=============================================================================
// Test Suite: State Inspection
//=============================================================================

FL_TEST_CASE("LcdRgbPeripheralMock - state inspection") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initial state
    FL_CHECK_FALSE(mock.isInitialized());
    FL_CHECK_FALSE(mock.isEnabled());
    FL_CHECK_FALSE(mock.isBusy());
    FL_CHECK(mock.getDrawCount() == 0);

    // After initialization
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 2;
    config.h_res = 960;
    FL_REQUIRE(mock.initialize(config));

    FL_CHECK(mock.isInitialized());
    FL_CHECK(mock.isEnabled());
    FL_CHECK_FALSE(mock.isBusy());
}

FL_TEST_CASE("LcdRgbPeripheralMock - history clearing") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    FL_REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateFrameBuffer(256);
    FL_REQUIRE(buffer != nullptr);

    // Draw some frames
    FL_REQUIRE(mock.drawFrame(buffer, 256));
    FL_REQUIRE(mock.waitFrameDone(100));
    FL_REQUIRE(mock.drawFrame(buffer, 256));
    FL_REQUIRE(mock.waitFrameDone(100));

    FL_CHECK(mock.getFrameHistory().size() == 2);
    size_t draw_count = mock.getDrawCount();
    FL_CHECK(draw_count == 2);

    // Clear history
    mock.clearFrameHistory();

    FL_CHECK(mock.getFrameHistory().size() == 0);
    // Draw count is NOT reset by clearFrameHistory
    FL_CHECK(mock.getDrawCount() == draw_count);

    mock.freeFrameBuffer(buffer);
}

FL_TEST_CASE("LcdRgbPeripheralMock - reset clears all state") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize and draw
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    FL_REQUIRE(mock.initialize(config));

    uint16_t* buffer = mock.allocateFrameBuffer(256);
    FL_REQUIRE(buffer != nullptr);
    FL_REQUIRE(mock.drawFrame(buffer, 256));
    FL_REQUIRE(mock.waitFrameDone(100));
    mock.freeFrameBuffer(buffer);

    // Reset
    mock.reset();

    // All state should be cleared
    FL_CHECK_FALSE(mock.isInitialized());
    FL_CHECK_FALSE(mock.isEnabled());
    FL_CHECK_FALSE(mock.isBusy());
    FL_CHECK(mock.getDrawCount() == 0);
    FL_CHECK(mock.getFrameHistory().size() == 0);
}

//=============================================================================
// Test Suite: Timing Utilities
//=============================================================================

FL_TEST_CASE("LcdRgbPeripheralMock - getMicroseconds") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    uint64_t t1 = mock.getMicroseconds();
    delay(1);  // Small delay
    uint64_t t2 = mock.getMicroseconds();

    // Time should advance
    FL_CHECK(t2 >= t1);
}

FL_TEST_CASE("LcdRgbPeripheralMock - delay") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    uint64_t start = mock.getMicroseconds();
    mock.delay(5);  // 5ms delay
    uint64_t end = mock.getMicroseconds();

    // Should have delayed at least 4ms (allow for timing variance)
    uint64_t elapsed_ms = (end - start) / 1000;
    FL_CHECK(elapsed_ms >= 4);
}

//=============================================================================
// Test Suite: Deinitialize
//=============================================================================

FL_TEST_CASE("LcdRgbPeripheralMock - deinitialize") {
    resetLcdRgbMockState();

    auto& mock = LcdRgbPeripheralMock::instance();

    // Initialize
    LcdRgbPeripheralConfig config;
    config.pclk_gpio = 10;
    config.pclk_hz = 3200000;
    config.num_lanes = 1;
    config.h_res = 480;
    FL_REQUIRE(mock.initialize(config));
    FL_CHECK(mock.isInitialized());

    // Deinitialize
    mock.deinitialize();
    FL_CHECK_FALSE(mock.isInitialized());
}

#endif // FASTLED_STUB_IMPL
