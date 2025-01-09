
/// @file    Blink.ino
/// @brief   Blink the first LED of an LED strip
/// @example Blink.ino

#include <FastLED.h>

// How many leds in your strip?
#define NUM_LEDS 256

// Demo of driving multiple WS2812 strips on different pins

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() { 
    //FastLED.addLeds<WS2812, 5>(leds, NUM_LEDS);  // GRB ordering is assumed
    FastLED.addLeds<WS2812, 4>(leds, NUM_LEDS);  // GRB ordering is assumed
    FastLED.addLeds<WS2812, 3>(leds, NUM_LEDS);  // GRB ordering is assumed
    FastLED.addLeds<WS2812, 6>(leds, NUM_LEDS);  // GRB ordering is assumed
    delay(1000);
}

void fill(CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
}

void loop() { 
  // Turn the LED on, then pause
  fill(CRGB(8,0,0));
  FastLED.show();
  delay(500);
  // Now turn the LED off, then pause
  fill(CRGB::Black);
  FastLED.show();
  delay(500);
}