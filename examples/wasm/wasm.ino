/// @file    NoisePlusPalette.ino
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.ino

// printf
#include <stdio.h>
#include <string>

#include <FastLED.h>
#include "fx/2d/noisepalette.hpp"
#include "third_party/arduinojson/json.h"
#include "slice.h"

#include "platforms/wasm/js.h"

#include "ui.h"


#define LED_PIN 3
#define BRIGHTNESS 96
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 100
#define MATRIX_HEIGHT 100
#define GRID_SERPENTINE false

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
#define SCALE 20

// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward. Try
// 1 for a very slow moving effect, or 60 for something that ends up looking
// like water.
#define SPEED 30

CRGB leds[NUM_LEDS];
XYMap xyMap = XYMap::constructRectangularGrid(MATRIX_WIDTH, MATRIX_HEIGHT);
NoisePalette noisePalette = NoisePalette(xyMap);

Slider brightness("Brightness", 255, 0, 255);
Slider speed("Speed", 15, 1, 50);
Checkbox isOff("Set Black", false);
Checkbox changePallete("Auto Next", true);
Slider changePalletTime("Change Palette Time", 5, 1, 100);
Slider scale( "Scale", 20, 1, 100);
Button changePalette("Next Palette");

void setup() {
    delay(1000); // sanity delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setCanvasUi(xyMap);
    FastLED.setBrightness(96);
    noisePalette.setSpeed(speed);
    noisePalette.setScale(scale);
}

void loop() {
    FastLED.setBrightness(!isOff ? brightness.as<uint8_t>() : 0);
    noisePalette.setSpeed(speed);
    noisePalette.setScale(scale);
    static int frame = 0;
    EVERY_N_MILLISECONDS_DYNAMIC(changePalletTime.as<int>() * 1000) {
        if (changePallete) {
            noisePalette.changeToRandomPalette();
        }
    }

    if (changePalette) {
        noisePalette.changeToRandomPalette();

    }

    EVERY_N_MILLISECONDS(1000) {
        printf("fastled running\r\n");
    }

    noisePalette.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
    frame++;
}


