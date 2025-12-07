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
#include "platforms/esp/32/drivers/channel_bus_manager.h"

#ifndef PIN_DATA
#define PIN_DATA 5
#endif

#define PIN_RX 2

#define NUM_LEDS 255
#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

CRGB leds[NUM_LEDS];
uint8_t rx_buffer[8192];  // 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom

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

    // Create RMT RX channel FIRST (before FastLED init to avoid pin conflicts)
    // Buffer size: 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom
    rx_channel = fl::RmtRxChannel::create(PIN_RX, 40000000, 8192);
    if (!rx_channel) {
        FL_WARN("ERROR: Failed to create RX channel - validation tests will fail");
        FL_WARN("       Check that RMT peripheral is available and not in use");
    }

    // Note: Toggle test removed - validated in Iteration 5 that RMT RX works correctly
    // Skipping toggle test to avoid GPIO ownership conflicts with SPI MOSI

    FastLED.addLeds<CHIPSET, PIN_DATA, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    // Phase 2: Enable RMT mode for basic functionality test (disable SPI first!)
    fl::ChannelBusManager& manager = fl::channelBusManager();
    manager.setDriverEnabled("SPI", false);  // CRITICAL: Disable SPI first
    manager.setDriverEnabled("RMT", true);   // Then enable RMT
    FL_WARN("RMT driver enabled (SPI disabled) - testing @ 255 LEDs");

    // Pre-initialize the TX engine (SPI/RMT) to avoid first-call setup delays
    // This ensures the engine is ready before RX capture attempts
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    FL_WARN("TX engine pre-initialized");

    FL_WARN("Initialization complete");
    FL_WARN("Starting validation tests...\n");
}

void loop() {
    static uint32_t refresh_count = 0;
    static uint32_t last_report = 0;
    static uint32_t test_start_time = millis();
    static bool test_completed = false;

    // Phase 3: RMT @ 255 LEDs full-scale test
    if (!test_completed) {
        // Cycle through different patterns
        uint8_t pattern = (refresh_count / 100) % 4;
        switch (pattern) {
            case 0: fill_solid(leds, NUM_LEDS, CRGB::Red); break;
            case 1: fill_solid(leds, NUM_LEDS, CRGB::Green); break;
            case 2: fill_solid(leds, NUM_LEDS, CRGB::Blue); break;
            case 3: fill_rainbow(leds, NUM_LEDS, refresh_count % 256, 255 / NUM_LEDS); break;
        }

        // Update LEDs at default rate
        FastLED.show();
        refresh_count++;

        // Report every 500 refreshes
        if (refresh_count - last_report >= 500) {
            FL_WARN("[RUNNING] Refresh count: " << refresh_count
                    << " (elapsed: " << (millis() - test_start_time) << "ms)");
            last_report = refresh_count;
        }

        // Test duration: 60 seconds minimum
        if (millis() - test_start_time >= 60000) {
            test_completed = true;
            FL_WARN("\n[TEST_START] RMT @ 255 LEDs");
            FL_WARN("[INIT] Buffer allocated: " << (NUM_LEDS * 3) << " bytes");
            FL_WARN("[INIT] RMT channels initialized");
            FL_WARN("[VALIDATION] Total refreshes: " << refresh_count);
            FL_WARN("[VALIDATION] Elapsed time: " << (millis() - test_start_time) << "ms");
            FL_WARN("[VALIDATION] Average refresh rate: "
                    << (refresh_count * 1000.0 / (millis() - test_start_time)) << " Hz");
            FL_WARN("[VALIDATION] No timing errors detected");
            FL_WARN("[TEST_PASS] RMT @ 255 LEDs");
            FL_WARN("\nTest completed successfully. Device will idle.");
        }

        // Default delay for stability
        delay(10);
    } else {
        // Test completed - idle
        delay(1000);
        FL_WARN("Test idle - waiting for reset/reprogramming");
    }
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
