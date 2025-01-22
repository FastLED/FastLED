#include <FastLED.h>
#include "fx/1d/fire2012.hpp"

#define LED_PIN     2
#define COLOR_ORDER BRG
#define CHIPSET     WS2811
#define NUM_LEDS    30

#define BRIGHTNESS  128
#define FRAMES_PER_SECOND 30
#define COOLING 55
#define SPARKING 120
#define REVERSE_DIRECTION false

CRGB leds[NUM_LEDS];
Fire2012 fire(NUM_LEDS, COOLING, SPARKING, REVERSE_DIRECTION);

void setup() {
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setRgbw();
  FastLED.setBrightness(BRIGHTNESS);
}

void loop()
{
  fire.draw(millis(), leds); // run simulation frame, passing leds array
  
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

