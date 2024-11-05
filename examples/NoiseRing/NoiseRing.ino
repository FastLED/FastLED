


/// @file    NoisePlusPalette.ino
/// @brief   Demonstrates how to mix noise generation with color palettes on a
/// 2D LED matrix
/// @example NoisePlusPalette.ino

// printf
#include <stdio.h>
#include <string>
#include <vector>

#include <FastLED.h>
#include "json.h"
#include "slice.h"
#include "noisegen.h"
#include "screenmap.h"
#include "math_macros.h"


#include "ui.h"


#define LED_PIN 3
#define BRIGHTNESS 96
#define COLOR_ORDER GRB

#define NUM_LEDS 250

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

Slider brightness("Brightness", 255, 0, 255);


ScreenMap makeScreenMap(int numLeds, float cm_between_leds) {
    ScreenMap screenMap = ScreenMap(numLeds);
    float circumference = numLeds * cm_between_leds;
    float radius = circumference / (2 * M_PI);

    for (int i = 0; i < numLeds; i++) {
        float angle = i * 2 * M_PI / numLeds;
        float x = radius * cos(angle) * 2;
        float y = radius * sin(angle);
        // screenMap[i] = {x, y};
        screenMap.set(i, {x, y});
    }
    screenMap.setDiameter(.5);
    return screenMap;
}


void setup() {
    ScreenMap xyMap = makeScreenMap(NUM_LEDS, 2.0);
    FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenCoords(xyMap);
    FastLED.setBrightness(brightness);
    //noisePalette.setSpeed(speed);
    //noisePalette.setScale(scale);
    //fxEngine.addFx(animartrix);
    //fxEngine.addFx(noisePalette);
}




void loop() {
    //for (int i = 0; i < NUM_LEDS; i++) {
    //    leds[i] = CRGB::White;
    //}

    uint32_t now = millis();

    // inoise8(x + ioffset,y + joffset,z);
    // go in circular formation and set the leds
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * 2 * M_PI / NUM_LEDS;
        float x = cos(angle);
        float y = sin(angle);
        x *= 0xffff;
        y *= 0xffff;
        float noise = inoise16(x, y, now);
        float noise2 = inoise16(x, y, 255 + now);
        float noise3 = inoise16(x, y, 512 + now);

        leds[i] = CHSV(noise >> 8, noise2 >> 8, noise3 >> 8);
    }


    FastLED.show();

}


