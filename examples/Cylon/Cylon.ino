/// @file    Cylon.ino
/// @brief   An animation that moves a single LED back and forth (Larson Scanner effect) using the fx library cylon
/// @example Cylon.ino

#include <FastLED.h>
#include "fx/cylon.hpp"

// How many leds in your strip?
#define NUM_LEDS 64 

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power).
#define DATA_PIN 2

// Define the array of leds
CRGB leds[NUM_LEDS];

// Create a CylonData instance
CylonData cylonData(leds, NUM_LEDS);

void setup() {
    FastLED.addLeds<WS2812,DATA_PIN,RGB>(leds,NUM_LEDS).setRgbw();
    FastLED.setBrightness(84);
}

void loop() { 
    CylonLoop(cylonData);
    FastLED.show();
    delay(cylonData.delay_ms);
}
