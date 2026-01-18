// Simple Adafruit Bridge Demo
//
// This example shows how to use the Adafruit_NeoPixel library with FastLED.
// As long as the Adafruit_NeoPixel library is installed (that is #include <Adafruit_NeoPixel.h>
// is present), this example will work. Otherwise you'll get warnings about a missing driver.

#define FASTLED_USE_ADAFRUIT_NEOPIXEL
#include "FastLED.h"

#define DATA_PIN 3
#define NUM_LEDS 10

CRGB leds[NUM_LEDS];
uint8_t hue = 0;

void setup() {
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
}

void loop() {
  fill_rainbow(leds, NUM_LEDS, hue, 255/NUM_LEDS);
  FastLED.show();
  
  hue++;
  delay(50);
}
