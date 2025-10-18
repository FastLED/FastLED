/// @file RP2040_RP2350_Parallel_WS2812_4Strip.ino
/// @brief Example: 4 parallel WS2812 strips on RP2040/RP2350
///
/// This sketch demonstrates driving 4 LED strips in parallel on RP2040/RP2350 boards
/// using a single PIO state machine and DMA channel.
///
/// ## Hardware Setup
///
/// **RP2040 Pico or RP2350:**
/// - Strip 0 → GPIO 2
/// - Strip 1 → GPIO 3
/// - Strip 2 → GPIO 4
/// - Strip 3 → GPIO 5
/// - GND → GND
/// - Power → 5V (with appropriate current limiting/power supply)
///
/// **Important:** All 4 data pins must be consecutive GPIO pins!
///
/// ## Performance
///
/// - Frame rate: ~50 FPS (WS2812 timing limited, not CPU limited)
/// - CPU overhead: ~2% per frame for bit transposition
/// - Memory: ~2400 bytes for 4 × 100 LEDs
/// - Resources saved: 3 PIO state machines, 3 DMA channels

#include <FastLED.h>

// ============================================================================
// Configuration
// ============================================================================

// Number of LEDs per strip
#define LEDS_PER_STRIP 100

// Base GPIO pin (must have 4 consecutive pins available)
#define BASE_PIN 2

// RGB order (GRB for WS2812B)
#define RGB_ORDER GRB

// Create LED arrays for 4 strips
CRGB leds[4][LEDS_PER_STRIP];

// Create the parallel controller
// Template parameters:
//   BASE_PIN: Starting GPIO pin (2, 3, 4, 5 in this case)
//   NUM_STRIPS: 4 strips (2, 4, or 8 supported)
//   T1_NS: High pulse (400 ns for WS2812B)
//   T2_NS: Low pulse (850 ns for WS2812B)
//   T3_NS: Reset time (50000 ns = 50 µs for WS2812B)
//   RGB_ORDER: Color order
fl::ParallelClocklessController<
    BASE_PIN, 4,          // Base pin, 4 strips
    400, 850, 50000,      // WS2812B timing: T1, T2, T3
    GRB                   // Color order
> controller;

// ============================================================================
// Setup
// ============================================================================

void setup() {
    delay(500);  // Power-up delay for LED strips

    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        delay(100);
    }
    Serial.println("\n\n=== RP2040/RP2350 Parallel WS2812 Example ===\n");

    // Register all 4 strips with the controller
    for (int i = 0; i < 4; i++) {
        if (!controller.addStrip(i, leds[i], LEDS_PER_STRIP)) {
            Serial.printf("ERROR: Failed to register strip %d\n", i);
            while (1) { delay(1000); }  // Halt on error
        }
    }

    // Initialize the controller
    if (!controller.init()) {
        Serial.println("ERROR: Controller initialization failed!");
        Serial.println("Possible causes:");
        Serial.println("  - No free PIO state machine available");
        Serial.println("  - No free DMA channel available");
        Serial.println("  - GPIO pins already in use");
        while (1) { delay(1000); }  // Halt on error
    }

    Serial.println("✓ Controller initialized successfully");
    Serial.printf("  Base pin: GPIO %d\n", BASE_PIN);
    Serial.printf("  Strips: 4\n");
    Serial.printf("  LEDs per strip: %d\n", LEDS_PER_STRIP);
    Serial.printf("  Total LEDs: %d\n", 4 * LEDS_PER_STRIP);
    Serial.println();
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
    // Pattern 1: Rainbow on each strip (offset by time)
    // Each strip shows a different hue
    static uint32_t start_time = millis();
    uint32_t elapsed = millis() - start_time;

    for (int strip = 0; strip < 4; strip++) {
        uint8_t hue_offset = strip * 64;  // ~90 degrees per strip
        fill_rainbow(leds[strip], LEDS_PER_STRIP, hue_offset, 10);
    }

    // Add motion
    for (int strip = 0; strip < 4; strip++) {
        uint8_t rotation = (elapsed / 50 + strip * 20) % 256;
        rotate(leds[strip], LEDS_PER_STRIP, rotation);
    }

    // Display the data
    controller.showPixels(0xFF);  // 255 = max brightness

    // Frame rate control (~50 FPS)
    delay(20);

    // Every 5 seconds, print stats
    static uint32_t last_stats = millis();
    if (millis() - last_stats > 5000) {
        last_stats = millis();
        Serial.printf("Frame: %lu ms\n", elapsed);
    }
}

// ============================================================================
// Helper: Rotate array elements
// ============================================================================

void rotate(CRGB* array, int length, uint8_t amount) {
    amount = amount % length;  // Handle wrap-around
    CRGB temp[amount];
    memcpy(temp, array + length - amount, amount * sizeof(CRGB));
    memmove(array + amount, array, (length - amount) * sizeof(CRGB));
    memcpy(array, temp, amount * sizeof(CRGB));
}

// ============================================================================
// Example Patterns
// ============================================================================

/// Solid color pattern (all strips same color)
void pattern_solid_color() {
    CRGB color = CRGB::Red;
    for (int strip = 0; strip < 4; strip++) {
        fill_solid(leds[strip], LEDS_PER_STRIP, color);
    }
}

/// Chase pattern (running light across all strips)
void pattern_chase(uint32_t speed_ms) {
    static uint32_t last_update = millis();
    static uint8_t position = 0;

    if (millis() - last_update > speed_ms) {
        last_update = millis();
        position = (position + 1) % LEDS_PER_STRIP;
    }

    for (int strip = 0; strip < 4; strip++) {
        fadeToBlackBy(leds[strip], LEDS_PER_STRIP, 32);
        leds[strip][position] = CRGB::Green;
    }
}

/// Alternating pattern (even/odd LEDs different colors)
void pattern_alternating() {
    for (int strip = 0; strip < 4; strip++) {
        for (int i = 0; i < LEDS_PER_STRIP; i++) {
            leds[strip][i] = (i % 2 == 0) ? CRGB::Blue : CRGB::Purple;
        }
    }
}

/// Breathing pattern (brightness animation)
void pattern_breathing(uint32_t period_ms) {
    static uint32_t start = millis();
    uint32_t elapsed = millis() - start;
    uint8_t brightness = 128 + 127 * sin8(256 * elapsed / period_ms);

    for (int strip = 0; strip < 4; strip++) {
        fill_solid(leds[strip], LEDS_PER_STRIP, CRGB::Cyan);
        for (int i = 0; i < LEDS_PER_STRIP; i++) {
            leds[strip][i].nscale8(brightness);
        }
    }
}

// Notes:
// - All 4 data pins MUST be consecutive GPIO pins (e.g., 2,3,4,5)
// - This saves 3 PIO state machines compared to sequential 4-strip setup
// - CPU time spent on bit transposition is < 2% per frame
// - Maximum frame rate is WS2812 timing limited (~327 FPS theoretical)
// - See DESIGN.md in the repository for technical details
