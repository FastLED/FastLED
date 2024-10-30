/// @file    Cylon.ino
/// @brief   An animation that moves a single LED back and forth (Larson Scanner effect) using the fx library cylon
/// @example Cylon.ino

#include <FastLED.h>
#include "fx/1d/cylon.hpp"

// How many leds in your strip?
#define NUM_LEDS 64 

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power).
#define DATA_PIN 2

// Define the array of leds
CRGB leds[NUM_LEDS];

// Create a Cylon instance
CylonRef cylon = CylonRef::New(NUM_LEDS);

void setup() { 
    FastLED.addLeds<WS2812,DATA_PIN,RGB>(leds,NUM_LEDS).setRgbw();
    FastLED.setBrightness(84);
}

void loop() { 
    cylon->draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
    delay(cylon->delay_ms);
}
