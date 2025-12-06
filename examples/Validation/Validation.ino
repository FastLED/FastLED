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
//   ⚠️ IMPORTANT: Physical jumper wire required for non-RMT TX → RMT RX loopback
//
//   When non-RMT peripherals are used for TX (e.g., SPI, ParallelIO):
//   - Connect GPIO PIN_DATA to itself with a physical jumper wire
//   - Internal loopback (io_loop_back flag) only works for RMT TX → RMT RX
//   - ESP32 GPIO matrix cannot route other peripheral outputs internally to RMT input
//
//   When RMT is used for TX (lower peripheral priority or disable other peripherals):
//   - No jumper wire needed - io_loop_back works for RMT TX → RMT RX
//
//   Alternative: Connect an LED strip between TX pin and ground, then connect
//   RX pin to LED data line to capture actual LED protocol timing (requires
//   two separate GPIO pins for TX and RX).
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
#include "ValidationTest.h"

#ifndef PIN_DATA
#define PIN_DATA 5
#endif

#define PIN_RX 2

#define NUM_LEDS 10
#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

CRGB leds[NUM_LEDS];
uint8_t rx_buffer[4096];

// RMT RX channel (persistent across loop iterations)
fl::shared_ptr<fl::RmtRxChannel> rx_channel;

void runTest(const char* test_name, int& total, int& passed);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    FL_WARN("\n=== FastLED RMT RX Validation Sketch ===");
    FL_WARN("Hardware: ESP32 (any variant)");
    FL_WARN("TX Pin (MOSI): GPIO " << PIN_DATA << " (ESP32-S3: use GPIO 11 for IO_MUX)");
    FL_WARN("RX Pin: GPIO " << PIN_RX);
    FL_WARN("");
    FL_WARN("⚠️  HARDWARE SETUP REQUIRED:");
    FL_WARN("   If using non-RMT peripherals for TX (e.g., SPI, ParallelIO):");
    FL_WARN("   → Connect GPIO " << PIN_DATA << " to GPIO " << PIN_RX << " with a physical jumper wire");
    FL_WARN("   → Internal loopback (io_loop_back) only works for RMT TX → RMT RX");
    FL_WARN("   → ESP32 GPIO matrix cannot route other peripheral outputs to RMT input");
    FL_WARN("");
    FL_WARN("   ESP32-S3 IMPORTANT: Use GPIO 11 (MOSI) for best performance");
    FL_WARN("   → GPIO 11 is SPI2 IO_MUX pin (bypasses GPIO matrix for 80MHz speed)");
    FL_WARN("   → Other GPIOs use GPIO matrix routing (limited to 26MHz, may see timing issues)");
    FL_WARN("");

    FastLED.addLeds<CHIPSET, PIN_DATA, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    // Pre-initialize the TX engine (SPI/RMT) to avoid first-call setup delays
    // This ensures the engine is ready before RX capture attempts
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    FL_WARN("TX engine pre-initialized");

    // Create RMT RX channel (persistent for all tests)
    rx_channel = fl::RmtRxChannel::create(PIN_RX, 40000000);
    if (!rx_channel) {
        FL_WARN("ERROR: Failed to create RX channel - validation tests will fail");
        FL_WARN("       Check that RMT peripheral is available and not in use");
    }

    FL_WARN("Initialization complete");
    FL_WARN("Starting validation tests...\n");
}

void loop() {
    EVERY_N_MILLISECONDS(1000) { FL_WARN("LOOP VALIDATION"); }

    int total_tests = 0;
    int passed_tests = 0;

    // Test 1: Solid red
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    runTest("Test 1: Solid Red", total_tests, passed_tests);

    // Test 2: Solid green
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    runTest("Test 2: Solid Green", total_tests, passed_tests);

    // Test 3: Solid blue
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    runTest("Test 3: Solid Blue", total_tests, passed_tests);

    // Test 4: Rainbow gradient
    fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS);
    runTest("Test 4: Rainbow Gradient", total_tests, passed_tests);

    // Test 5: Alternating pattern
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = (i % 2) ? CRGB::Blue : CRGB::Red;
    }
    runTest("Test 5: Alternating R/B", total_tests, passed_tests);

    // Test 6: All off (black)
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    runTest("Test 6: All Off", total_tests, passed_tests);

    // Test 7: White (all channels max)
    fill_solid(leds, NUM_LEDS, CRGB::White);
    runTest("Test 7: White", total_tests, passed_tests);

    // Test 8: Gradient red→blue
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t ratio = (i * 255) / (NUM_LEDS - 1);
        leds[i] = CRGB(255 - ratio, 0, ratio);
    }
    runTest("Test 8: Gradient R→B", total_tests, passed_tests);

    // Print summary
    FL_WARN("\n=== Test Suite Complete ===");
    FL_WARN("Results: " << passed_tests << "/" << total_tests << " tests passed ("
            << (100.0 * passed_tests / total_tests) << "%)");

    if (passed_tests == total_tests) {
        FL_WARN("Status: ALL TESTS PASSED ✓✓✓");
    } else {
        FL_WARN("Status: SOME TESTS FAILED ✗");
    }

    FL_WARN("\nWaiting 10 seconds before restart...\n");
    delay(10000);
}

void runTest(const char* test_name, int& total, int& passed) {
    total++;
    FL_WARN("\n=== " << test_name << " ===");

    size_t bytes_captured = capture(rx_channel, rx_buffer);
    if (bytes_captured == 0) {
        FL_WARN("Result: FAIL ✗ (capture failed)");
        return;
    }

    size_t bytes_expected = NUM_LEDS * 3;
    if (bytes_captured > bytes_expected) {
        FL_WARN("Info: Captured " << bytes_captured << " bytes (" << bytes_expected
                << " LED data + " << (bytes_captured - bytes_expected) << " RESET)");
    }

    // Validate: byte-level comparison (COLOR_ORDER is RGB, so no reordering)
    int mismatches = 0;
    size_t bytes_to_check = bytes_captured < bytes_expected ? bytes_captured : bytes_expected;

    for (size_t i = 0; i < NUM_LEDS; i++) {
        size_t byte_offset = i * 3;
        if (byte_offset + 2 >= bytes_to_check) {
            FL_WARN("WARNING: Incomplete data for LED[" << static_cast<int>(i)
                    << "] (only " << bytes_captured << " bytes captured)");
            break;
        }

        uint8_t expected_r = leds[i].r;
        uint8_t expected_g = leds[i].g;
        uint8_t expected_b = leds[i].b;

        uint8_t actual_r = rx_buffer[byte_offset + 0];
        uint8_t actual_g = rx_buffer[byte_offset + 1];
        uint8_t actual_b = rx_buffer[byte_offset + 2];

        if (expected_r != actual_r || expected_g != actual_g || expected_b != actual_b) {
            FL_WARN("ERROR: Mismatch on LED[" << static_cast<int>(i)
                    << "], expected RGB(" << static_cast<int>(expected_r) << ","
                    << static_cast<int>(expected_g) << "," << static_cast<int>(expected_b)
                    << ") but got RGB(" << static_cast<int>(actual_r) << ","
                    << static_cast<int>(actual_g) << "," << static_cast<int>(actual_b) << ")");
            mismatches++;
        }
    }

    FL_WARN("Bytes Captured: " << bytes_captured << " (expected: " << bytes_expected << ")");
    FL_WARN("Accuracy: " << (100.0 * (NUM_LEDS - mismatches) / NUM_LEDS) << "% ("
            << (NUM_LEDS - mismatches) << "/" << NUM_LEDS << " LEDs match)");

    if (mismatches == 0) {
        FL_WARN("Result: PASS ✓");
        passed++;
    } else {
        FL_WARN("Result: FAIL ✗");
    }
}
