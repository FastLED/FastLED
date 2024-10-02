/// @file    Fire2012.ino
/// @brief   Simple one-dimensional fire animation
/// @example Fire2012.ino

#include <FastLED.h>
#include "fx/fire2020.hpp"

#define LED_PIN     2
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define NUM_LEDS    30

#define BRIGHTNESS  128
#define FRAMES_PER_SECOND 30

CRGB leds[NUM_LEDS];
Fire2020Data state = {
  leds,           // leds - required
  NUM_LEDS,       // num_leds - required
  nullptr,        // heat - will be allocated if not provided,
  55,             // cooling
  120,            // sparking
  false,          // reverse_direction
};

void setup() {
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setRgbw();
  FastLED.setBrightness(BRIGHTNESS);
}

void loop()
{
  Fire2020Loop(state); // run simulation frame
  
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

