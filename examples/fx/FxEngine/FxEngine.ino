/// @file    FxEngine.ino
/// @brief   Demonstrates how to use the FxEngine to switch between different effects on a 2D LED matrix.
///          This example is compatible with the new FastLED wasm compiler. Install it by running
///          `pip install fastled` then running `fastled` in this sketch directory.
/// @example FxEngine.ino


#include <FastLED.h>

#if  defined(__AVR__) || defined(STM32F1) || defined(ARDUINO_TEENSY30) || defined(ARDUINO_TEENSY31) || defined(ARDUINO_TEENSYLC)
// __AVR__:  Not enough memory enough for the FxEngine, so skipping this example
// STM32F1:  Also not big enough to hold the FxEngine, and fill fail at the linking error.
//           If you are on PlatformIO then you have the ability to pass in --specs=nano.specs
//           to the linker in order to resolve this. As such, this example is disable for the
//           common platform, which is assumed to be Arduino.
// Teensy30: Similar - linker error about overflowing Flash
// Teensy31: Similar - linker error about overflowing Flash
// TeensyLC: Similar - linker error about overflowing Flash
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
