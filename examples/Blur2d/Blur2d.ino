// Description: This example shows how to blur a strip of LEDs. It uses the blur1d function to blur the strip and fadeToBlackBy to dim the strip. A bright pixel moves along the strip.
// Author: Zach Vorhies


#include <FastLED.h>
#include "fl/ui.h"
#include "fl/xymap.h"

using namespace fl;

#define WIDTH 22
#define HEIGHT 22

#define NUM_LEDS (WIDTH * HEIGHT)
#define BLUR_AMOUNT 16
#define DATA_PIN 3  // Change this to match your LED strip's data pin
#define BRIGHTNESS 255

CRGB leds[NUM_LEDS];
uint8_t pos = 0;
bool toggle = false;
XYMap xymap(WIDTH, HEIGHT, false);


void setup() {
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  // Add a bright pixel that moves
  //leds[pos] = CHSV(pos * 2, 255, 255);
  // Blur the entire strip
  blur2d(leds, WIDTH, HEIGHT, BLUR_AMOUNT, xymap);
  //fadeToBlackBy(leds, NUM_LEDS, 1);

  EVERY_N_MILLISECONDS(250) {
    int x = random(WIDTH);
    int y = random(HEIGHT);
    uint8_t r = random(255);
    uint8_t g = random(255);
    uint8_t b = random(255);
    CRGB c = CRGB(r, g, b);
    leds[xymap(x, y)] = c;
    if (x < WIDTH - 1 && y < HEIGHT - 1) {
      leds[xymap(x+1, y)] = c;
      leds[xymap(x, y+1)] = c;
      leds[xymap(x+1, y+1)] = c;
    }
  }

  FastLED.show();
  delay(20);
}
