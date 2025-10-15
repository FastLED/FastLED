/// @file quad_spi_basic.cpp
/// @brief Implementation of QuadSPI Basic example
///
/// NOTE: This code is separated into .h/.cpp files to work around PlatformIO's
/// Library Dependency Finder (LDF). The LDF aggressively scans .ino files and
/// can pull in unwanted dependencies (like TinyUSB audio code), but it does NOT
/// traverse into .cpp files the same way. By keeping the implementation in .cpp,
/// we avoid LDF triggering malloc dependencies on platforms like NRF52.

#include <Arduino.h>
#include <FastLED.h>

// ESP32-only example - do not compile on other platforms
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

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

#elif CONFIG_IDF_TARGET_ESP32H2
// ESP32-H2 - Using SPI2 QuadSPI pins
#define CLOCK_PIN 4    // SPI2 CLK
#define DATA_PIN_0 5   // SPI2 MOSI (D0)
#define DATA_PIN_1 0   // SPI2 MISO (D1)
#define DATA_PIN_2 2   // SPI2 WP (D2)
#define DATA_PIN_3 3   // SPI2 HD (D3)

#elif CONFIG_IDF_TARGET_ESP32C5
// ESP32-C5 - Using safe GPIO pins (avoid flash pins 15-22, USB pins 13-14)
#define CLOCK_PIN 12   // Safe GPIO
#define DATA_PIN_0 11  // Safe GPIO (D0)
#define DATA_PIN_1 5   // Safe GPIO (D1)
#define DATA_PIN_2 4   // Safe GPIO (D2)
#define DATA_PIN_3 3   // Safe GPIO (D3)

#elif CONFIG_IDF_TARGET_ESP32C6
// ESP32-C6 - Using SPI2 QuadSPI IO_MUX pins (optimal performance)
#define CLOCK_PIN 6    // SPI2 CLK (FSPICLK)
#define DATA_PIN_0 7   // SPI2 MOSI (FSPID/D0)
#define DATA_PIN_1 2   // SPI2 MISO (FSPIQ/D1)
#define DATA_PIN_2 5   // SPI2 WP (FSPIWP/D2)
#define DATA_PIN_3 4   // SPI2 HD (FSPIHD/D3)

#elif CONFIG_IDF_TARGET_ESP32C2
// ESP32-C2 - Using safe GPIO pins (only GPIO 0-20 available, avoid flash pins 11-17)
#define CLOCK_PIN 10   // Safe GPIO
#define DATA_PIN_0 0   // Safe GPIO (D0)
#define DATA_PIN_1 1   // Safe GPIO (D1)
#define DATA_PIN_2 2   // Safe GPIO (D2)
#define DATA_PIN_3 3   // Safe GPIO (D3)

#else
// Fallback for unknown variants - use safe pins that avoid common issues
// Avoid pins 18, 21, 22 which may conflict with flash on some variants
#define CLOCK_PIN 14
#define DATA_PIN_0 13
#define DATA_PIN_1 12
#define DATA_PIN_2 27
#define DATA_PIN_3 26
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

void quad_spi_basic_setup() {
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
    Serial.print("  Lane 0: "); Serial.print(NUM_LEDS_LANE_0); Serial.print(" LEDs on pin "); Serial.println(DATA_PIN_0);
    Serial.print("  Lane 1: "); Serial.print(NUM_LEDS_LANE_1); Serial.print(" LEDs on pin "); Serial.println(DATA_PIN_1);
    Serial.print("  Lane 2: "); Serial.print(NUM_LEDS_LANE_2); Serial.print(" LEDs on pin "); Serial.println(DATA_PIN_2);
    Serial.print("  Lane 3: "); Serial.print(NUM_LEDS_LANE_3); Serial.print(" LEDs on pin "); Serial.println(DATA_PIN_3);
    Serial.print("  Shared Clock: pin "); Serial.println(CLOCK_PIN);
    Serial.println();

    Serial.println("Starting rainbow animation...");
}

void quad_spi_basic_loop() {
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

#else  // !ESP32

// Stub functions for non-ESP32 platforms
void quad_spi_basic_setup() {}
void quad_spi_basic_loop() {}

#endif  // ESP32
