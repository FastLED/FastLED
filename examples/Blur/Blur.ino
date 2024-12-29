// UIDescription: This example shows how to blur a strip of LEDs. It uses the blur1d function to blur the strip and fadeToBlackBy to dim the strip. A bright pixel moves along the strip.
// Author: Zach Vorhies

#include <FastLED.h>

#define NUM_LEDS 64
#define DATA_PIN 3  // Change this to match your LED strip's data pin
#define BRIGHTNESS 255

CRGB leds[NUM_LEDS];
uint8_t pos = 0;
bool toggle = false;

void setup() {
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  // Add a bright pixel that moves
  leds[pos] = CHSV(pos * 2, 255, 255);
  // Blur the entire strip
  blur1d(leds, NUM_LEDS, 172);
  fadeToBlackBy(leds, NUM_LEDS, 16);
  FastLED.show();
  // Move the position of the dot
  if (toggle) {
    pos = (pos + 1) % NUM_LEDS;
  }
  toggle = !toggle;
  delay(20);
}
