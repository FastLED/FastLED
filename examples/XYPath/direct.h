

/*
This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

#include <Arduino.h>
#include <FastLED.h>

#include "fl/draw_visitor.h"
#include "fl/math_macros.h"
#include "fl/raster.h"
#include "fl/time_alpha.h"
#include "fl/ui.h"
#include "fl/xypath.h"
#include "fl/fx/time.h"
#include "fl/leds.h"

#include "src/xypaths.h"

// Sketch.
#include "src/wave.h"
#include "src/xypaths.h"
#include "fl/stl/function.h"



#define HEIGHT 64
#define WIDTH 64
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true
#define TIME_ANIMATION 1000 // ms

fl::LedsXY<WIDTH, HEIGHT> leds;
fl::XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);
fl::UITitle title("Simple control of an xy path");
fl::UIDescription description("This is more of a test for new features.");

// fl::UIButton trigger("My Trigger");
fl::UISlider offset("Offset", 0.0f, 0.0f, 1.0f, 0.01f);
fl::UISlider steps("Steps", 100.0f, 1.0f, 200.0f, 1.0f);
fl::UISlider length("Length", 1.0f, 0.0f, 1.0f, 0.01f);

fl::XYPathPtr heartPath = fl::XYPath::NewHeartPath(WIDTH, HEIGHT);

void setup() {
    Serial.begin(115200);
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(screenmap);
}

void loop() {
    // leds(x,y) = fl::CRGB(255, 0, 0);
    fl::clear(leds);
    float from = offset;
    float to = length.value() + offset.value();
    heartPath->drawColor(fl::CRGB(255, 0, 0), from, to, &leds, steps.as_int());
    FastLED.show();
}
