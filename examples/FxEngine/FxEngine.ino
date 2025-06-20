/// @file    FxEngine.ino
/// @brief   Demonstrates FxEngine for switching between effects
/// @example FxEngine.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <FastLED.h>
using namespace fl;

#if  defined(__AVR__)
// __AVR__:  Not enough memory enough for the FxEngine, so skipping this example
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


UISlider SCALE("SCALE", 20, 20, 100);
UISlider SPEED("SPEED", 30, 20, 100);

CRGB leds[NUM_LEDS];
XYMap xyMap(MATRIX_WIDTH, MATRIX_HEIGHT, IS_SERPINTINE);  // No serpentine
NoisePalette noisePalette1(xyMap);
NoisePalette noisePalette2(xyMap);
FxEngine fxEngine(NUM_LEDS);
UICheckbox switchFx("Switch Fx", true);

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(MATRIX_WIDTH, MATRIX_HEIGHT);
    FastLED.setBrightness(96);
    noisePalette1.setPalettePreset(2);
    noisePalette2.setPalettePreset(4);
    fxEngine.addFx(noisePalette1);
    fxEngine.addFx(noisePalette2);
}

void loop() {
    noisePalette1.setSpeed(SPEED);
    noisePalette1.setScale(SCALE);
    noisePalette2.setSpeed(SPEED);
    noisePalette2.setScale(int(SCALE) * 3 / 2);  //  Make the different.
    EVERY_N_SECONDS(1) {
        if (switchFx) {
            fxEngine.nextFx(500);
        }
    }
    fxEngine.draw(millis(), leds);
    FastLED.show();
}

#endif  // __AVR__
