// @filter: (platform is esp32)

/// @file    RX.ino
/// @brief   RX device test for ESP32
/// @example RX.ino
///
/// This example demonstrates FastLED's RX device on ESP32 by capturing
/// edge transitions from manual pin toggles. This validates the RX device can
/// accurately capture timing data.
///
/// Hardware Setup:
///   - GPIO 0: TX/RX pin (uses same pin for loopback)
///   - No external wiring required (internal loopback test)
///
/// Test Flow:
///   1. Create and initialize RX device
///   2. Toggle pin in a defined pattern (HIGH/LOW with specific delays)
///   3. Capture edge timing data using RX device
///   4. Analyze and display raw edge timings
///
/// Platform Support:
///   - ESP32 (all variants: classic, S3, C3, C6, etc.)
///
/// run with (in fastled repo)
//    bash debug RX --expect "TX Pin: GPIO 0" --expect "RX Pin: GPIO 1" --expect "RX Device: RMT" --fail-on ERROR

#include <FastLED.h>
#include "fl/rx_device.h"
#include "test.h"
#include "SketchHalt.h"

// ============================================================================
// Configuration
// ============================================================================

// ⚠️  DO NOT CHANGE THESE PIN ASSIGNMENTS - NO EXCEPTIONS! ⚠️
// These specific pins are required for hardware testing infrastructure.
// Changing PIN_TX or PIN_RX will break automated testing equipment.
// PHYSICALLY CONNECT GPIO 0 TO GPIO 19 WITH A WIRE FOR THIS TEST.
#define EDGE_BUFFER_SIZE 100
#define WAIT_TIMEOUT_MS 100

// Pin and RX type configuration (extern for test.cpp access)
const int PIN_TX = 0;   // DO NOT CHANGE - REQUIRED FOR TEST INFRASTRUCTURE
const int PIN_RX = 1;   // DO NOT CHANGE - REQUIRED FOR TEST INFRASTRUCTURE
constexpr fl::RxDeviceType RX_TYPE = fl::RxDeviceType::RMT;

// ============================================================================
// Pin Toggle Pattern
// ============================================================================

// Pattern: HIGH 1ms, LOW 1ms, HIGH 2ms, LOW 2ms, HIGH 3ms, LOW 100us
const fl::array<PinToggle, 6> TEST_PATTERN = {{
    {true, 1000},   // HIGH for 1ms
    {false, 1000},  // LOW for 1ms
    {true, 2000},   // HIGH for 2ms
    {false, 2000},  // LOW for 2ms
    {true, 3000},   // HIGH for 3ms
    {false, 100}    // LOW for 100us (end)
}};

// ============================================================================
// Global State
// ============================================================================

fl::shared_ptr<fl::RxDevice> g_rx_device;

// Sketch halt controller - handles safe halting without watchdog timer resets
SketchHalt halt;

// ============================================================================
// Arduino Setup & Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    const char* loop_back_mode = PIN_TX == PIN_RX ? "INTERNAL" : "JUMPER WIRE";

    FL_WARN("\n=== FastLED RX Device Test ===");
    FL_WARN("Platform: ESP32");
    FL_WARN("TX Pin: GPIO " << PIN_TX);
    FL_WARN("RX Pin: GPIO " << PIN_RX);
    FL_WARN("RX Device: " << (RX_TYPE == fl::RxDeviceType::RMT ? "RMT" : "ISR"));
    FL_WARN("LOOP BACK MODE: " << loop_back_mode);

    // Sanity check: Verify jumper wire connection when TX and RX are different pins
    if (PIN_TX != PIN_RX) {
        if (!verifyJumperWire(PIN_TX, PIN_RX)) {
            halt.error("Missing jumper wire between TX and RX pins");
            return;
        }
    } else {
        FL_WARN("TX and RX use same pin (" << PIN_TX << ") - no jumper wire needed");
    }

    FL_WARN("");

    // Create RX device for testing
    // Use 1MHz resolution for better timing accuracy (1us per tick)
    FL_WARN("Creating RX device for testing...");
    auto rx_test = fl::RxDevice::create<RX_TYPE>(PIN_RX);
    if (!rx_test) {
        halt.error("Failed to create RX device for testing");
        return;
    }

    // Initialize test RX device with config
    fl::RxConfig test_config;
    test_config.buffer_size = 10;
    test_config.hz = 1000000;  // 1MHz resolution for better timing accuracy
    test_config.signal_range_min_ns = 100;
    test_config.signal_range_max_ns = 10000000;
    test_config.start_low = true;

    if (!rx_test->begin(test_config)) {
        halt.error("Failed to initialize test RX device");
        return;
    }

    // Test RX device functionality
    if (!testRxDevice(rx_test, PIN_TX)) {
        halt.error("RX device sanity check failed - RX not working");
        return;
    }
    FL_WARN("");

    // Create main RX device for the loop
    // Use 1MHz resolution to allow longer timeouts (40MHz = ~819us max, 1MHz = ~32ms max)
    FL_WARN("Creating main RX device...");
    g_rx_device = fl::RxDevice::create<RX_TYPE>(PIN_RX);
    if (!g_rx_device) {
        halt.error("Failed to create main RX device");
        return;
    }
    FL_WARN("✓ Main RX device created\n");

    delay(1000);
}

void loop() {
    // IMPORTANT: Must be first line - handles halt state and prevents watchdog resets
    if (halt.check()) return;

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║  RX DEVICE TEST");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝\n");

    // Configure RX device
    fl::RxConfig config;
    config.buffer_size = EDGE_BUFFER_SIZE;
    config.hz = 1000000;                     // 1MHz resolution (allows up to ~32ms timeout)
    config.signal_range_min_ns = 100;       // 100ns glitch filter
    config.signal_range_max_ns = 10000000;  // 10ms idle timeout (1MHz RMT allows up to ~32ms)
    config.start_low = true;                 // Pin starts LOW

    // Execute toggles and capture data
    FL_WARN("[TEST] Initializing RX device and executing toggles...");
    executeToggles(*g_rx_device, config, TEST_PATTERN, PIN_TX, WAIT_TIMEOUT_MS);

    // Wait for capture completion
    FL_WARN("[TEST] Waiting for capture (timeout: " << WAIT_TIMEOUT_MS << "ms)...");
    auto wait_result = g_rx_device->wait(WAIT_TIMEOUT_MS);

    if (wait_result == fl::RxWaitResult::TIMEOUT) {
        FL_ERROR("Timeout waiting for data");
    } else if (wait_result == fl::RxWaitResult::BUFFER_OVERFLOW) {
        FL_ERROR("Buffer overflow during capture");
    } else {
        FL_WARN("[TEST] ✓ Data captured successfully");

        // Get edge timings
        fl::array<fl::EdgeTime, EDGE_BUFFER_SIZE> edge_buffer;
        size_t edge_count = g_rx_device->getRawEdgeTimes(edge_buffer);

        // Validate edge timing against expected pattern
        const uint32_t TOLERANCE_PERCENT = 15;  // ±15% tolerance for timing jitter
        validateEdgeTiming(edge_buffer, edge_count, TEST_PATTERN, TOLERANCE_PERCENT);
    }

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ TEST COMPLETE - Waiting 5 seconds...");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝\n");

    delay(5000);
}
