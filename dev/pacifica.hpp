#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include "fx/pacifica.hpp"

#define DATA_PIN            2
#define NUM_LEDS            60
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE            WS2812B
#define COLOR_ORDER         BRG

CRGB leds[NUM_LEDS];
PacificaData pacificaData(leds, NUM_LEDS);

void setup() {
  delay(3000); // 3 second delay for boot recovery, and a moment of silence
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setRgbw();
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
}

void loop() {
  EVERY_N_MILLISECONDS(20) {
    PacificaLoop(pacificaData);
    FastLED.show();
  }
}
