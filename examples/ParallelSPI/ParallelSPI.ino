/// @file    ParallelSPI.ino
/// @brief   Control multiple LED strips simultaneously with parallel output
/// @example ParallelSPI.ino
///
/// This example demonstrates FastLED's parallel SPI output feature, which allows you
/// to drive 2, 4, or 8 LED strips simultaneously instead of sequentially. This can
/// dramatically improve refresh rates for multi-strip installations.
///
/// **How Parallel SPI Works:**
/// When multiple LED strips use clock-based chipsets (APA102, SK9822) with the SAME
/// clock pin, FastLED automatically detects this and enables parallel output mode.
/// The hardware sends data to multiple strips on different GPIO pins simultaneously,
/// using a shared clock signal to synchronize them.
///
/// **When to Use Parallel Output:**
/// - You have 2+ strips of clock-based LEDs (APA102, SK9822, etc.)
/// - You need high refresh rates (>100 FPS with multiple strips)
/// - All strips use the same clock pin (hardware requirement)
/// - Sequential output is causing visible lag or tearing
///
/// **Performance Expectations:**
/// - 2 strips: Up to 2x faster than sequential
/// - 4 strips: Up to 4x faster than sequential
/// - 8 strips: Up to 8x faster than sequential
/// Actual speedup depends on LED count and platform performance.
///
/// **IMPORTANT - LED Chipset Requirements:**
/// ✓ SUPPORTED: APA102, SK9822 (clock-based SPI LEDs)
/// ✗ NOT SUPPORTED: WS2812, WS2811, SK6812 (timing-based, no clock pin)
///
/// **Hardware Setup:**
/// 1. Connect each LED strip's DATA line to consecutive GPIO pins
/// 2. Connect ALL strips' CLOCK lines to the SAME GPIO pin
/// 3. Sharing the clock pin is what triggers parallel mode
///
/// **ESP32 GPIO Pin Recommendations:**
/// For ESP32, avoid strapping pins (0, 2, 15) and input-only pins (34-39).
/// Good choices for data pins: 4, 5, 12-14, 16-19, 21-23, 25-27, 32-33
/// Good choices for clock pin: 18, 19, 23 (default SPI pins)
///
/// **Configuration Below:**
/// Edit NUM_STRIPS, NUM_LEDS, and pin assignments to match your hardware.
///
/// **Platform Support:**
/// - ESP32 (all variants: C3, C2, C6, S2, S3, etc.)
/// - Host simulation (for testing)
/// - Other platforms with SPI hardware support

// ============================================================================
// CONFIGURATION - Edit these to match your hardware
// ============================================================================

// Number of parallel LED strips (must be 2, 4, or 8)
#define NUM_STRIPS 2

// Number of LEDs per strip (all strips should have the same length)
#define NUM_LEDS 30

// Shared clock pin for ALL strips - this is critical for parallel mode!
// Recommendation for ESP32: Use GPIO 18, 19, or 23 (standard SPI clock pins)
#define LED_CLOCK_PIN 18

// Data pin assignments (edit if needed for your board)
// Recommendation for ESP32: Avoid strapping pins 0, 2, 15
#define LED_DATA_PIN_0 4   // Strip 0 data pin
#define LED_DATA_PIN_1 5   // Strip 1 data pin
#define LED_DATA_PIN_2 12  // Strip 2 data pin (only used if NUM_STRIPS >= 4)
#define LED_DATA_PIN_3 13  // Strip 3 data pin (only used if NUM_STRIPS >= 4)
#define LED_DATA_PIN_4 16  // Strip 4 data pin (only used if NUM_STRIPS >= 8)
#define LED_DATA_PIN_5 17  // Strip 5 data pin (only used if NUM_STRIPS >= 8)
#define LED_DATA_PIN_6 21  // Strip 6 data pin (only used if NUM_STRIPS >= 8)
#define LED_DATA_PIN_7 22  // Strip 7 data pin (only used if NUM_STRIPS >= 8)

#include <FastLED.h>

// This example is only compiled for stub platform or ESP32
#if defined(FASTLED_STUB_PLATFORM)

// Validate configuration
#if NUM_STRIPS != 2 && NUM_STRIPS != 4 && NUM_STRIPS != 8
    #error "NUM_STRIPS must be 2, 4, or 8"
#endif

// Check platform support
#if !defined(FASTLED_SPI_HOST_SIMULATION) && !defined(ESP32)
    #error "This example requires ESP32 or host simulation platform"
#endif

// Define LED arrays - one per strip
CRGB leds[NUM_STRIPS][NUM_LEDS];

// Animation state
uint8_t hue = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("FastLED Parallel Output Example");
    Serial.println("================================");
    Serial.print("Number of strips: ");
    Serial.println(NUM_STRIPS);
    Serial.print("LEDs per strip: ");
    Serial.println(NUM_LEDS);
    Serial.print("Clock pin: GPIO");
    Serial.println(LED_CLOCK_PIN);

    // ========================================================================
    // Register LED strips with FastLED
    // ========================================================================
    // CRITICAL: All strips use the SAME clock pin (CLOCK_PIN)
    // This shared clock pin is what triggers FastLED's parallel SPI mode!
    // The hardware will automatically send data to all strips simultaneously.

    #if NUM_STRIPS >= 2
        FastLED.addLeds<APA102, LED_DATA_PIN_0, LED_CLOCK_PIN, BGR>(leds[0], NUM_LEDS);
        FastLED.addLeds<APA102, LED_DATA_PIN_1, LED_CLOCK_PIN, BGR>(leds[1], NUM_LEDS);
        Serial.println("Strip 0: Data=GPIO" + String(LED_DATA_PIN_0) + ", Clock=GPIO" + String(LED_CLOCK_PIN));
        Serial.println("Strip 1: Data=GPIO" + String(LED_DATA_PIN_1) + ", Clock=GPIO" + String(LED_CLOCK_PIN));
    #endif

    #if NUM_STRIPS >= 4
        FastLED.addLeds<APA102, LED_DATA_PIN_2, LED_CLOCK_PIN, BGR>(leds[2], NUM_LEDS);
        FastLED.addLeds<APA102, LED_DATA_PIN_3, LED_CLOCK_PIN, BGR>(leds[3], NUM_LEDS);
        Serial.println("Strip 2: Data=GPIO" + String(LED_DATA_PIN_2) + ", Clock=GPIO" + String(LED_CLOCK_PIN));
        Serial.println("Strip 3: Data=GPIO" + String(LED_DATA_PIN_3) + ", Clock=GPIO" + String(LED_CLOCK_PIN));
    #endif

    #if NUM_STRIPS >= 8
        FastLED.addLeds<APA102, LED_DATA_PIN_4, LED_CLOCK_PIN, BGR>(leds[4], NUM_LEDS);
        FastLED.addLeds<APA102, LED_DATA_PIN_5, LED_CLOCK_PIN, BGR>(leds[5], NUM_LEDS);
        FastLED.addLeds<APA102, LED_DATA_PIN_6, LED_CLOCK_PIN, BGR>(leds[6], NUM_LEDS);
        FastLED.addLeds<APA102, LED_DATA_PIN_7, LED_CLOCK_PIN, BGR>(leds[7], NUM_LEDS);
        Serial.println("Strip 4: Data=GPIO" + String(LED_DATA_PIN_4) + ", Clock=GPIO" + String(LED_CLOCK_PIN));
        Serial.println("Strip 5: Data=GPIO" + String(LED_DATA_PIN_5) + ", Clock=GPIO" + String(LED_CLOCK_PIN));
        Serial.println("Strip 6: Data=GPIO" + String(LED_DATA_PIN_6) + ", Clock=GPIO" + String(LED_CLOCK_PIN));
        Serial.println("Strip 7: Data=GPIO" + String(LED_DATA_PIN_7) + ", Clock=GPIO" + String(LED_CLOCK_PIN));
    #endif

    Serial.println("================================");
    Serial.println("Setup complete!");
    Serial.println("");
}

void loop() {
    // Create a different effect on each strip

    // Strip 0: Rainbow wave
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[0][i] = CHSV(hue + (i * 256 / NUM_LEDS), 255, 255);
    }

    #if NUM_STRIPS >= 2
    // Strip 1: Pulse effect
    uint8_t brightness = beatsin8(60, 0, 255);
    fill_solid(leds[1], NUM_LEDS, CHSV(hue + 85, 255, brightness));
    #endif

    #if NUM_STRIPS >= 4
    // Strip 2: Moving dot
    fill_solid(leds[2], NUM_LEDS, CRGB::Black);
    uint8_t pos = beatsin8(30, 0, NUM_LEDS - 1);
    leds[2][pos] = CHSV(hue + 170, 255, 255);

    // Strip 3: Static color
    fill_solid(leds[3], NUM_LEDS, CHSV(hue, 255, 255));
    #endif

    #if NUM_STRIPS >= 8
    // Strip 4: Rainbow fill
    fill_rainbow(leds[4], NUM_LEDS, hue, 7);

    // Strip 5: Sine wave brightness
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t b = sin8(hue + (i * 8));
        leds[5][i] = CHSV(160, 255, b);
    }

    // Strip 6: Alternating colors
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[6][i] = (i % 2) ? CHSV(hue, 255, 255) : CHSV(hue + 128, 255, 255);
    }

    // Strip 7: Fire effect simulation
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t heat = random8(128, 255);
        leds[7][i] = CHSV(random8(20), 255, heat);
    }
    #endif

    // Update all strips simultaneously - this is where parallel output shines!
    FastLED.show();

    // Advance animation
    hue++;

    // Print frame rate every 100 frames
    EVERY_N_MILLISECONDS(1000) {
        static uint32_t frameCount = 0;
        static uint32_t lastTime = millis();
        uint32_t now = millis();
        uint32_t elapsed = now - lastTime;

        if (elapsed > 0) {
            uint32_t fps = (frameCount * 1000) / elapsed;
            Serial.print("FPS: ");
            Serial.println(fps);

            frameCount = 0;
            lastTime = now;
        }
        frameCount++;
    }
}
#else
void setup() {}
void loop() {}

#endif // defined(FASTLED_STUB_PLATFORM)
