#include "fx/2d/noisepalette.hpp"
#include <FastLED.h>

#define LED_PIN 2
#define COLOR_ORDER GRB
#define CHIPSET WS2811

#define MATRIX_WIDTH 22
#define MATRIX_HEIGHT 22

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

CRGB leds[NUM_LEDS];
XYMap xyMap(MATRIX_WIDTH, MATRIX_HEIGHT);
NoisePalette noisePalette(leds, xyMap);

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(96);
    noisePalette.lazyInit();
}

void loop() {
    EVERY_N_MILLISECONDS(5000) { noisePalette.changeToRandomPalette(); }

    noisePalette.draw();
    FastLED.show();
}
