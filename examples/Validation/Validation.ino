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
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_decoder.h"

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
#define COLOR_ORDER GRB        // Color ordering

// ============================================================================
// Global State
// ============================================================================

// TX buffer (what we send to LED strip)
CRGB leds_tx[NUM_LEDS];

// RX buffer (what we receive back via RMT)
CRGB leds_rx[NUM_LEDS];

// RX infrastructure
fl::shared_ptr<fl::esp32::RmtRxChannel> rx_channel;  // Pointer to interface (initialized in setup)
fl::esp32::RmtRxDecoder rx_decoder(fl::esp32::CHIPSET_TIMING_WS2812B_RX, 40000000);

// Symbol buffer (1024 symbols = ~42 LEDs @ 24 symbols per LED)
// RmtSymbol is a uint32_t alias that matches ESP-IDF's rmt_symbol_word_t
RmtSymbol rx_symbols[1024];

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
    FastLED.setBrightness(128);  // 50% brightness for stability

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

    // 2. Clear RX state
    rx_channel->reset();
    rx_decoder.reset();
    fl::memset(leds_rx, 0, sizeof(leds_rx));

    // 3. Start RX reception (must start BEFORE TX transmission)
    if (!rx_channel->startReceive(rx_symbols, 1024)) {
        Serial.println("ERROR: Failed to start RX reception");
        return;
    }

    // Small delay to ensure RX is ready
    delayMicroseconds(100);

    // 4. Transmit TX data
    uint32_t tx_start = micros();
    FastLED.show();
    uint32_t tx_duration = micros() - tx_start;

    // 5. Wait for RX completion (timeout after 1 second)
    uint32_t timeout = millis() + 1000;
    while (!rx_channel->isReceiveDone() && millis() < timeout) {
        delayMicroseconds(10);
    }

    if (!rx_channel->isReceiveDone()) {
        Serial.println("ERROR: RX timeout (no signal received)");
        Serial.println("Check: io_loop_back enabled and same pin used for TX/RX");
        return;
    }

    // 6. Decode symbols to bytes
    size_t symbols_received = rx_channel->getReceivedSymbols();
    size_t bytes_decoded = 0;
    bool decode_ok = rx_decoder.decode(
        rx_symbols,
        symbols_received,
        (uint8_t*)leds_rx,
        &bytes_decoded
    );

    if (!decode_ok) {
        Serial.printf("ERROR: Decode failed (errors: %zu / %zu symbols = %.1f%%)\n",
            rx_decoder.getErrorCount(),
            symbols_received,
            100.0 * rx_decoder.getErrorCount() / symbols_received);
        return;
    }

    // 7. Validate TX vs RX
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

    // 8. Report results
    Serial.printf("TX Duration: %u us\n", tx_duration);
    Serial.printf("RX Symbols: %zu (expected: %d)\n", symbols_received, NUM_LEDS * 24);
    Serial.printf("Bytes Decoded: %zu (expected: %d)\n", bytes_decoded, NUM_LEDS * 3);
    Serial.printf("Decode Errors: %zu symbols\n", rx_decoder.getErrorCount());
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
