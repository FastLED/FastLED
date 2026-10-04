/// @file    MultipleStripsInOneArray.ino
/// @brief   Demonstrates how to use multiple LED strips, each with their own data in one shared array
/// @example MultipleStripsInOneArray.ino

// @filter: (board is not ATtiny1604)

// MultipleStripsInOneArray - see https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples for more info on
// using multiple controllers.  In this example, we're going to set up three NEOPIXEL strips on three
// different pins, each strip will be referring to a different part of the single led array
//
// The segments are equal-sized here only to keep the arithmetic readable.  Each
// addLeds() call takes its own offset and its own length, so unequal segments
// work just as well -- see MultiArrays.ino for a mixed-length example.

#include <FastLED.h>

#define NUM_STRIPS 3
#if FL_PLATFORM_HAS_TINY_MEMORY
// Keep three independently addressed segments within 512 B SRAM.
#define NUM_LEDS_PER_STRIP 4
#else
#define NUM_LEDS_PER_STRIP 60
#endif
#define NUM_LEDS NUM_LEDS_PER_STRIP * NUM_STRIPS

CRGB leds[NUM_STRIPS * NUM_LEDS_PER_STRIP];

// For mirroring strips, all the "special" stuff happens just in setup.  We
// just addLeds multiple times, once for each strip
void setup() {
  // Add the first strip at index 0.
  FastLED.addLeds<NEOPIXEL, 2>(leds, 0, NUM_LEDS_PER_STRIP);

  // Add the second strip after the first segment.
  FastLED.addLeds<NEOPIXEL, 3>(leds, NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP);

  // Add the third strip after the second segment.
  FastLED.addLeds<NEOPIXEL, 4>(leds, 2 * NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP);

}

void loop() {
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Red;
    FastLED.show();
    leds[i] = CRGB::Black;
    delay(100);
  }
}
