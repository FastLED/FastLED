// @filter: (memory is high)

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

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include <FastLED.h>

// Note: FASTLED_ALLOW_INTERRUPTS improves performance on AVR platforms
// Commented out to enable precompiled headers for faster compilation
// #define FASTLED_ALLOW_INTERRUPTS 0
#include "fl/fx/1d/pacifica.h"
#include "fl/screenmap.h"

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
    pacifica.draw(Fx::DrawContext(fl::millis(), leds));
    FastLED.show();
  }
}
