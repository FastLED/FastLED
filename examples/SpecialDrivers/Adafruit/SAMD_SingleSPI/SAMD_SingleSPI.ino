/// @file    SAMD_SingleSPI.ino
/// @brief   SAMD21/SAMD51 Single-SPI LED Demo
/// @example SAMD_SingleSPI.ino
///
/// This example demonstrates using FastLED with SPI-based LED chipsets on
/// SAMD21 (Cortex-M0+) and SAMD51 (Cortex-M4F) platforms.
///
/// Supported Boards:
/// - Arduino Zero (SAMD21)
/// - Adafruit Feather M0 (SAMD21)
/// - Adafruit Feather M4 (SAMD51)
/// - Adafruit Grand Central M4 (SAMD51)
///
/// Supported LED Chipsets (SPI-based):
/// - APA102 (DotStar)
/// - SK9822
/// - LPD8806
/// - WS2801
/// - P9813
///
/// Hardware Connections:
/// - Connect LED strip DATA to the board's MOSI pin
/// - Connect LED strip CLOCK to the board's SCK pin
/// - Connect LED strip GND to board GND
/// - Connect LED strip VCC to appropriate power supply
///
/// Pin Mappings (Hardware SPI):
///
/// Arduino Zero:
///   MOSI (DATA): Pin 11 (SERCOM1)
///   SCK (CLOCK): Pin 13 (SERCOM1)
///
/// Adafruit Feather M0:
///   MOSI (DATA): Pin 23 (SERCOM4)
///   SCK (CLOCK): Pin 24 (SERCOM4)
///
/// Adafruit Feather M4:
///   MOSI (DATA): Pin 23 (SERCOM1)
///   SCK (CLOCK): Pin 24 (SERCOM1)
///
/// Adafruit Grand Central M4:
///   MOSI (DATA): Pin 51 (SERCOM2)
///   SCK (CLOCK): Pin 52 (SERCOM2)
///
/// Note: The SAMD parallel SPI implementation from Iterations 1-10 provides
/// infrastructure for dual-lane and quad-lane operation, but requires bus
/// manager integration for user-facing API. This example uses standard
/// single-SPI mode which works with the existing FastLED API.

#include <Arduino.h>
#include <FastLED.h>

// LED strip configuration
#define NUM_LEDS 30

// Board-specific pin configuration
// These pins use hardware SPI (SERCOM peripheral)
#if defined(__SAMD51__)  // Feather M4 or Grand Central M4
  #if defined(ADAFRUIT_GRAND_CENTRAL_M4)
    #define DATA_PIN 51   // MOSI on Grand Central M4 (ICSP header)
    #define CLOCK_PIN 52  // SCK on Grand Central M4 (ICSP header)
  #else  // Feather M4 or other SAMD51
    #define DATA_PIN 23   // MOSI on Feather M4
    #define CLOCK_PIN 24  // SCK on Feather M4
  #endif
#elif defined(__SAMD21__)  // Arduino Zero or Feather M0
  #if defined(ADAFRUIT_FEATHER_M0)
    #define DATA_PIN 23   // MOSI on Feather M0
    #define CLOCK_PIN 24  // SCK on Feather M0
  #else  // Arduino Zero or other SAMD21
    #define DATA_PIN 11   // MOSI on Arduino Zero (ICSP header)
    #define CLOCK_PIN 13  // SCK on Arduino Zero (ICSP header)
  #endif
#else
  // Fallback for unknown SAMD boards
  #define DATA_PIN 11
  #define CLOCK_PIN 13
  #warning "Unknown SAMD board - using default pins (DATA=11, CLOCK=13)"
#endif

// Define the LED array
CRGB leds[NUM_LEDS];

// Animation variables
uint8_t hue = 0;
uint8_t brightness = 50;

void setup() {
  // Initialize serial for debugging (optional)
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    // Wait up to 3 seconds for serial connection
  }

  Serial.println("FastLED SAMD Single-SPI Demo");
  Serial.print("Board: ");
  #if defined(__SAMD51__)
    Serial.print("SAMD51 (Cortex-M4F @ 120 MHz)");
  #elif defined(__SAMD21__)
    Serial.print("SAMD21 (Cortex-M0+ @ 48 MHz)");
  #else
    Serial.print("Unknown SAMD");
  #endif
  Serial.println();

  Serial.print("DATA Pin (MOSI): ");
  Serial.println(DATA_PIN);
  Serial.print("CLOCK Pin (SCK): ");
  Serial.println(CLOCK_PIN);
  Serial.println();

  // Small delay for power stabilization
  delay(500);

  // Initialize FastLED with SPI-based LED chipset
  // Uncomment the line for your LED chipset:

  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);  // DotStar (BGR ordering typical)
  // FastLED.addLeds<SK9822, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);  // SK9822 (BGR ordering typical)
  // FastLED.addLeds<LPD8806, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // LPD8806 (RGB ordering typical)
  // FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);  // WS2801 (RGB ordering typical)
  // FastLED.addLeds<P9813, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);   // P9813 (RGB ordering typical)

  FastLED.setBrightness(brightness);

  Serial.println("FastLED initialized successfully!");
  Serial.println("Running rainbow animation...");
}

void loop() {
  // Rainbow chase animation
  fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
  FastLED.show();

  hue++;
  delay(20);

  // Optional: Print frame rate every 2 seconds
  static unsigned long lastPrint = 0;
  static unsigned int frameCount = 0;
  frameCount++;

  unsigned long now = millis();
  if (now - lastPrint >= 2000) {
    float fps = frameCount / ((now - lastPrint) / 1000.0);
    Serial.print("FPS: ");
    Serial.println(fps);
    frameCount = 0;
    lastPrint = now;
  }
}
