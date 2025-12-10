// @filter: (platform is esp32)

/// @file    RX.ino
/// @brief   GPIO ISR RX device test for ESP32
/// @example RX.ino
///
/// This example demonstrates FastLED's GPIO ISR RX device on ESP32 by capturing
/// edge transitions from manual pin toggles. This validates the RX device can
/// accurately capture timing data.
///
/// Hardware Setup:
///   - GPIO 0: TX/RX pin (uses same pin for loopback)
///   - No external wiring required (internal loopback test)
///
/// Test Flow:
///   1. Create and initialize GPIO ISR RX device
///   2. Toggle pin in a defined pattern (HIGH/LOW with specific delays)
///   3. Capture edge timing data using ISR RX device
///   4. Analyze and display raw edge timings
///
/// Platform Support:
///   - ESP32 (all variants: classic, S3, C3, C6, etc.)

#include <FastLED.h>
#include "fl/rx_device.h"
#include "fl/error.h"

// ============================================================================
// Configuration
// ============================================================================

// ⚠️  DO NOT CHANGE THESE PIN ASSIGNMENTS - NO EXCEPTIONS! ⚠️
// These specific pins are required for hardware testing infrastructure.
// Changing PIN_TX or PIN_RX will break automated testing equipment.
// PHYSICALLY CONNECT GPIO 0 TO GPIO 19 WITH A WIRE FOR THIS TEST.
#define PIN_TX 0   // DO NOT CHANGE - REQUIRED FOR TEST INFRASTRUCTURE
#define PIN_RX 19  // DO NOT CHANGE - REQUIRED FOR TEST INFRASTRUCTURE
#define EDGE_BUFFER_SIZE 100
#define WAIT_TIMEOUT_MS 100

// ============================================================================
// Pin Toggle Pattern
// ============================================================================

struct PinToggle {
    bool is_high;       // Pin state (HIGH or LOW)
    uint32_t delay_us;  // Delay in microseconds after setting state
};

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

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Test pin loopback connection with digitalWrite/digitalRead
 * @return true if loopback test passes, false otherwise
 */
bool testPinLoopback() {
    FL_WARN("Testing pin loopback connection...");

    // Configure pins
    pinMode(PIN_TX, OUTPUT);
    pinMode(PIN_RX, INPUT);

    // Test LOW state
    digitalWrite(PIN_TX, LOW);
    delayMicroseconds(10);  // Allow signal to settle
    bool low_read = digitalRead(PIN_RX);

    // Test HIGH state
    digitalWrite(PIN_TX, HIGH);
    delayMicroseconds(10);  // Allow signal to settle
    bool high_read = digitalRead(PIN_RX);

    // Reset to LOW
    digitalWrite(PIN_TX, LOW);

    // Validate results
    if (!low_read && high_read) {
        FL_WARN("✓ Pin loopback test PASSED");
        FL_WARN("  LOW: TX=0 -> RX=" << (low_read ? "1" : "0") << " ✓");
        FL_WARN("  HIGH: TX=1 -> RX=" << (high_read ? "1" : "0") << " ✓");
        return true;
    } else {
        FL_ERROR("Pin loopback test FAILED");
        FL_ERROR("  LOW: TX=0 -> RX=" << (low_read ? "1 (expected 0)" : "0 ✓"));
        FL_ERROR("  HIGH: TX=1 -> RX=" << (high_read ? "1 ✓" : "0 (expected 1)"));
        FL_ERROR("");
        FL_ERROR("Please connect GPIO " << PIN_TX << " to GPIO " << PIN_RX << " with a wire.");
        return false;
    }
}

// ============================================================================
// Arduino Setup & Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    FL_WARN("\n=== FastLED GPIO ISR RX Device Test ===");
    FL_WARN("Platform: ESP32");
    FL_WARN("TX Pin: GPIO " << PIN_TX);
    FL_WARN("RX Pin: GPIO " << PIN_RX);
    FL_WARN("");

    // Test pin loopback connection
    if (!testPinLoopback()) {
        while (1) delay(1000);  // Halt on failure
    }
    FL_WARN("");

    // Create RX device
    FL_WARN("Creating GPIO ISR RX device...");
    g_rx_device = fl::RxDevice::create("ISR", PIN_RX, EDGE_BUFFER_SIZE);
    if (!g_rx_device) {
        FL_ERROR("Failed to create GPIO ISR RX device");
        while (1) delay(1000);  // Halt
    }
    FL_WARN("✓ GPIO ISR RX device created\n");

    delay(1000);
}

void executeToggles(fl::RxDevice& rx,
                    const fl::RxConfig& config,
                    fl::span<const PinToggle> toggles,
                    uint32_t wait_ms) {

    // Initialize RX device
    if (!rx.begin(config)) {
        FL_ERROR("Failed to initialize RX device");
        return;
    }

    // Execute pin toggles
    for (size_t i = 0; i < toggles.size(); i++) {
        digitalWrite(PIN_TX, toggles[i].is_high ? HIGH : LOW);
        delayMicroseconds(toggles[i].delay_us);
    }
}


void loop() {
    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ GPIO ISR RX DEVICE TEST");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝\n");

    // Configure RX device
    fl::RxConfig config;
    config.signal_range_min_ns = 100;       // 100ns glitch filter
    config.signal_range_max_ns = 10000000;  // 10ms idle timeout
    config.start_low = true;                 // Pin starts LOW

    // Execute toggles and capture data
    FL_WARN("[TEST] Initializing RX device and executing toggles...");
    executeToggles(*g_rx_device, config, TEST_PATTERN, WAIT_TIMEOUT_MS);

    // Wait for capture completion
    FL_WARN("[TEST] Waiting for capture (timeout: " << WAIT_TIMEOUT_MS << "ms)...");
    auto wait_result = g_rx_device->wait(WAIT_TIMEOUT_MS);

    if (wait_result == fl::RxWaitResult::TIMEOUT) {
        FL_WARN("ERROR: Timeout waiting for data");
    } else if (wait_result == fl::RxWaitResult::BUFFER_OVERFLOW) {
        FL_WARN("WARNING: Buffer overflow during capture");
    } else {
        FL_WARN("[TEST] ✓ Data captured successfully");

        // Get edge timings
        fl::array<fl::EdgeTime, EDGE_BUFFER_SIZE> edge_buffer;
        size_t edge_count = g_rx_device->getRawEdgeTimes(edge_buffer);

        FL_WARN("[TEST] Captured " << edge_count << " edges");

        if (edge_count == 0) {
            FL_WARN("ERROR: No edges captured!");
        } else {
            // Print edge timings
            FL_WARN("[TEST] Edge timings:");
            for (size_t i = 0; i < edge_count; i++) {
                FL_WARN("  [" << i << "] " << (edge_buffer[i].high ? "HIGH" : "LOW ")
                        << " " << edge_buffer[i].ns << "ns (" << (edge_buffer[i].ns / 1000) << "us)");
            }

            // Validate edge alternation
            bool alternation_valid = true;
            for (size_t i = 1; i < edge_count; i++) {
                if (edge_buffer[i].high == edge_buffer[i-1].high) {
                    FL_WARN("ERROR: Sequential " << (edge_buffer[i].high ? "HIGH" : "LOW")
                            << " values at indices " << (i-1) << " and " << i
                            << " - edges should alternate HIGH/LOW");
                    alternation_valid = false;
                }
            }

            // Validate timing accuracy against TEST_PATTERN
            bool timing_valid = true;
            const uint32_t TOLERANCE_PERCENT = 15;  // ±15% tolerance for timing jitter
            FL_WARN("[TEST] Validating timing accuracy (±" << TOLERANCE_PERCENT << "% tolerance):");

            size_t expected_edge_count = TEST_PATTERN.size();
            if (edge_count != expected_edge_count) {
                FL_WARN("WARNING: Edge count mismatch - expected " << expected_edge_count
                        << ", got " << edge_count);
                timing_valid = false;
            }

            size_t compare_count = edge_count < expected_edge_count ? edge_count : expected_edge_count;
            for (size_t i = 0; i < compare_count; i++) {
                uint32_t expected_us = TEST_PATTERN[i].delay_us;
                uint32_t actual_us = edge_buffer[i].ns / 1000;
                bool expected_high = TEST_PATTERN[i].is_high;
                bool actual_high = edge_buffer[i].high;

                // Calculate tolerance range
                uint32_t tolerance_us = (expected_us * TOLERANCE_PERCENT) / 100;
                uint32_t min_us = expected_us - tolerance_us;
                uint32_t max_us = expected_us + tolerance_us;

                // Validate timing and level
                bool timing_ok = (actual_us >= min_us) && (actual_us <= max_us);
                bool level_ok = (expected_high == actual_high);

                if (timing_ok && level_ok) {
                    FL_WARN("  [" << i << "] ✓ " << (actual_high ? "HIGH" : "LOW ")
                            << " " << actual_us << "us (expected " << expected_us
                            << "us ±" << tolerance_us << "us)");
                } else {
                    if (!level_ok) {
                        FL_WARN("  [" << i << "] ✗ Level mismatch: expected "
                                << (expected_high ? "HIGH" : "LOW") << ", got "
                                << (actual_high ? "HIGH" : "LOW"));
                        timing_valid = false;
                    }
                    if (!timing_ok) {
                        FL_WARN("  [" << i << "] ✗ Timing out of range: " << actual_us
                                << "us (expected " << expected_us << "us ±" << tolerance_us
                                << "us, range: " << min_us << "-" << max_us << "us)");
                        timing_valid = false;
                    }
                }
            }

            // Validate results
            if (!alternation_valid) {
                FL_WARN("ERROR: Edge timings are not properly alternating");
                FL_WARN("ERROR: Expected pattern: HIGH, LOW, HIGH, LOW, ...");
                FL_WARN("ERROR: Actual pattern contains sequential identical states");
            } else if (!timing_valid) {
                FL_WARN("ERROR: Captured edge timings do not match expected pattern");
                FL_WARN("ERROR: Check timing accuracy and tolerance settings");
            } else if (edge_count >= 5) {
                FL_WARN("[TEST] ✓ PASS: Captured " << edge_count << " edges with proper alternation");
                FL_WARN("[TEST] ✓ PASS: All timing values match expected pattern within tolerance");
                FL_WARN("[TEST] ✓ GPIO ISR RX device working correctly!");
            } else {
                FL_WARN("WARNING: Only captured " << edge_count << " edges (expected >=5)");
            }
        }
    }

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ TEST COMPLETE - Waiting 5 seconds...");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝\n");

    delay(5000);
}
