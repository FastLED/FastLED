

/*
This demo is best viewed using the FastLED compiler.
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
#include "fx/time.h"

// Sketch.
#include "src/wave.h"
#include "src/xypaths.h"

using namespace fl;

#define HEIGHT 64
#define WIDTH 64
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true
#define TIME_ANIMATION 1000 // ms

CRGB leds[NUM_LEDS];

XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);
// XYPathPtr shape = XYPath::NewRosePath(WIDTH, HEIGHT);

// Speed up writing to the super sampled waveFx by writing
// to a raster. This will allow duplicate writes to be removed.



UITitle title("Simple control of an xy path");
UIDescription description("Use a path on the WaveFx");
UIButton trigger("Trigger");

UISlider pointX("Point X", 0.0f, 0.0f, WIDTH - 1, 1.0f);
UISlider pointY("Point Y", 0.0f, 0.0f, HEIGHT - 1, 1.0f);

void setup() {
    Serial.begin(115200);
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(screenmap);

}

//////////////////// LOOP SECTION /////////////////////////////


void loop() {
    // Your code here
    fl::clear(leds);
    int x = pointX.as_int();
    int y = pointY.as_int();

    leds[xyMap(x, y)] = CRGB(255, 0, 0);

    FastLED.show();
}
