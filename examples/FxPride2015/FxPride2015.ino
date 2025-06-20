/// @file    FxPride2015.ino
/// @brief   Pride2015 effect with ScreenMap
/// @example FxPride2015.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <FastLED.h>
#include "fx/1d/pride2015.h"
#include "fl/screenmap.h"

using namespace fl;

#define DATA_PIN    3
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    200
#define BRIGHTNESS  255

CRGB leds[NUM_LEDS];
Pride2015 pride(NUM_LEDS);

void setup() {
  ScreenMap screenMap = ScreenMap::DefaultStrip(NUM_LEDS, 1.5f, 0.8f);
  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setScreenMap(screenMap)
    .setDither(BRIGHTNESS < 255);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  pride.draw(Fx::DrawContext(millis(), leds));
  FastLED.show();  
}
