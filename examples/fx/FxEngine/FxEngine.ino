/// @file    FxEngine.ino
/// @brief   Demonstrates how to use the FxEngine to switch between different effects on a 2D LED matrix.
///          This example is compatible with the new FastLED wasm compiler. Install it by running
///          `pip install fastled` then running `fastled` in this sketch directory.
/// @example FxEngine.ino


#include <FastLED.h>

#ifdef __AVR__
// Not powerful enough for the FxEngine, so skipping this example
void setup() {}
void loop() {}

#else

#include "fx/2d/noisepalette.h"
#include "fx/2d/animartrix.hpp"
#include "fx/fx_engine.h"
#include "fl/ui.h"

#define LED_PIN 2
#define BRIGHTNESS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 22
#define MATRIX_HEIGHT 22

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

#ifdef __EMSCRIPTEN__
#define IS_SERPINTINE false
#else
#define IS_SERPINTINE true
#endif


Slider SCALE("SCALE", 20, 20, 100);
Slider SPEED("SPEED", 30, 20, 100);

CRGB leds[NUM_LEDS];
XYMap xyMap(MATRIX_WIDTH, MATRIX_HEIGHT, IS_SERPINTINE);  // No serpentine
NoisePalette noisePalette(xyMap);
Animartrix animartrix(xyMap, POLAR_WAVES);
FxEngine fxEngine(NUM_LEDS);
Checkbox switchFx("Switch Fx", true);

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(MATRIX_WIDTH, MATRIX_HEIGHT);
    FastLED.setBrightness(96);
    noisePalette.setPalettePreset(2);
    fxEngine.addFx(noisePalette);
    fxEngine.addFx(animartrix);
}

void loop() {
    noisePalette.setSpeed(SPEED);
    noisePalette.setScale(SCALE);
    EVERY_N_SECONDS(1) {
        if (switchFx) {
            fxEngine.nextFx(500);
        }
    }
    fxEngine.draw(millis(), leds);
    FastLED.show();
}

#endif  // __AVR__
