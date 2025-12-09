// @filter: (platform is esp32)

// examples/Validation/Validation.ino
//
// FastLED LED Timing Validation Sketch for ESP32.
//
// This sketch validates LED output by testing different driver backends:
//   - RMT: RMT TX → RMT RX loopback (internal, no jumper wire needed)
//   - SPI: SPI TX → RMT RX loopback (requires physical jumper wire)
//   - PARLIO: PARLIO TX validation (ESP32-C6 only, single or multi-lane)
//
// Usage:
//   Edit the driver selection defines below to choose which driver to test.
//   Uncomment ONE driver define (VALIDATE_RMT, VALIDATE_SPI, or VALIDATE_PARLIO).
//
// Platform Support:
//   - ESP32 (classic) - RMT, SPI
//   - ESP32-S3 (Xtensa) - RMT, SPI
//   - ESP32-C3 (RISC-V) - RMT, SPI
//   - ESP32-C6 (RISC-V) - RMT, SPI, PARLIO
//

#include <FastLED.h>

// ============================================================================
// USER CONFIGURATION - Edit these defines to select driver and test mode
// ============================================================================

// Driver Selection: Uncomment ONE driver to test
// #define VALIDATE_RMT       // Test RMT TX → RMT RX loopback (no jumper wire needed)
// #define VALIDATE_SPI       // Test SPI TX → RMT RX loopback (requires physical jumper wire)
#define VALIDATE_PARLIO    // Test PARLIO TX (ESP32-C6 only)

// Multi-lane option (available for drivers that support it)
// Note: Currently only PARLIO supports multi-lane mode
#define MULTILANE   // Enable 4-lane testing (comment out for single lane)

// Hardware Configuration
#ifndef PIN_DATA
#define PIN_DATA 5
#endif
#define PIN_RX 0

#define NUM_LEDS 10
#define CHIPSET WS2812B
#define COLOR_ORDER RGB

// ============================================================================
// DRIVER-SPECIFIC IMPLEMENTATION (auto-selected based on defines above)
// ============================================================================

#if defined(VALIDATE_RMT)
    #include "ValidationTest_RMT.h"
    #define DRIVER_NAME "RMT"
#elif defined(VALIDATE_SPI)
    #include "ValidationTest_SPI.h"
    #define DRIVER_NAME "SPI"
#elif defined(VALIDATE_PARLIO)
    #include "ValidationTest_PARLIO.h"
    #define DRIVER_NAME "PARLIO"
#else
    #error "No driver selected! Uncomment one of: VALIDATE_RMT, VALIDATE_SPI, VALIDATE_PARLIO"
#endif

// LED arrays (allocation depends on multi-lane mode)
#ifdef MULTILANE
    CRGB leds_lane0[NUM_LEDS];
    CRGB leds_lane1[NUM_LEDS];
    CRGB leds_lane2[NUM_LEDS];
    CRGB leds_lane3[NUM_LEDS];
#else
    CRGB leds[NUM_LEDS];
    uint8_t rx_buffer[8192];  // 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom
#endif

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    FL_WARN("\n=== FastLED Validation - " << DRIVER_NAME << " Driver ===");
    FL_WARN("TX Pin: GPIO " << PIN_DATA);
    FL_WARN("RX Pin: GPIO " << PIN_RX);
    FL_WARN("");

#ifdef MULTILANE
    validation_setup_multilane();
#else
    validation_setup();
#endif
}

void loop() {
#ifdef MULTILANE
    validation_loop_multilane();
#else
    validation_loop();
#endif
}
