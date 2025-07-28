/// @file    OctoWS2811Demo.ino
/// @brief   Demonstrates how to use OctoWS2811 output
/// @example OctoWS2811Demo.ino

// Only compile this example on Teensy platforms with OctoWS2811 support
#if defined(__arm__) && defined(TEENSYDUINO)

#define USE_OCTOWS2811
#include <OctoWS2811.h>
#include <FastLED.h>

#define NUM_LEDS_PER_STRIP 64
#define NUM_STRIPS 8

CRGB leds[NUM_STRIPS * NUM_LEDS_PER_STRIP];

// Pin layouts on the teensy 3:
// OctoWS2811: 2,14,7,8,6,20,21,5

void setup() {
  FastLED.addLeds<OCTOWS2811>(leds, NUM_LEDS_PER_STRIP);
  FastLED.setBrightness(32);
}

void loop() {
  static uint8_t hue = 0;
  for(int i = 0; i < NUM_STRIPS; i++) {
    for(int j = 0; j < NUM_LEDS_PER_STRIP; j++) {
      leds[(i*NUM_LEDS_PER_STRIP) + j] = CHSV((32*i) + hue+j,192,255);
    }
  }

  // Set the first n leds on each strip to show which strip it is
  for(int i = 0; i < NUM_STRIPS; i++) {
    for(int j = 0; j <= i; j++) {
      leds[(i*NUM_LEDS_PER_STRIP) + j] = CRGB::Red;
    }
  }

  hue++;

  FastLED.show();
  FastLED.delay(10);
}

#else
// Fallback for non-Teensy platforms
#include <FastLED.h>

void setup() {
  // This example requires Teensy with OctoWS2811 library
  // No hardware initialization on non-Teensy platforms
}

void loop() {
  // No-op on non-Teensy platforms
  delay(1000);
}

#endif // defined(__arm__) && defined(TEENSYDUINO)
