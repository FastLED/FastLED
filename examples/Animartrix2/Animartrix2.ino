// @filter: (memory is high)

/// @file    Animartrix2.ino
/// @brief   Demo of the Animatrix2 effects (fixed-point experimental)
/// @example Animartrix2.ino
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
This is the Animartrix2 demo - a fixed-point experimental rewrite of the
famous Animartrix effects by Stefan Petrick. The effects are generated
using polar coordinates with fixed-point math for improved performance.
You can switch between Animartrix (float) and Animartrix2 (fixed-point)
using the Version dropdown to compare quality and performance.
*/

#define FL_ANIMARTRIX_USES_FAST_MATH 1

#include "FastLED.h"


// DRAW TIME: 7ms


#include <FastLED.h>
#include "fl/json.h"
#include "fl/slice.h"
#include "fl/fx/fx_engine.h"

#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/animartrix2.hpp"
#include "fl/ui.h"

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

#define FIRST_ANIMATION_V1 fl::POLAR_WAVES
#define FIRST_ANIMATION_V2 fl::ANIM2_POLAR_WAVES

// This is purely use for the web compiler to display the animartrix effects.
// This small led was chosen because otherwise the bloom effect is too strong.
#define LED_DIAMETER 0.15  // .15 cm or 1.5mm


#define POWER_LIMITER_ACTIVE
#define POWER_VOLTS 5
#define POWER_MILLIAMPS 2000


fl::CRGB leds[NUM_LEDS];
fl::XYMap xyMap = fl::XYMap::constructRectangularGrid(MATRIX_WIDTH, MATRIX_HEIGHT);


fl::UITitle title("Animartrix2");
fl::UIDescription description("Demo of the Animatrix effects. Switch between Animartrix2 (fixed-point) and Animartrix (float) using the Version dropdown. @author of fx is StefanPetrick");

fl::UISlider brightness("Brightness", BRIGHTNESS, 0, 255);
fl::UIDropdown version("Version", {"Animartrix2 (fixed-point)", "Animartrix (float)"});
fl::UIDropdown fxIndex("Animation", {
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
fl::UIDropdown colorOrder("Color Order", {"RGB", "RBG", "GRB", "GBR", "BRG", "BGR"});
fl::UISlider timeSpeed("Time Speed", 1, -10, 10, .1);


fl::Animartrix animartrix_v1(xyMap, FIRST_ANIMATION_V1);
fl::Animartrix2 animartrix_v2(xyMap, FIRST_ANIMATION_V2);
fl::FxEngine fxEngine(NUM_LEDS);

int v1Id = -1;
int v2Id = -1;

const bool kPowerLimiterActive = false;

void setup_max_power() {
    if (kPowerLimiterActive) {
        FastLED.setMaxPowerInVoltsAndMilliamps(POWER_VOLTS, POWER_MILLIAMPS);  // Set max power to 2 amps
    }
}

fl::EOrder toColorOrder(int value) {
    switch(value) {
        case 0: return static_cast<fl::EOrder>(RGB);
        case 1: return static_cast<fl::EOrder>(RBG);
        case 2: return static_cast<fl::EOrder>(GRB);
        case 3: return static_cast<fl::EOrder>(GBR);
        case 4: return static_cast<fl::EOrder>(BRG);
        case 5: return static_cast<fl::EOrder>(BGR);
        default: return static_cast<fl::EOrder>(RGB);
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

    // Add both versions - v2 first (default)
    v2Id = fxEngine.addFx(animartrix_v2);
    v1Id = fxEngine.addFx(animartrix_v1);

    colorOrder.onChanged([](fl::UIDropdown &dropdown) {
        fl::EOrder order = toColorOrder(dropdown.as_int());
        animartrix_v1.setColorOrder(order);
        animartrix_v2.setColorOrder(order);
    });
}

void loop() {
    FL_WARN("*** LOOP ***");
    uint32_t start = fl::millis();
    FastLED.setBrightness(brightness);
    fxEngine.setSpeed(timeSpeed);

    // Handle version switch
    static int lastVersion = -1;
    if (version.as_int() != lastVersion) {
        lastVersion = version.as_int();
        if (lastVersion == 0) {
            fxEngine.setNextFx(v2Id, 0);
        } else {
            fxEngine.setNextFx(v1Id, 0);
        }
    }

    // Handle animation index change - sync both versions
    static int lastFxIndex = -1;
    if (fxIndex.as_int() != lastFxIndex) {
        lastFxIndex = fxIndex.as_int();
        animartrix_v1.fxSet(lastFxIndex);
        animartrix_v2.fxSet(lastFxIndex);
    }

    fxEngine.draw(fl::millis(), leds);
    uint32_t end = fl::millis();
    FL_WARN("*** DRAW TIME: " << int(end - start) << "ms");
    FastLED.show();
    uint32_t end2 = fl::millis();
    FL_WARN("*** SHOW + DRAW TIME: " << int(end2 - start) << "ms");
}
