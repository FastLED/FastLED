/// @file    MirroringSample.ino
/// @brief   Demonstrates how to use multiple LED strips, each with the same data
/// @example MirroringSample.ino

#include "fl/sketch_macros.h"

#if !SKETCH_HAS_LOTS_OF_MEMORY
#include "platforms/sketch_fake.hpp"
#else

// MirroringSample - see https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples for more info on
// using multiple controllers.  In this example, we're going to set up four NEOPIXEL strips on four
// different pins, and show the same thing on all four of them, a simple bouncing dot/cyclon type pattern

#include "fl/sketch_macros.h"

#if !SKETCH_HAS_LOTS_OF_MEMORY
#include "platforms/sketch_fake.hpp"
#else

#include <FastLED.h>

#define NUM_LEDS_PER_STRIP 60
CRGB leds[NUM_LEDS_PER_STRIP];

// For mirroring strips, all the "special" stuff happens just in setup.  We
// just addLeds multiple times, once for each strip
void setup() {
  // tell FastLED there's 60 NEOPIXEL leds on pin 4
  FastLED.addLeds<NEOPIXEL, 4>(leds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin 5
  FastLED.addLeds<NEOPIXEL, 5>(leds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin 12
  FastLED.addLeds<NEOPIXEL, 12>(leds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin 13
  FastLED.addLeds<NEOPIXEL, 13>(leds, NUM_LEDS_PER_STRIP);
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

#endif // SKETCH_HAS_LOTS_OF_MEMORY
