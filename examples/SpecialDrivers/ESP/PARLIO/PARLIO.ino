// @filter: (platform is esp32) and ((target is ESP32C6) or (target is ESP32P4) or (target is ESP32H2) or (target is ESP32C5))

/// ESP32 PARLIO Parallel LED Demo
///
/// This example demonstrates using the PARLIO (Parallel I/O) peripheral on
/// ESP32-C6, ESP32-P4, ESP32-H2, or ESP32-C5 to drive multiple LED strips
/// simultaneously using FastLED's standard API.
///
/// PARLIO provides hardware-accelerated parallel output with precise timing,
/// allowing you to control 1-16 LED strips with minimal CPU overhead.
///
/// Hardware Requirements:
/// - ESP32-C6, ESP32-P4, ESP32-H2, or ESP32-C5 (chips with PARLIO peripheral)
/// - WS2812/WS2812B LED strips (up to 16 parallel strips)
/// - Adequate power supply for your LEDs (5V, ~60mA per LED at full white)
///
/// Pin Selection Guidelines:
/// - PARLIO validates pins using FastLED's pin validation system
/// - Avoid pins used for: SPI flash, strapping, USB-JTAG, etc.
/// - See FASTLED_UNUSABLE_PIN_MASK in src/platforms/esp/32/core/fastpin_esp32.h
/// - If you use an invalid pin, PARLIO will reject it with a clear error message
///
/// Pin Restrictions by Platform:
///
/// ESP32-C6:
///   - Avoid: GPIO 24-26, 28-30 (SPI Flash)
///   - Safe pins: 0-3, 6-7, 10, 14, 16-23
///
/// ESP32-P4:
///   - Avoid: GPIO 24-25 (USB)
///   - Most other pins are safe (55 GPIO pins available)
///
/// ESP32-H2:
///   - Avoid: GPIO 15-21 (SPI Flash)
///   - Safe pins: 0-14, 22+
///
/// ESP32-C5:
///   - Avoid: GPIO 13-18, 20-22 (SPI Flash/USB)
///   - Safe pins: 0-12, 19, 23+
///
/// Performance:
/// - 60+ FPS for typical LED counts (<500 LEDs per strip)
/// - Near-zero CPU usage during transmission (hardware DMA)
/// - Automatic chunking for large LED counts
///
/// Version Requirements:
/// - ESP32-C6: Requires ESP-IDF 5.5.0+ (earlier versions have PARLIO driver bug)

#include "FastLED.h"

// Configuration
#define NUM_STRIPS 8
#define NUM_LEDS_PER_STRIP 100
#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUM_STRIPS)

// Pin definitions - Platform-specific safe pins
// These pins are carefully chosen to avoid forbidden pins on each platform

#if defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32-C6: Safe pins avoiding strapping (4,5,8,9,15), flash (24-30), USB-JTAG (12-13)
    #define PIN0 6
    #define PIN1 7
    #define PIN2 16
    #define PIN3 17
    #define PIN4 18
    #define PIN5 19
    #define PIN6 20
    #define PIN7 21
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    // ESP32-P4: Most pins are safe, avoid USB (24-25)
    #define PIN0 1
    #define PIN1 2
    #define PIN2 3
    #define PIN3 4
    #define PIN4 5
    #define PIN5 6
    #define PIN6 7
    #define PIN7 8
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-H2: Safe pins avoiding SPI flash (15-21)
    #define PIN0 0
    #define PIN1 1
    #define PIN2 2
    #define PIN3 3
    #define PIN4 4
    #define PIN5 5
    #define PIN6 10
    #define PIN7 11
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
    // ESP32-C5: Safe pins avoiding USB (13-14), flash (15-18, 20-22)
    #define PIN0 0
    #define PIN1 1
    #define PIN2 2
    #define PIN3 3
    #define PIN4 4
    #define PIN5 5
    #define PIN6 6
    #define PIN7 7
#else
    #error "This example requires ESP32-C6, ESP32-P4, ESP32-H2, or ESP32-C5 with PARLIO peripheral"
#endif

// LED buffer - interleaved layout for 8 strips
CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000); // Allow time for serial connection

    Serial.println("\n=== ESP32 PARLIO Parallel LED Demo ===");
    Serial.print("Platform: ");
    #if defined(CONFIG_IDF_TARGET_ESP32C6)
        Serial.println("ESP32-C6");
    #elif defined(CONFIG_IDF_TARGET_ESP32P4)
        Serial.println("ESP32-P4");
    #elif defined(CONFIG_IDF_TARGET_ESP32H2)
        Serial.println("ESP32-H2");
    #elif defined(CONFIG_IDF_TARGET_ESP32C5)
        Serial.println("ESP32-C5");
    #endif

    Serial.print("Strips: ");
    Serial.println(NUM_STRIPS);
    Serial.print("LEDs per strip: ");
    Serial.println(NUM_LEDS_PER_STRIP);
    Serial.println("Initializing PARLIO driver...");

    // Add LED strips - PARLIO automatically activates for parallel strips
    // Each strip gets its own section of the LED buffer
    FastLED.addLeds<WS2812, PIN0, GRB>(leds + (0 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN1, GRB>(leds + (1 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN2, GRB>(leds + (2 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN3, GRB>(leds + (3 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN4, GRB>(leds + (4 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN5, GRB>(leds + (5 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN6, GRB>(leds + (6 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN7, GRB>(leds + (7 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);

    FastLED.setBrightness(64);  // Start at 25% brightness

    Serial.println("PARLIO initialized successfully!");
    Serial.println("GPIO pin assignments:");
    Serial.print("  Strip 0: GPIO "); Serial.println(PIN0);
    Serial.print("  Strip 1: GPIO "); Serial.println(PIN1);
    Serial.print("  Strip 2: GPIO "); Serial.println(PIN2);
    Serial.print("  Strip 3: GPIO "); Serial.println(PIN3);
    Serial.print("  Strip 4: GPIO "); Serial.println(PIN4);
    Serial.print("  Strip 5: GPIO "); Serial.println(PIN5);
    Serial.print("  Strip 6: GPIO "); Serial.println(PIN6);
    Serial.print("  Strip 7: GPIO "); Serial.println(PIN7);
    Serial.println("\nStarting animation...");
}

uint8_t hue = 0;

void loop() {
    // Simple rainbow animation across all strips
    // Each strip gets a different starting hue
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        fill_rainbow(
            leds + (strip * NUM_LEDS_PER_STRIP),  // Start of this strip's data
            NUM_LEDS_PER_STRIP,                    // Number of LEDs in strip
            hue + (strip * 32),                    // Starting hue offset per strip
            7                                       // Hue delta between LEDs
        );
    }

    FastLED.show();
    hue++;

    // Optional: Print FPS every 100 frames
    EVERY_N_MILLISECONDS(1000) {
        Serial.print("FPS: ");
        Serial.println(FastLED.getFPS());
    }
}
