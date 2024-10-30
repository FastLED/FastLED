#include <FastLED.h>
#include "fx/1d/noisewave.hpp"

#define LED_PIN     2
#define COLOR_ORDER BRG
#define CHIPSET     WS2811
#define NUM_LEDS    484

CRGB leds[NUM_LEDS];
NoiseWaveRef noiseWave = Fx::make<NoiseWave>(NUM_LEDS);

void setup() {
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(128);
  noiseWave->lazyInit();
}

void loop()
{
  noiseWave->draw(DrawContext{millis(), leds});
  FastLED.show();
  FastLED.delay(1000 / 60);
}

