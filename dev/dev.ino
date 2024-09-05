/// @file    HSVDemo.ino
/// @brief   Cycle through hues on an LED strip using HSV color model
/// @example HSVDemo.ino

#define FASTLED_RMT_BUILTIN_DRIVER 0
#define FASTLED_EXPERIMENTAL_ESP32_RGBW_ENABLED 0
#define FASTLED_EXPERIMENTAL_ESP32_RGBW_MODE kRGBWExactColors

#include <FastLED.h>
#include "noise.h"

// How many leds in your strip?
#define NUM_LEDS 10

// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define DATA_PIN 9

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is assumed
    delay(2000);  // If something ever goes wrong this delay will allow upload.
}

void loop() { 
  uint16_t hue = inoise16(millis() * 80, 0, 0) >> 8;
  uint16_t sat = inoise16(millis() * 40, 0, 0) >> 8;
  uint16_t val = inoise16(millis() * 20, 0, 0) >> 8;

  // Fill the LED array with the current hue
  fill_solid(leds, NUM_LEDS, CHSV(hue, sat, val));
  FastLED.show();

  // Small delay to control the speed of the color change
  delay(1);
}
