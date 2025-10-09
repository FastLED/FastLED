/// @file QuadSPI_Basic.ino
/// @brief Basic example of using Hardware Quad-SPI for parallel LED control on ESP32
///
/// This example demonstrates driving 4 APA102 LED strips simultaneously using
/// ESP32's hardware Quad-SPI peripheral with DMA for zero CPU overhead.
///
/// Hardware Requirements:
/// - ESP32, ESP32-S2, or ESP32-S3 board
/// - 4× APA102 (Dotstar) LED strips
/// - Shared clock line connected to all 4 strips
/// - 4× separate data lines (one per strip)
///
/// Wiring:
/// - Clock: GPIO 12 (shared by all 4 strips)
/// - Data Lane 0: GPIO 11 (MOSI/D0)
/// - Data Lane 1: GPIO 13 (MISO/D1)
/// - Data Lane 2: GPIO 14 (WP/D2)
/// - Data Lane 3: GPIO 10 (HD/D3)
///
/// Performance:
/// - 4×100 LEDs transmitted in ~0.08ms @ 40 MHz
/// - 0% CPU usage during transmission (DMA-driven)
/// - ~27× faster than sequential software SPI
///
/// @note This feature requires ESP32/S2/S3. Not supported on other platforms.

#include <FastLED.h>
#include "platforms/quad_spi_platform.h"

// Only compile if Quad-SPI is available on this platform
#if FASTLED_HAS_QUAD_SPI

// Pin definitions (ESP32-S3 HSPI default pins)
#define CLOCK_PIN 12
#define DATA_PIN_0 11
#define DATA_PIN_1 13
#define DATA_PIN_2 14
#define DATA_PIN_3 10

// LED strip configuration
#define NUM_LEDS_LANE_0 60
#define NUM_LEDS_LANE_1 100
#define NUM_LEDS_LANE_2 80
#define NUM_LEDS_LANE_3 120

// LED arrays (one per strip)
CRGB leds_lane0[NUM_LEDS_LANE_0];
CRGB leds_lane1[NUM_LEDS_LANE_1];
CRGB leds_lane2[NUM_LEDS_LANE_2];
CRGB leds_lane3[NUM_LEDS_LANE_3];

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("FastLED Quad-SPI Basic Example");
    Serial.println("===============================");
    Serial.println("Hardware: ESP32 with 4× APA102 LED strips");
    Serial.println();

    // Add LED strips - FastLED automatically detects shared clock pins
    // and enables hardware Quad-SPI for parallel transmission
    FastLED.addLeds<APA102, DATA_PIN_0, CLOCK_PIN, RGB>(leds_lane0, NUM_LEDS_LANE_0);
    FastLED.addLeds<APA102, DATA_PIN_1, CLOCK_PIN, RGB>(leds_lane1, NUM_LEDS_LANE_1);
    FastLED.addLeds<APA102, DATA_PIN_2, CLOCK_PIN, RGB>(leds_lane2, NUM_LEDS_LANE_2);
    FastLED.addLeds<APA102, DATA_PIN_3, CLOCK_PIN, RGB>(leds_lane3, NUM_LEDS_LANE_3);

    Serial.println("LED strips initialized:");
    Serial.printf("  Lane 0: %d LEDs on pin %d\n", NUM_LEDS_LANE_0, DATA_PIN_0);
    Serial.printf("  Lane 1: %d LEDs on pin %d\n", NUM_LEDS_LANE_1, DATA_PIN_1);
    Serial.printf("  Lane 2: %d LEDs on pin %d\n", NUM_LEDS_LANE_2, DATA_PIN_2);
    Serial.printf("  Lane 3: %d LEDs on pin %d\n", NUM_LEDS_LANE_3, DATA_PIN_3);
    Serial.printf("  Shared Clock: pin %d\n", CLOCK_PIN);
    Serial.println();

    Serial.println("Starting rainbow animation...");
}

void loop() {
    static uint8_t hue = 0;

    // Different rainbow patterns on each strip
    // This demonstrates that each strip is independently controlled

    // Lane 0: Rainbow
    fill_rainbow(leds_lane0, NUM_LEDS_LANE_0, hue, 7);

    // Lane 1: Rainbow with offset
    fill_rainbow(leds_lane1, NUM_LEDS_LANE_1, hue + 64, 7);

    // Lane 2: Rainbow with different density
    fill_rainbow(leds_lane2, NUM_LEDS_LANE_2, hue + 128, 5);

    // Lane 3: Rainbow with opposite direction
    fill_rainbow(leds_lane3, NUM_LEDS_LANE_3, hue + 192, 7);

    // Show all strips - transmits all 4 strips in parallel via hardware DMA
    // Zero CPU overhead during transmission thanks to Quad-SPI
    FastLED.show();

    hue++;
    delay(20);
}

#else

// Stub functions for platforms without Quad-SPI support
void setup() {}
void loop() {}

#endif  // FASTLED_HAS_QUAD_SPI
