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
Cylon cylon(NUM_LEDS);

void setup() { 
    Serial.begin(57600);
    Serial.println("resetting");
    FastLED.addLeds<WS2812,DATA_PIN,BRG>(leds,NUM_LEDS).setRgbw();
    FastLED.setBrightness(84);
    cylon.lazyInit();
}

void loop() { 
    Serial.print("x");
    cylon.draw(millis(), leds);
    FastLED.show();
    delay(cylon.delay_ms);
}
