// @filter: (memory is high)

/// @file    Animartrix.ino
/// @brief   Demo of the Animatrix effects
/// @example Animartrix.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.
///
/// @author  Stefan Petrick
/// @author  Zach Vorhies (FastLED adaptation)
///

/*
This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.



OVERVIEW:
This is the famouse Animartrix demo by Stefan Petrick. The effects are generated
using polor polar coordinates. The effects are very complex and powerful.
*/

#define FL_ANIMARTRIX_USES_FAST_MATH 1

/*
Performence notes @64x64:
  * ESP32-S3:
    * FL_ANIMARTRIX_USES_FAST_MATH 0: 143ms
    * FL_ANIMARTRIX_USES_FAST_MATH 1: 90ms
*/

#include "FastLED.h"


// DRAW TIME: 7ms


#include <FastLED.h>
#include "fl/json.h"
#include "fl/slice.h"
#include "fl/fx/fx_engine.h"

#include "fl/fx/2d/animartrix.hpp"
#include "fl/ui.h"

using namespace fl;


#ifndef PIN_DATA
#define PIN_DATA 3
#endif  // PIN_DATA

#ifndef LED_PIN
#define LED_PIN PIN_DATA
#endif  // LED_PIN

#define BRIGHTNESS 32
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 64

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

#define FIRST_ANIMATION POLAR_WAVES

// This is purely use for the web compiler to display the animartrix effects.
// This small led was chosen because otherwise the bloom effect is too strong.
#define LED_DIAMETER 0.15  // .15 cm or 1.5mm


#define POWER_LIMITER_ACTIVE
#define POWER_VOLTS 5
#define POWER_MILLIAMPS 2000


CRGB leds[NUM_LEDS];
XYMap xyMap = XYMap::constructRectangularGrid(MATRIX_WIDTH, MATRIX_HEIGHT);


UITitle title("Animartrix");
UIDescription description("Demo of the Animatrix effects. @author of fx is StefanPetrick");

UISlider brightness("Brightness", BRIGHTNESS, 0, 255);
UIDropdown fxIndex("Animartrix - index", {
    "RGB_BLOBS5", "RGB_BLOBS4", "RGB_BLOBS3", "RGB_BLOBS2", "RGB_BLOBS",
    "POLAR_WAVES", "SLOW_FADE", "ZOOM2", "ZOOM", "HOT_BLOB",
    "SPIRALUS2", "SPIRALUS", "YVES", "SCALEDEMO1", "LAVA1",
    "CALEIDO3", "CALEIDO2", "CALEIDO1", "DISTANCE_EXPERIMENT", "CENTER_FIELD",
    "WAVES", "CHASING_SPIRALS", "ROTATING_BLOB", "RINGS", "COMPLEX_KALEIDO",
    "COMPLEX_KALEIDO_2", "COMPLEX_KALEIDO_3", "COMPLEX_KALEIDO_4", "COMPLEX_KALEIDO_5", "COMPLEX_KALEIDO_6",
    "WATER", "PARAMETRIC_WATER", "MODULE_EXPERIMENT1", "MODULE_EXPERIMENT2", "MODULE_EXPERIMENT3",
    "MODULE_EXPERIMENT4", "MODULE_EXPERIMENT5", "MODULE_EXPERIMENT6", "MODULE_EXPERIMENT7", "MODULE_EXPERIMENT8",
    "MODULE_EXPERIMENT9", "MODULE_EXPERIMENT10", "MODULE_EXPERIMENT_SM1", "MODULE_EXPERIMENT_SM2", "MODULE_EXPERIMENT_SM3",
    "MODULE_EXPERIMENT_SM4", "MODULE_EXPERIMENT_SM5", "MODULE_EXPERIMENT_SM6", "MODULE_EXPERIMENT_SM8", "MODULE_EXPERIMENT_SM9",
    "MODULE_EXPERIMENT_SM10", "FLUFFY_BLOBS"
});
UIDropdown colorOrder("Color Order", {"RGB", "RBG", "GRB", "GBR", "BRG", "BGR"});
UISlider timeSpeed("Time Speed", 1, -10, 10, .1);



Animartrix animartrix(xyMap, FIRST_ANIMATION);
FxEngine fxEngine(NUM_LEDS);

const bool kPowerLimiterActive = false;

void setup_max_power() {
    if (kPowerLimiterActive) {
        FastLED.setMaxPowerInVoltsAndMilliamps(POWER_VOLTS, POWER_MILLIAMPS);  // Set max power to 2 amps
    }
}


void setup() {
    Serial.begin(115200);
    FL_WARN("*** SETUP ***");

    auto screen_map = xyMap.toScreenMap();
    screen_map.setDiameter(LED_DIAMETER);
    FastLED.addLeds<WS2811, PIN_DATA, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screen_map);
    FastLED.setBrightness(brightness);
    setup_max_power();
    fxEngine.addFx(animartrix);

    colorOrder.onChanged([](UIDropdown &dropdown) {
        int value = dropdown.as_int();
        switch(value) {
            case 0: value = RGB; break;
            case 1: value = RBG; break;
            case 2: value = GRB; break;
            case 3: value = GBR; break;
            case 4: value = BRG; break;
            case 5: value = BGR; break;
        }
        animartrix.setColorOrder(static_cast<EOrder>(value));
    });
}

void loop() {
    FL_WARN("*** LOOP ***");
    uint32_t start = fl::millis();
    FastLED.setBrightness(brightness);
    fxEngine.setSpeed(timeSpeed);
    static int lastFxIndex = -1;
    if (fxIndex.as_int() != lastFxIndex) {
        lastFxIndex = fxIndex.as_int();
        animartrix.fxSet(fxIndex.as_int());
    }
    fxEngine.draw(fl::millis(), leds);
    uint32_t end = fl::millis();
    FL_WARN("*** DRAW TIME: " << int(end - start) << "ms");
    FastLED.show();
    uint32_t end2 = fl::millis();
    FL_WARN("*** SHOW + DRAW TIME: " << int(end2 - start) << "ms");
}
