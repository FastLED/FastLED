/// @file    Noise.ino
/// @brief   Demonstrates how to use noise generation on a 2D LED matrix
/// @example Noise.ino


#include <FastLED.h>

#include "fx/2d/noisepalette.h"
#include "fx/fx_engine.h"
#include "fx/2d/animartrix.hpp"
#include "fx/2d/scale_up.h"
#include "fx/fx_engine.h"

#define LED_PIN 2
#define BRIGHTNESS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define MATRIX_SMALL_WIDTH 11
#define MATRIX_SMALL_HEIGHT 11
#define MATRIX_WIDTH 22
#define MATRIX_HEIGHT 22
#define GRID_SERPENTINE 1

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

#define SCALE 20
#define SPEED 30

CRGB leds[NUM_LEDS];
XYMap xyMap(MATRIX_WIDTH, MATRIX_HEIGHT, GRID_SERPENTINE);
XYMap xyMapSmall = XYMap::constructRectangularGrid(MATRIX_SMALL_WIDTH, MATRIX_SMALL_HEIGHT);
AnimartrixRef animartrix = Fx::make<Animatrix>(xyMapSmall, POLAR_WAVES);
ScaleUp scaleUp(xyMap, animartrix.get());
FxEngine fxEngine(NUM_LEDS);


void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(96);
    fxEngine.addFx(&scaleUp);
}

void loop() {
    fxEngine.draw(millis(), leds);
    FastLED.show();
}
