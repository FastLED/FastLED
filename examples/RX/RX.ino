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

// ============================================================================
// Configuration
// ============================================================================

#define PIN_TX 0
#define PIN_RX PIN_TX  // Same pin for loopback test
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
// Arduino Setup & Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    FL_WARN("\n=== FastLED GPIO ISR RX Device Test ===");
    FL_WARN("Platform: ESP32");
    FL_WARN("Pin: GPIO " << PIN_TX << " (TX/RX loopback)");
    FL_WARN("");

    // Configure TX pin
    pinMode(PIN_TX, OUTPUT);
    digitalWrite(PIN_TX, LOW);

    // Create RX device
    FL_WARN("Creating GPIO ISR RX device...");
    g_rx_device = fl::RxDevice::create("ISR", PIN_RX, EDGE_BUFFER_SIZE);
    if (!g_rx_device) {
        FL_WARN("ERROR: Failed to create GPIO ISR RX device");
        while (1) delay(1000);  // Halt
    }
    FL_WARN("✓ GPIO ISR RX device created\n");

    delay(1000);
}


// ============================================================================
// Helper Functions
// ============================================================================

void executeToggles(fl::RxDevice& rx,
                    const fl::RxConfig& config,
                    fl::span<const PinToggle> toggles,
                    uint32_t wait_ms) {

    // Initialize RX device
    if (!rx.begin(config)) {
        FL_WARN("ERROR: Failed to initialize RX device");
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

            // Validate results
            if (!alternation_valid) {
                FL_WARN("[TEST] ✗ FAIL: Edge timings are not properly alternating");
                FL_WARN("[TEST] ✗ Expected pattern: HIGH, LOW, HIGH, LOW, ...");
                FL_WARN("[TEST] ✗ Actual pattern contains sequential identical states");
            } else if (edge_count >= 5) {
                FL_WARN("[TEST] ✓ PASS: Captured " << edge_count << " edges with proper alternation");
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
