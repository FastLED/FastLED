// @filter: (memory is high)

/// @file    MirroringSample.ino
/// @brief   Demonstrates how to use multiple LED strips, each with the same data
/// @example MirroringSample.ino

// MirroringSample - see https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples for more info on
// using multiple controllers.  In this example, we're going to set up four NEOPIXEL strips on four
// different pins, and show the same thing on all four of them, a simple bouncing dot/cyclon type pattern
//
// NOTE: This example uses platform-specific pin definitions to automatically select safe pins
// for your board. Finding truly "universal" safe pins across all platforms is challenging due
// to different architectures, boot requirements, and hardware constraints (strapping pins,
// flash interfaces, USB, etc.). The platform detection below handles these differences automatically.

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include <FastLED.h>

#define NUM_LEDS_PER_STRIP 60
CRGB leds[NUM_LEDS_PER_STRIP];

// Platform-specific safe pin definitions
// These pins are chosen to avoid strapping pins, flash pins, USB pins, etc.
#if defined(ESP32)
  // ESP32 variants - use universally safe pins that work across most ESP32 modules
  #if defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-C3: Avoid pin 9 (strapping), use safe GPIO
    #define PIN_1  2
    #define PIN_2  3
    #define PIN_3  4
    #define PIN_4  5
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S3: Use safe pins that avoid USB-JTAG (19, 20) and flash/PSRAM (26-32)
    #define PIN_1  1
    #define PIN_2  2
    #define PIN_3  3
    #define PIN_4  4
  #elif defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32-C6: Avoid strapping pins 8, 9 and USB pins 12, 13
    #define PIN_1  2
    #define PIN_2  3
    #define PIN_3  4
    #define PIN_4  5
  #elif defined(CONFIG_IDF_TARGET_ESP32C5)
    // ESP32-C5: Avoid strapping pins 2, 7, 25, 27, 28, USB pins 13, 14, and flash pins 16-22
    #define PIN_1  3
    #define PIN_2  4
    #define PIN_3  5
    #define PIN_4  8
  #elif defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-H2: Avoid flash pins 15-21, strapping pins 2, 3, 8, 9, 25, USB-Serial-JTAG 26, 27
    #define PIN_1  0
    #define PIN_2  1
    #define PIN_3  4
    #define PIN_4  5
  #elif defined(CONFIG_IDF_TARGET_ESP32C2)
    // ESP32-C2: Only GPIO 0-20 available. Avoid flash pins 11-17, strapping pins 8, 9
    #define PIN_1  0
    #define PIN_2  1
    #define PIN_3  2
    #define PIN_4  3
  #else
    // ESP32 classic (WROOM-32, WROVER, etc): Avoid strapping and flash pins
    #define PIN_1  13
    #define PIN_2  14
    #define PIN_3  15
    #define PIN_4  16
  #endif
#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040 (Raspberry Pi Pico): Most GPIO are safe, using mid-range pins
  #define PIN_1  10
  #define PIN_2  11
  #define PIN_3  12
  #define PIN_4  13
#elif defined(__IMXRT1062__)
  // Teensy 4.0/4.1: Most pins are safe, using common GPIO
  #define PIN_1  2
  #define PIN_2  3
  #define PIN_3  4
  #define PIN_4  5
#elif defined(__MK66FX1M0__) || defined(__MK64FX512__)
  // Teensy 3.6: Safe pins away from Serial
  #define PIN_1  2
  #define PIN_2  3
  #define PIN_3  4
  #define PIN_4  5
#else
  // Universal fallback for other platforms (AVR, etc.)
  // These are common digital pins on most Arduino boards
  #define PIN_1  4
  #define PIN_2  5
  #define PIN_3  6
  #define PIN_4  7
#endif

// For mirroring strips, all the "special" stuff happens just in setup.  We
// just addLeds multiple times, once for each strip
void setup() {
  // tell FastLED there's 60 NEOPIXEL leds on pin PIN_1
  FastLED.addLeds<NEOPIXEL, PIN_1>(leds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin PIN_2
  FastLED.addLeds<NEOPIXEL, PIN_2>(leds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin PIN_3
  FastLED.addLeds<NEOPIXEL, PIN_3>(leds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin PIN_4
  FastLED.addLeds<NEOPIXEL, PIN_4>(leds, NUM_LEDS_PER_STRIP);
}

void loop() {
  for(int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
    // set our current dot to red
    leds[i] = CRGB::Red;
    FastLED.show();
    // clear our current dot before we move on
    leds[i] = CRGB::Black;
    delay(100);
  }

  for(int i = NUM_LEDS_PER_STRIP-1; i >= 0; i--) {
    // set our current dot to red
    leds[i] = CRGB::Red;
    FastLED.show();
    // clear our current dot before we move on
    leds[i] = CRGB::Black;
    delay(100);
  }
}
