// ValidationTest_RMT.h - RMT TX → RMT RX loopback validation

#pragma once

#include <FastLED.h>
#include "ValidationTest.h"

// Multi-lane support check
#ifdef MULTILANE
    #error "RMT driver does not yet support multi-lane mode. Comment out MULTILANE define."
#endif

extern CRGB leds[];
extern uint8_t rx_buffer[];

// RMT RX channel (persistent across loop iterations)
fl::shared_ptr<fl::RmtRxChannel> rx_channel;

inline void runTest(const char* test_name, int& total, int& passed) {
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

inline void validation_setup() {
    FL_WARN("⚠️  HARDWARE SETUP:");
    FL_WARN("   RMT TX → RMT RX loopback (internal, no jumper wire needed)");
    FL_WARN("   io_loop_back flag enabled for RMT → RMT routing");
    FL_WARN("");

    // Initialize RMT RX channel
    FL_WARN("Initializing RMT RX channel on GPIO " << PIN_RX);
    rx_channel = fl::RmtRxChannel::make(PIN_RX, fl::CHIPSET_TIMING_WS2812B_RX);
    if (!rx_channel) {
        FL_WARN("ERROR: Failed to create RX channel");
        while (1) { delay(1000); }
    }

    // Enable internal loopback for RMT TX → RMT RX
    rx_channel->setInternalLoopback(true);
    FL_WARN("RMT RX channel configured (internal loopback enabled)");

    // Initialize FastLED
    FastLED.addLeds<CHIPSET, PIN_DATA, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    // Pre-initialize the TX engine to avoid first-call setup delays
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    FL_WARN("TX engine pre-initialized");
    FL_WARN("Initialization complete");
    FL_WARN("Starting validation tests...\n");
}

inline void validation_loop() {
    static int total = 0, passed = 0;
    static bool tests_run = false;

    if (tests_run) {
        delay(1000);
        return;
    }

    // Run test suite
    FL_WARN("=== RMT TX → RMT RX Loopback Validation ===\n");

    // Test 1: Solid Red
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    runTest("Solid Red", total, passed);

    // Test 2: Solid Green
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    runTest("Solid Green", total, passed);

    // Test 3: Solid Blue
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    runTest("Solid Blue", total, passed);

    // Test 4: Rainbow pattern
    fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS);
    runTest("Rainbow Pattern", total, passed);

    // Test 5: Alternating colors
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = (i % 2 == 0) ? CRGB::Yellow : CRGB::Cyan;
    }
    runTest("Alternating Yellow/Cyan", total, passed);

    // Final results
    FL_WARN("\n=== Test Results ===");
    FL_WARN("Passed: " << passed << "/" << total);
    FL_WARN("Success Rate: " << (100.0 * passed / total) << "%");

    if (passed == total) {
        FL_WARN("\n✓ ALL TESTS PASSED - RMT driver validated successfully");
    } else {
        FL_WARN("\n✗ SOME TESTS FAILED - Check timing configuration");
    }

    tests_run = true;
}
