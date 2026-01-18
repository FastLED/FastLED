// @filter: (platform is esp32)
/// @file DriverTest.ino
/// @brief ESP32 Generic Driver Test - Tests all available LED channel drivers
///
/// ============================================================================
/// OVERVIEW
/// ============================================================================
///
/// This example demonstrates how to:
/// 1. Query available LED drivers at runtime using FastLED's Channel API
/// 2. Test each driver by sending LED data through it
/// 3. Validate that platform-specific expected drivers are present
///
/// The test runs automatically on startup and prints results to Serial.
///
/// ============================================================================
/// HOW IT WORKS
/// ============================================================================
///
/// FastLED's ESP32 implementation provides multiple LED driver backends:
///
///   ┌─────────────────────────────────────────────────────────────────────┐
///   │ Driver   │ Description                     │ Typical Use Case       │
///   ├──────────┼─────────────────────────────────┼────────────────────────┤
///   │ RMT      │ Remote Control peripheral       │ General purpose        │
///   │ SPI      │ Serial Peripheral Interface     │ High-speed strips      │
///   │ I2S      │ Inter-IC Sound (LCD CAM mode)   │ ESP32-S3 parallel      │
///   │ PARLIO   │ Parallel I/O (RMT5 successor)   │ ESP32-C6/H2/P4         │
///   │ UART     │ Universal Async Transmitter     │ Fallback option        │
///   │ LCD_RGB  │ LCD RGB interface               │ ESP32-P4 only          │
///   └──────────┴─────────────────────────────────┴────────────────────────┘
///
/// Each platform has different drivers available based on hardware support.
/// This test automatically detects the platform and validates the expected
/// drivers are present.
///
/// ============================================================================
/// WIRING
/// ============================================================================
///
///   ESP32 Board          LED Strip
///   ┌─────────┐         ┌─────────┐
///   │         │         │         │
///   │  GPIO 2 ├─────────┤ Data In │
///   │         │         │         │
///   │    GND  ├─────────┤ GND     │
///   │         │         │         │
///   │    5V   ├─────────┤ VCC     │
///   │         │         │         │
///   └─────────┘         └─────────┘
///
///   Note: Adjust DATA_PIN in PlatformConfig.h if using a different pin.
///
/// ============================================================================
/// SERIAL OUTPUT EXAMPLE
/// ============================================================================
///
///   +================================================================+
///   | ESP32 Generic Driver Test                                      |
///   | Tests all available LED channel drivers via Channel API        |
///   +================================================================+
///
///   Platform:  ESP32-S3
///   Data Pin:  2
///   LED Count: 60
///
///   +================================================================+
///   | DRIVER VALIDATION FOR ESP32-S3                                 |
///   +================================================================+
///
///   Expected drivers (4):
///     - SPI
///     - RMT
///     - I2S
///     - UART
///
///   Available drivers (4):
///     - SPI (priority: 2, enabled: yes)
///     - RMT (priority: 1, enabled: yes)
///     - I2S (priority: -1, enabled: yes)
///     - UART (priority: 0, enabled: yes)
///
///   Validation results:
///     [PASS] SPI driver found
///     [PASS] RMT driver found
///     [PASS] I2S driver found
///     [PASS] UART driver found
///
///   TEST_SUITE_COMPLETE: PASS
///
/// ============================================================================
/// FILES IN THIS EXAMPLE
/// ============================================================================
///
///   DriverTest.ino    - Main sketch (this file)
///   PlatformConfig.h  - Platform detection, pin/LED configuration
///   TestRunner.h      - Test framework and driver testing logic
///
/// ============================================================================

#include <FastLED.h>
#include "PlatformConfig.h"
#include "TestRunner.h"

// ============================================================================
// LED ARRAY
// ============================================================================

CRGB leds[NUM_LEDS];

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);  // Allow time for serial monitor connection

    // Initialize FastLED with WS2812 strip
    // The driver can be switched at runtime using setExclusiveDriver()
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(TEST_BRIGHTNESS);

    // Create and run the test suite
    DriverTestRunner runner(leds, NUM_LEDS);
    runner.runAllTests();
    runner.printSummary();
}

// ============================================================================
// ARDUINO LOOP
// ============================================================================

void loop() {
    // Tests complete - nothing to do in loop
    // Results were printed in setup()
    delay(10000);
}
