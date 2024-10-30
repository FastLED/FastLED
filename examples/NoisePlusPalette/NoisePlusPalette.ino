/// @file    NoisePlusPalette.ino
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.ino

#ifndef COMPILE_NOISEPLUSPALETTE
#if defined(__AVR__)
// This has grown too large for the AVR to handle.
#define COMPILE_NOISEPLUSPALETTE 0
#else
#define COMPILE_NOISEPLUSPALETTE 1
#endif
#endif // COMPILE_NOISEPLUSPALETTE

#if COMPILE_NOISEPLUSPALETTE

#include <FastLED.h>
#include "fx/2d/noisepalette.hpp"
#include "ui.h"

#define LED_PIN 3
#define BRIGHTNESS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

#if __EMSCRIPTEN__
#define GRID_SERPENTINE 0
#else
#define GRID_SERPENTINE 1
#endif

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// This example combines two features of FastLED to produce a remarkable range
// of effects from a relatively small amount of code.  This example combines
// FastLED's color palette lookup functions with FastLED's Perlin noise
// generator, and the combination is extremely powerful.
//
// You might want to look at the "ColorPalette" and "Noise" examples separately
// if this example code seems daunting.
//
//
// The basic setup here is that for each frame, we generate a new array of
// 'noise' data, and then map it onto the LED matrix through a color palette.
//
// Periodically, the color palette is changed, and new noise-generation
// parameters are chosen at the same time.  In this example, specific
// noise-generation values have been selected to match the given color palettes;
// some are faster, or slower, or larger, or smaller than others, but there's no
// reason these parameters can't be freely mixed-and-matched.
//
// In addition, this example includes some fast automatic 'data smoothing' at
// lower noise speeds to help produce smoother animations in those cases.
//
// The FastLED built-in color palettes (Forest, Clouds, Lava, Ocean, Party) are
// used, as well as some 'hand-defined' ones, and some proceedurally generated
// palettes.

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.
// The higher the value of scale, the more "zoomed out" the noise iwll be.  A
// value of 1 will be so zoomed in, you'll mostly see solid colors.

Slider SCALE("SCALE", 20, 1, 100, 1);

// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward. Try
// 1 for a very slow moving effect, or 60 for something that ends up looking
// like water.
Slider SPEED("SPEED", 30, 1, 60, 1);

CRGB leds[NUM_LEDS];
XYMap xyMap(MATRIX_WIDTH, MATRIX_HEIGHT, GRID_SERPENTINE);
NoisePaletteRef noisePalette = NoisePaletteRef::New(xyMap);

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(96);
    noisePalette->lazyInit();
    noisePalette->setSpeed(SPEED);
    noisePalette->setScale(SCALE);
}

void loop() {
    noisePalette->setSpeed(SPEED);
    noisePalette->setScale(SCALE);
    EVERY_N_MILLISECONDS(5000) { noisePalette->changeToRandomPalette(); }

    noisePalette->draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
}

#else
void setup() {}
void loop() {}
#endif // COMPILE_NOISEPLUSPALETTE
