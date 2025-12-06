// @filter: (platform is esp32)

// examples/Validation/Validation.ino
//
// FastLED LED Timing Validation Sketch for ESP32.
//
// This sketch validates LED output by reading back timing values using the
// RMT peripheral in receive mode. It performs TX→RX loopback testing to verify
// that transmitted LED data matches received data.
//
// Use case: When developing a FastLED driver for a new peripheral, it is useful
// to ready back the LED's recieved to verify that the timing is correct.
//
// Hardware Setup:
//   - No jumper wire needed! Uses io_loop_back for internal GPIO loopback
//   - TX and RX both use the same GPIO pin (software loopback)
//   - Optionally connect an LED strip for visual confirmation
//
// Platform Support:
//   - ESP32 (classic)
//   - ESP32-S3 (Xtensa)
//   - ESP32-C3 (RISC-V)
//   - ESP32-C6 (RISC-V)
//
// Expected Output:
//   Serial monitor will show test results with PASS/FAIL status
//

#include <FastLED.h>
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"

// ============================================================================
// Configuration
// ============================================================================

#ifndef PIN_DATA
#define PIN_DATA 5           // Default data pin (can be overridden by platformio.ini)
#endif

#define SPI_DATA_PIN PIN_DATA  // SPI MOSI → LED Strip
#define RMT_RX_PIN PIN_DATA    // RMT RX input (same pin via io_loop_back)
#define NUM_LEDS 10            // Test with 10 LEDs
#define CHIPSET WS2812B        // LED chipset
#define COLOR_ORDER RGB        // Color ordering, don't change.

// ============================================================================
// Global State
// ============================================================================

// TX buffer (what we send to LED strip)
CRGB leds_tx[NUM_LEDS];

// RX buffer (what we receive back via RMT)
CRGB leds_rx[NUM_LEDS];

// RX infrastructure
fl::shared_ptr<fl::esp32::RmtRxChannel> rx_channel;  // Pointer to interface (initialized in setup)

// Test statistics
int total_tests = 0;
int passed_tests = 0;

// Forward declaration
void runTest(const char* test_name, void (*pattern_fn)());

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);  // Wait for serial or timeout

    Serial.println("\n=== FastLED RMT RX Validation Sketch ===");
    Serial.println("Hardware: ESP32 (any variant)");
    Serial.printf("GPIO Pin: %d (TX and RX via io_loop_back - no jumper needed)\n\n", PIN_DATA);

    // Initialize TX (FastLED SPI driver)
    FastLED.addLeds<CHIPSET, SPI_DATA_PIN, COLOR_ORDER>(leds_tx, NUM_LEDS);
    FastLED.setBrightness(255);  // 50% brightness for stability

    // Initialize RX channel using factory method
    rx_channel = fl::esp32::RmtRxChannel::create(RMT_RX_PIN, 40000000);
    if (!rx_channel || !rx_channel->begin()) {
        Serial.println("FATAL: RX channel init failed");
        Serial.println("Check GPIO pin availability and ESP-IDF RMT5 support");
        while (1) { delay(1000); }
    }

    Serial.println("Initialization complete");
    Serial.println("Starting validation tests...\n");
}

// ============================================================================
// Main Loop - Test Suite
// ============================================================================

void loop() {
    EVERY_N_MILLISECONDS(1000) { FL_WARN("LOOP VALIDATION"); }

    total_tests = 0;
    passed_tests = 0;

    // Test 1: Solid red
    runTest("Solid Red", []() {
        fill_solid(leds_tx, NUM_LEDS, CRGB::Red);
    });

    // Test 2: Solid green
    runTest("Solid Green", []() {
        fill_solid(leds_tx, NUM_LEDS, CRGB::Green);
    });

    // Test 3: Solid blue
    runTest("Solid Blue", []() {
        fill_solid(leds_tx, NUM_LEDS, CRGB::Blue);
    });

    // Test 4: Rainbow gradient
    runTest("Rainbow Gradient", []() {
        fill_rainbow(leds_tx, NUM_LEDS, 0, 255 / NUM_LEDS);
    });

    // Test 5: Alternating pattern
    runTest("Alternating R/B", []() {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds_tx[i] = (i % 2) ? CRGB::Blue : CRGB::Red;
        }
    });

    // Test 6: All off (black)
    runTest("All Off", []() {
        fill_solid(leds_tx, NUM_LEDS, CRGB::Black);
    });

    // Test 7: White (all channels max)
    runTest("White", []() {
        fill_solid(leds_tx, NUM_LEDS, CRGB::White);
    });

    // Test 8: Gradient red→blue
    runTest("Gradient R→B", []() {
        for (int i = 0; i < NUM_LEDS; i++) {
            uint8_t ratio = (i * 255) / (NUM_LEDS - 1);
            leds_tx[i] = CRGB(255 - ratio, 0, ratio);
        }
    });

    // Summary
    Serial.println("\n=== Test Suite Complete ===");
    Serial.printf("Results: %d/%d tests passed (%.1f%%)\n",
        passed_tests, total_tests,
        100.0 * passed_tests / total_tests);

    if (passed_tests == total_tests) {
        Serial.println("Status: ALL TESTS PASSED ✓✓✓");
    } else {
        Serial.println("Status: SOME TESTS FAILED ✗");
    }

    Serial.println("\nWaiting 10 seconds before restart...\n");
    delay(10000);
}

// ============================================================================
// Test Execution
// ============================================================================

void runTest(const char* test_name, void (*pattern_fn)()) {
    total_tests++;
    Serial.printf("\n=== Test %d: %s ===\n", total_tests, test_name);

    // 1. Generate test pattern
    pattern_fn();

    // 2. Clear RX buffer
    fl::memset(leds_rx, 0, sizeof(leds_rx));

    // 3. Re-arm the RX receiver by calling begin() (clears state and starts new receive)
    if (!rx_channel->begin()) {
        Serial.println("ERROR: Failed to begin/arm RX receiver");
        return;
    }

    // 4. Small delay to ensure receiver is fully armed
    delayMicroseconds(100);

    // 5. Transmit TX data (while RX is armed and listening)
    uint32_t tx_start = micros();
    FastLED.show();
    uint32_t tx_duration = micros() - tx_start;

    // 6. Wait for RX completion (timeout after 100ms)
    uint32_t timeout = millis() + 100;
    while (!rx_channel->finished() && millis() < timeout) {
        delayMicroseconds(10);
    }

    if (!rx_channel->finished()) {
        Serial.println("ERROR: RX timeout (no signal received)");
        Serial.println("Check: io_loop_back enabled and same pin used for TX/RX");
        return;
    }

    // 7. Decode symbols to bytes using convenience decode() method
    fl::HeapVector<uint8_t> decoded_bytes;
    auto decode_result = rx_channel->decode(fl::esp32::CHIPSET_TIMING_WS2812B_RX,
                                             fl::back_inserter(decoded_bytes));

    if (!decode_result.ok()) {
        Serial.printf("ERROR: Decode failed (error code: %d)\n",
            static_cast<int>(decode_result.error()));
        return;
    }

    // Copy decoded bytes to leds_rx (limit to expected size to avoid buffer overflow)
    size_t bytes_decoded = decode_result.value();
    size_t bytes_expected = NUM_LEDS * 3;  // 3 bytes per LED (GRB for WS2812B)
    size_t bytes_to_copy = bytes_decoded < bytes_expected ? bytes_decoded : bytes_expected;

    fl::memcpy(leds_rx, decoded_bytes.data(), bytes_to_copy);

    // If we decoded more bytes than expected (due to RESET code), that's OK - just ignore the extra bytes
    if (bytes_decoded > bytes_expected) {
        Serial.printf("Note: Decoded %zu bytes, but only copying first %zu bytes to avoid buffer overflow\n",
                      bytes_decoded, bytes_expected);
    }

    // 8. Validate TX vs RX
    int mismatches = 0;
    for (int i = 0; i < NUM_LEDS; i++) {
        if (leds_tx[i] != leds_rx[i]) {
            Serial.printf("ERROR: Mismatch on LED[%d], expected RGB(%3d,%3d,%3d) but got RGB(%3d,%3d,%3d)\n",
                i,
                leds_tx[i].r, leds_tx[i].g, leds_tx[i].b,
                leds_rx[i].r, leds_rx[i].g, leds_rx[i].b);
            mismatches++;
        }
    }

    // 9. Report results
    Serial.printf("TX Duration: %u us\n", tx_duration);
    Serial.printf("Bytes Decoded: %zu (expected: %d)\n", bytes_decoded, NUM_LEDS * 3);
    Serial.printf("Accuracy: %.1f%% (%d/%d LEDs match)\n",
        100.0 * (NUM_LEDS - mismatches) / NUM_LEDS,
        NUM_LEDS - mismatches,
        NUM_LEDS);

    if (mismatches == 0) {
        Serial.println("Result: PASS ✓");
        passed_tests++;
    } else {
        Serial.println("Result: FAIL ✗");
    }
}
