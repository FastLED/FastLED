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

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else


// DRAW TIME: 7ms


#include <FastLED.h>
#include "fl/json.h"
#include "fl/slice.h"
#include "fx/fx_engine.h"

#include "fx/2d/animartrix.hpp"
#include "fl/ui.h"

using namespace fl;


#define LED_PIN 3
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
UINumberField fxIndex("Animartrix - index", 0, 0, NUM_ANIMATIONS - 1);
UINumberField colorOrder("Color Order", 0, 0, 5);
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
    FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screen_map);
    FastLED.setBrightness(brightness);
    setup_max_power();
    fxEngine.addFx(animartrix);

    colorOrder.onChanged([](int value) {
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
    uint32_t start = millis();
    FastLED.setBrightness(brightness);
    fxEngine.setSpeed(timeSpeed);
    static int lastFxIndex = -1;
    if (fxIndex.value() != lastFxIndex) {
        lastFxIndex = fxIndex;
        animartrix.fxSet(fxIndex);
    }
    fxEngine.draw(millis(), leds);
    uint32_t end = millis();
    FL_WARN("*** DRAW TIME: " << int(end - start) << "ms");
    FastLED.show();
    uint32_t end2 = millis();
    FL_WARN("*** SHOW + DRAW TIME: " << int(end2 - start) << "ms");
}


#endif  // __AVR__
