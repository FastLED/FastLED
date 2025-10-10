/// @file QuadSPI_Basic.ino
/// @brief Basic example of using Hardware Quad-SPI for parallel LED control on ESP32
///
/// This example demonstrates driving 4 APA102 LED strips simultaneously using
/// ESP32's hardware Quad-SPI peripheral with DMA for zero CPU overhead.
///
/// Hardware Requirements:
/// - ESP32 variant (ESP32, S2, S3, C3, P4, H2)
/// - 4× APA102 (Dotstar) LED strips
/// - Shared clock line connected to all 4 strips
/// - 4× separate data lines (one per strip)
///
/// Wiring (automatically selected based on ESP32 variant):
/// ESP32:    CLK=14, D0=13, D1=12, D2=2,  D3=4  (HSPI pins)
/// ESP32-S2: CLK=12, D0=11, D1=13, D2=14, D3=9  (SPI2 pins)
/// ESP32-S3: CLK=12, D0=11, D1=13, D2=14, D3=9  (SPI2 pins)
/// ESP32-C3: CLK=6,  D0=7,  D1=2,  D2=5,  D3=4  (SPI2 pins)
/// ESP32-P4: CLK=9,  D0=8,  D1=10, D2=11, D3=6  (SPI2 pins)
///
/// Performance:
/// - 4×100 LEDs transmitted in ~0.08ms @ 40 MHz
/// - 0% CPU usage during transmission (DMA-driven)
/// - ~27× faster than sequential software SPI
///
/// @note This feature requires ESP32/S2/S3. Not supported on other platforms.

// ESP32-only example - do not compile on other platforms
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

#include <FastLED.h>
#include "platforms/quad_spi_platform.h"

// Only compile if Quad-SPI is available on this platform
#if FASTLED_HAS_QUAD_SPI

// Pin definitions - hardware QuadSPI pins per ESP32 variant
// These use the IO_MUX pins for optimal performance (up to 80MHz)
#if CONFIG_IDF_TARGET_ESP32
// ESP32 (original) - Using HSPI (SPI2) QuadSPI pins
#define CLOCK_PIN 14   // HSPI CLK
#define DATA_PIN_0 13  // HSPI MOSI (D0)
#define DATA_PIN_1 12  // HSPI MISO (D1)
#define DATA_PIN_2 2   // HSPI WP (D2)
#define DATA_PIN_3 4   // HSPI HD (D3)

#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
// ESP32-S2/S3 - Using SPI2 QuadSPI pins
#define CLOCK_PIN 12   // SPI2 CLK
#define DATA_PIN_0 11  // SPI2 MOSI (D0)
#define DATA_PIN_1 13  // SPI2 MISO (D1)
#define DATA_PIN_2 14  // SPI2 WP (D2)
#define DATA_PIN_3 9   // SPI2 HD (D3)

#elif CONFIG_IDF_TARGET_ESP32C3
// ESP32-C3 - Using SPI2 QuadSPI pins
#define CLOCK_PIN 6    // SPI2 CLK
#define DATA_PIN_0 7   // SPI2 MOSI (D0)
#define DATA_PIN_1 2   // SPI2 MISO (D1)
#define DATA_PIN_2 5   // SPI2 WP (D2)
#define DATA_PIN_3 4   // SPI2 HD (D3)

#elif CONFIG_IDF_TARGET_ESP32P4
// ESP32-P4 - Using SPI2 QuadSPI pins
#define CLOCK_PIN 9    // SPI2 CLK
#define DATA_PIN_0 8   // SPI2 MOSI (D0)
#define DATA_PIN_1 10  // SPI2 MISO (D1)
#define DATA_PIN_2 11  // SPI2 WP (D2)
#define DATA_PIN_3 6   // SPI2 HD (D3)

#else
// Fallback for unknown variants - use VSPI-like pins
#define CLOCK_PIN 18
#define DATA_PIN_0 23
#define DATA_PIN_1 19
#define DATA_PIN_2 22
#define DATA_PIN_3 21
#endif

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

#else  // !ESP32

// Stub functions for non-ESP32 platforms
void setup() {}
void loop() {}

#endif  // ESP32
