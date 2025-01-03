/// @file    Pacifica.ino
/// @brief   Gentle, blue-green ocean wave animation
/// @example Pacifica.ino

//
//  "Pacifica"
//  Gentle, blue-green ocean waves.
//  December 2019, Mark Kriegsman and Mary Corey March.
//  For Dan.
//


#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include "fx/1d/pacifica.h"

using namespace fl;

#define DATA_PIN            3
#define NUM_LEDS            60
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB

CRGB leds[NUM_LEDS];
Pacifica pacifica(NUM_LEDS);

void setup() {
  delay(3000); // 3 second delay for boot recovery, and a moment of silence
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
}

void loop() {
  EVERY_N_MILLISECONDS(20) {
    pacifica.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
  }
}
