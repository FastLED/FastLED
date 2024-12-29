/// @file    Animartrix.ino
/// @brief   Demonstrates Stefan Petricks Animartrix effects.
/// @author  Stefan Petrick
/// @author  Zach Vorhies (FastLED adaptation)
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`


#include <stdio.h>
#include <string>

#include <FastLED.h>
#include "fl/json.h"
#include "fl/slice.h"
#include "fx/fx_engine.h"

#include "fx/2d/animartrix.hpp"
#include "fl/ui.h"

using namespace fl;


#define LED_PIN 3
#define BRIGHTNESS 96
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 22
#define MATRIX_HEIGHT 22

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

#define FIRST_ANIMATION POLAR_WAVES


CRGB leds[NUM_LEDS];
XYMap xyMap = XYMap::constructRectangularGrid(MATRIX_WIDTH, MATRIX_HEIGHT);


UITitle title("Animartrix");
UIDescription description("Demo of the Animatrix effects. @author of fx is StefanPetrick");

UISlider brightness("Brightness", 255, 0, 255);
UINumberField fxIndex("Animartrix - index", 0, 0, NUM_ANIMATIONS - 1);
UISlider timeSpeed("Time Speed", 1, -10, 10, .1);

Animartrix animartrix(xyMap, FIRST_ANIMATION);
FxEngine fxEngine(NUM_LEDS);

void setup() {
    FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(xyMap);
    FastLED.setBrightness(brightness);
    fxEngine.addFx(animartrix);
}

void loop() {
    FastLED.setBrightness(brightness);
    fxEngine.setSpeed(timeSpeed);
    static int lastFxIndex = -1;
    if (fxIndex.value() != lastFxIndex) {
        lastFxIndex = fxIndex;
        animartrix.fxSet(fxIndex);
    }
    fxEngine.draw(millis(), leds);
    FastLED.show();
}


