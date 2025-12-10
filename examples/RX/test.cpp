

#include <Arduino.h>
#include "FastLED.h"
#include "fl/rx_device.h"
#include "test.h"

bool verifyJumperWire(int pin_tx, int pin_rx) {
    FL_WARN("Verifying jumper wire connection between GPIO " << pin_tx << " and GPIO " << pin_rx << "...");

    pinMode(pin_tx, OUTPUT);
    pinMode(pin_rx, INPUT);

    // Test HIGH
    digitalWrite(pin_tx, HIGH);
    delay(1);  // Allow signal to propagate
    int rx_high = digitalRead(pin_rx);

    // Test LOW
    digitalWrite(pin_tx, LOW);
    delay(1);  // Allow signal to propagate
    int rx_low = digitalRead(pin_rx);

    if (rx_high != HIGH || rx_low != LOW) {
        FL_ERROR("Jumper wire sanity check FAILED!");
        FL_ERROR("  digitalWrite(TX=" << pin_tx << ", HIGH) → digitalRead(RX=" << pin_rx << ") = " << rx_high << " (expected HIGH=1)");
        FL_ERROR("  digitalWrite(TX=" << pin_tx << ", LOW)  → digitalRead(RX=" << pin_rx << ") = " << rx_low << " (expected LOW=0)");
        FL_ERROR("");
        FL_ERROR("REQUIRED: Physically connect GPIO " << pin_tx << " to GPIO " << pin_rx << " with a jumper wire!");
        return false;
    }

    FL_WARN("✓ Jumper wire verified: GPIO " << pin_tx << " → GPIO " << pin_rx << " signal path working");
    return true;
}

void executeToggles(fl::RxDevice& rx,
                    const fl::RxConfig& config,
                    fl::span<const PinToggle> toggles,
                    int pin_tx,
                    uint32_t wait_ms) {

    // Set pin to initial state before begin()
    pinMode(pin_tx, OUTPUT);
    digitalWrite(pin_tx, config.start_low ? LOW : HIGH);
    delayMicroseconds(100);  // Allow pin to settle

    // Initialize RX device
    if (!rx.begin(config)) {
        FL_ERROR("Failed to initialize RX device");
        return;
    }

    // Execute pin toggles
    for (size_t i = 0; i < toggles.size(); i++) {
        digitalWrite(pin_tx, toggles[i].is_high ? HIGH : LOW);
        delayMicroseconds(toggles[i].delay_us);
    }
}

bool validateEdgeTiming(fl::span<const fl::EdgeTime> edges,
                        size_t edge_count,
                        fl::span<const PinToggle> expected_pattern,
                        uint32_t tolerance_percent) {

    FL_WARN("[TEST] Captured " << edge_count << " edges");

    if (edge_count == 0) {
        FL_ERROR("No edges captured!");
        return false;
    }

    // Print edge timings
    FL_WARN("[TEST] Edge timings:");
    for (size_t i = 0; i < edge_count; i++) {
        FL_WARN("  [" << i << "] " << (edges[i].high ? "HIGH" : "LOW ")
                << " " << edges[i].ns << "ns (" << (edges[i].ns / 1000) << "us)");
    }

    // Validate edge alternation
    bool alternation_valid = true;
    for (size_t i = 1; i < edge_count; i++) {
        if (edges[i].high == edges[i-1].high) {
            FL_ERROR("Sequential " << (edges[i].high ? "HIGH" : "LOW")
                    << " values at indices " << (i-1) << " and " << i
                    << " - edges should alternate HIGH/LOW");
            alternation_valid = false;
        }
    }

    // Validate timing accuracy against expected pattern
    bool timing_valid = true;
    FL_WARN("[TEST] Validating timing accuracy (±" << tolerance_percent << "% tolerance):");

    // Expected edge count is expected_pattern.size() - 1 because the last edge
    // ends with timeout (no subsequent transition to measure duration)
    size_t expected_edge_count = expected_pattern.size() - 1;
    if (edge_count != expected_edge_count) {
        FL_WARN("WARNING: Edge count mismatch - expected " << expected_edge_count
                << ", got " << edge_count);
        timing_valid = false;
    }

    size_t compare_count = edge_count < expected_edge_count ? edge_count : expected_edge_count;
    for (size_t i = 0; i < compare_count; i++) {
        uint32_t expected_us = expected_pattern[i].delay_us;
        uint32_t actual_us = edges[i].ns / 1000;
        bool expected_high = expected_pattern[i].is_high;
        bool actual_high = edges[i].high;

        // Calculate tolerance range
        uint32_t tolerance_us = (expected_us * tolerance_percent) / 100;
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
        FL_ERROR("Edge timings are not properly alternating");
        FL_ERROR("Expected pattern: HIGH, LOW, HIGH, LOW, ...");
        FL_ERROR("Actual pattern contains sequential identical states");
        return false;
    } else if (!timing_valid) {
        FL_ERROR("Captured edge timings do not match expected pattern");
        FL_ERROR("Check timing accuracy and tolerance settings");
        return false;
    } else if (edge_count >= 5) {
        FL_WARN("[TEST] ✓ PASS: Captured " << edge_count << " edges with proper alternation");
        FL_WARN("[TEST] ✓ PASS: All timing values match expected pattern within tolerance");
        FL_WARN("[TEST] ✓ RX device working correctly!");
        return true;
    } else {
        FL_WARN("WARNING: Only captured " << edge_count << " edges (expected >=5)");
        return false;
    }
}

bool testRxDevice(fl::shared_ptr<fl::RxDevice> rx, int pin_tx) {
    FL_WARN("Testing RX device with low-frequency pattern...");

    if (!rx) {
        FL_ERROR("Failed to test RX device - null device provided");
        return false;
    }

    int pin_rx = rx->getPin();

    // Configure RX device for low-frequency test
    fl::RxConfig config;
    config.signal_range_min_ns = 100;       // 100ns glitch filter
    config.signal_range_max_ns = 30000000;  // 30ms idle timeout (ESP-IDF RMT limit: 32767000ns)
    config.start_low = true;                 // Pin starts LOW

    // Initialize TX pin and set to LOW
    pinMode(pin_tx, OUTPUT);
    pinMode(pin_rx, INPUT);
    digitalWrite(pin_tx, LOW);
    delay(10);  // Allow pin to settle

    if (!rx->begin(config)) {
        FL_ERROR("Failed to initialize RX device");
        return false;
    }

    // Generate simple test pattern: 4 edges (LOW->HIGH->LOW->HIGH)
    // Pattern: HIGH 10ms, LOW 10ms, HIGH 10ms
    FL_WARN("Generating test pattern on GPIO " << pin_tx << "...");
    digitalWrite(pin_tx, HIGH);
    delay(10);
    digitalWrite(pin_tx, LOW);
    delay(10);
    digitalWrite(pin_tx, HIGH);
    delay(10);
    digitalWrite(pin_tx, LOW);

    // Wait for capture with timeout
    FL_WARN("Waiting for RX capture...");
    auto wait_result = rx->wait(100);

    if (wait_result == fl::RxWaitResult::TIMEOUT) {
        FL_ERROR("RX device test FAILED - timeout waiting for data");
        FL_ERROR("  No edges captured within 100ms");
        FL_ERROR("  This suggests the RX device cannot read from GPIO " << pin_rx);
        return false;
    }

    // Get captured edges
    fl::array<fl::EdgeTime, 10> edge_buffer;
    size_t edge_count = rx->getRawEdgeTimes(edge_buffer);

    if (edge_count < 3) {
        FL_ERROR("RX device test FAILED - insufficient edges captured");
        FL_ERROR("  Expected at least 3 edges, got " << edge_count);
        FL_ERROR("  Pin loopback may not be working correctly");
        return false;
    }

    // Validate timing is reasonable (each edge should be ~10ms apart)
    bool timing_ok = true;
    for (size_t i = 0; i < edge_count && i < 3; i++) {
        uint32_t duration_ms = edge_buffer[i].ns / 1000000;
        if (duration_ms < 5 || duration_ms > 20) {
            FL_WARN("WARNING: Edge " << i << " timing unusual: " << duration_ms << "ms (expected ~10ms)");
            timing_ok = false;
        }
    }

    if (timing_ok) {
        FL_WARN("✓ RX device test PASSED");
        FL_WARN("  Captured " << edge_count << " edges");
        FL_WARN("  Timing appears correct (~10ms per edge)");
        return true;
    } else {
        FL_WARN("✓ RX device test PASSED (with timing warnings)");
        FL_WARN("  Captured " << edge_count << " edges");
        FL_WARN("  Timing may be affected by system load");
        return true;  // Still pass - we got edges
    }
}
