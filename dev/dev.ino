#include <FastLED.h>
#include "fx/noisewave.hpp"

#define LED_PIN     2
#define COLOR_ORDER BRG
#define CHIPSET     WS2811
#define NUM_LEDS    200

CRGB leds[NUM_LEDS];
NoiseWaveData noiseWaveData(leds, NUM_LEDS);

void setup() {
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setRgbw();
  FastLED.setBrightness(128);
}

void loop()
{
  NoiseWaveLoop(noiseWaveData);
  FastLED.show();
  FastLED.delay(1000 / 60);
}

