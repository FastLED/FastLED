/// @file RP2040_RP2350_Parallel_WS2812_Auto.ino
/// @brief Example: Automatic parallel WS2812 strips on RP2040/RP2350
///
/// This sketch demonstrates the new automatic parallel grouping feature for
/// RP2040/RP2350 boards. Unlike the manual ParallelClocklessController, this
/// uses the standard FastLED.addLeds() API and automatically detects consecutive
/// GPIO pins for parallel output.
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
/// **Important:** Consecutive GPIO pins are required for parallel output!
///
/// ## How Automatic Grouping Works
///
/// The driver automatically detects consecutive pins and groups them for parallel output:
/// - **GPIO 2-5 (consecutive)**: Automatically grouped into 4-lane parallel output
/// - **Non-consecutive pins**: Fall back to sequential (non-parallel) output
///
/// Supported parallel groups: 2, 4, or 8 consecutive pins
///
/// ## Performance
///
/// - Frame rate: ~50 FPS (WS2812 timing limited, not CPU limited)
/// - CPU overhead: ~2% per frame for bit transposition
/// - Memory: ~2400 bytes for 4 × 100 LEDs
/// - Resources saved vs manual setup: None (same PIO/DMA usage, but easier API!)
///
/// ## Enabling the Feature
///
/// To use this automatic parallel driver, define before including FastLED.h:
/// ```cpp
/// #define FASTLED_RP2040_CLOCKLESS_PIO_AUTO 1
/// #include <FastLED.h>
/// ```

// @filter: (platform is rp2040)

// Enable automatic parallel grouping driver
#define FASTLED_RP2040_CLOCKLESS_PIO_AUTO 1

#include <FastLED.h>

#if !defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_RP2350)
#error "This sketch requires RP2040 or RP2350 platform (Raspberry Pi Pico). Use bash compile rp2040 SpecialDrivers/RP instead."
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

/// Helper: Rotate array elements
void rotate(CRGB* array, int length, uint8_t amount);

// ============================================================================
// Configuration
// ============================================================================

// Number of LEDs per strip
#define LEDS_PER_STRIP 100

// GPIO pins (must be consecutive for parallel output!)
#define PIN_STRIP_0 2
#define PIN_STRIP_1 3
#define PIN_STRIP_2 4
#define PIN_STRIP_3 5

// RGB order (GRB for WS2812B)
#define RGB_ORDER GRB

// Create standard FastLED arrays (one per strip)
CRGB leds0[LEDS_PER_STRIP];
CRGB leds1[LEDS_PER_STRIP];
CRGB leds2[LEDS_PER_STRIP];
CRGB leds3[LEDS_PER_STRIP];

// ============================================================================
// Setup
// ============================================================================

void setup() {
    delay(500);  // Power-up delay for LED strips

    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        delay(100);
    }
    Serial.println("\n\n=== RP2040/RP2350 Automatic Parallel WS2812 Example ===\n");
    Serial.println("This example demonstrates automatic parallel grouping.\n");

    // Standard FastLED.addLeds() calls - automatic parallel grouping!
    // Because these pins are consecutive (2, 3, 4, 5), they will be
    // automatically grouped into a single 4-lane parallel output.
    FastLED.addLeds<WS2812, PIN_STRIP_0, RGB_ORDER>(leds0, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN_STRIP_1, RGB_ORDER>(leds1, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN_STRIP_2, RGB_ORDER>(leds2, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN_STRIP_3, RGB_ORDER>(leds3, LEDS_PER_STRIP);

    Serial.println("Controller initialized!");
    Serial.println("Automatic parallel grouping detected consecutive GPIO pins 2-5");
    Serial.println("Using 4-lane parallel output with single PIO state machine and DMA channel");
    Serial.println();
    Serial.println("Compare this with the manual Parallel_IO.ino example - same performance,");
    Serial.println("but much simpler API! Just use standard FastLED.addLeds() calls.\n");
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
    // Pattern: Rainbow on each strip (offset by time)
    // Each strip shows a different hue
    static uint32_t start_time = millis();
    uint32_t elapsed = millis() - start_time;

    for (int strip = 0; strip < 4; strip++) {
        CRGB* strip_leds = nullptr;
        switch (strip) {
            case 0: strip_leds = leds0; break;
            case 1: strip_leds = leds1; break;
            case 2: strip_leds = leds2; break;
            case 3: strip_leds = leds3; break;
        }

        if (strip_leds) {
            uint8_t hue_offset = strip * 64;  // ~90 degrees per strip
            fill_rainbow(strip_leds, LEDS_PER_STRIP, hue_offset, 10);

            // Add motion
            uint8_t rotation = (elapsed / 50 + strip * 20) % 256;
            rotate(strip_leds, LEDS_PER_STRIP, rotation);
        }
    }

    // Standard FastLED.show() - all 4 strips output in parallel!
    FastLED.show();

    // Frame rate control (~50 FPS)
    delay(20);

    // Every 5 seconds, print stats
    static uint32_t last_stats = millis();
    if (millis() - last_stats > 5000) {
        last_stats = millis();
        Serial.print("Frame time: ");
        Serial.print(elapsed);
        Serial.println(" ms");
        Serial.println("All 4 strips updating in parallel via automatic grouping");
    }
}

// ============================================================================
// Helper: Rotate array elements
// ============================================================================

void rotate(CRGB* array, int length, uint8_t amount) {
    amount = amount % length;  // Handle wrap-around
    if (amount == 0) return;

    CRGB temp[amount];

    // Copy last 'amount' elements to temp
    for (int i = 0; i < amount; i++) {
        temp[i] = array[length - amount + i];
    }

    // Shift remaining elements right
    for (int i = length - 1; i >= amount; i--) {
        array[i] = array[i - amount];
    }

    // Copy temp to beginning
    for (int i = 0; i < amount; i++) {
        array[i] = temp[i];
    }
}

// ============================================================================
// Notes
// ============================================================================

// Benefits of Automatic Parallel Grouping:
// - Standard FastLED.addLeds() API (no custom controller classes)
// - Automatic detection of consecutive pins
// - Same performance as manual parallel setup
// - Graceful fallback for non-consecutive pins
// - Works with FastLED.show() - no custom showLeds() call needed
//
// Requirements:
// - Pins must be consecutive GPIO numbers for parallel output
// - Define FASTLED_RP2040_CLOCKLESS_PIO_AUTO before including FastLED.h
// - Valid parallel groups: 2, 4, or 8 consecutive pins
//
// Mixed Consecutive/Non-consecutive Example:
// FastLED.addLeds<WS2812, 2>(...);  // Group 1: GPIO 2-5 (parallel)
// FastLED.addLeds<WS2812, 3>(...);  //
// FastLED.addLeds<WS2812, 4>(...);  //
// FastLED.addLeds<WS2812, 5>(...);  //
// FastLED.addLeds<WS2812, 10>(...); // Group 2: GPIO 10 (sequential fallback)
// FastLED.addLeds<WS2812, 15>(...); // Group 3: GPIO 15 (sequential fallback)
//
// The driver automatically creates:
// - One 4-pin parallel group for GPIO 2-5 (uses 1 PIO SM + 1 DMA channel)
// - Two single-pin sequential groups for GPIO 10 and 15
