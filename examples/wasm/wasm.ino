/// @file    wasm.ino
/// @brief   Demonstrates an advanced ino file with multiple effects and UI elements
/// @author  Zach Vorhies
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.


#include <stdio.h>
#include <string>

#include <FastLED.h>
#include "fx/2d/noisepalette.h"
#include "fl/json.h"
#include "fl/slice.h"
#include "fx/fx_engine.h"

#include "fx/2d/animartrix.hpp"
#include "fl/ui.h"

using namespace fl;


#define LED_PIN 3
#define BRIGHTNESS 96
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

UITitle title("FastLED Wasm Demo");
UIDescription description("This example combines two features of FastLED to produce a remarkable range of effects from a relatively small amount of code.  This example combines FastLED's color palette lookup functions with FastLED's Perlin noise generator, and the combination is extremely powerful");

UISlider brightness("Brightness", 255, 0, 255);
UICheckbox isOff("Off", false);
UISlider speed("Noise - Speed", 15, 1, 50);
UICheckbox changePallete("Noise - Auto Palette", true);
UISlider changePalletTime("Noise - Time until next random Palette", 5, 1, 100);
UISlider scale( "Noise - Scale", 20, 1, 100);
UIButton changePalette("Noise - Next Palette");
UIButton changeFx("Switch between Noise & Animartrix");
UINumberField fxIndex("Animartrix - index", 0, 0, NUM_ANIMATIONS);
UISlider timeSpeed("Time Speed", 1, -10, 10, .1);

Animartrix animartrix(xyMap, POLAR_WAVES);
FxEngine fxEngine(NUM_LEDS);

void setup() {
    Serial.begin(115200);
    Serial.println("Sketch setup");
    FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(xyMap);
    Serial.println("FastLED setup done");
    FastLED.setBrightness(brightness);
    //noisePalette.setSpeed(speed);
    noisePalette.setScale(scale);
    fxEngine.addFx(animartrix);
    fxEngine.addFx(noisePalette);
    Serial.println("Sketch setup done");
}

void loop() {
    FastLED.setBrightness(!isOff ? brightness.as<uint8_t>() : 0);
    noisePalette.setSpeed(speed);
    noisePalette.setScale(scale);
    fxEngine.setSpeed(timeSpeed);

    if (changeFx) {
        fxEngine.nextFx();
    }
    static int frame = 0;
    EVERY_N_MILLISECONDS_DYNAMIC(changePalletTime.as<int>() * 1000) {
        if (changePallete) {
            noisePalette.changeToRandomPalette();
        }
    }

    if (changePalette) {
        noisePalette.changeToRandomPalette();

    }
    static int lastFxIndex = -1;
    if (fxIndex.value() != lastFxIndex) {
        lastFxIndex = fxIndex;
        animartrix.fxSet(fxIndex);
    }


    fxEngine.draw(millis(), leds);
    FastLED.show();
    frame++;
}


