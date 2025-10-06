/// @file    FxPacifica.ino
/// @brief   Pacifica ocean effect with ScreenMap
/// @example FxPacifica.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

//
//  "Pacifica"
//  Gentle, blue-green ocean waves.
//  December 2019, Mark Kriegsman and Mary Corey March.
//  For Dan.
//


#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include "fx/1d/pacifica.h"
#include "fl/screenmap.h"
#include "defs.h"  // for ENABLE_SKETCH

#if !ENABLE_SKETCH
void setup() {}
void loop() {}
#else


using namespace fl;

#define DATA_PIN            3
#define NUM_LEDS            60
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB

CRGB leds[NUM_LEDS];
Pacifica pacifica(NUM_LEDS);

void setup() {
  Serial.begin(115200);
  ScreenMap screenMap = ScreenMap::DefaultStrip(NUM_LEDS, 1.5f, 0.5f);
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenMap);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
}

void loop() {
  EVERY_N_MILLISECONDS(20) {
    pacifica.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
  }
}

#endif // ENABLE_SKETCH
