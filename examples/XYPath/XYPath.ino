

/*
This demo is best viewed using the FastLED compiler.
Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

#include <Arduino.h>
#include <FastLED.h>

#include "fl/math_macros.h"
#include "fl/raster.h"
#include "fl/time_alpha.h"
#include "fl/ui.h"
#include "fl/xypath.h"
#include "fl/draw_visitor.h"

// Sketch.
#include "wave.h"
#include "xypaths.h"

using namespace fl;

#define HEIGHT 64
#define WIDTH 64
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];


XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);
// XYPathPtr shape = XYPath::NewRosePath(WIDTH, HEIGHT);
TimeLinear pointTransition(10000);
// Speed up writing to the super sampled waveFx by writing
// to a raster. This will allow duplicate writes to be removed.
XYRaster raster;
WaveEffect wave_fx;  // init in setup().
fl::vector<XYPathPtr> shapes = CreateXYPaths(WIDTH, HEIGHT);

// A vector for collecting subpixels, with overflow.
vector_inlined<SubPixel2x2, 32> subpixels;



XYPathPtr getShape(int which) {
    int len = shapes.size();
    which = which % len;
    if (which < 0) {
        which += len;
    }
    return shapes[which];
}

void setup() {
    Serial.begin(115200);
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(screenmap);

    // Initialize wave simulation. Please don't use static constructors, keep it
    // in setup().
    wave_fx = NewWaveSimulation2D(xyMap);
}



//////////////////// UI Section /////////////////////////////
UITitle title("XYPath Demo");
UIDescription description("Use a path on the WaveFx");

UICheckbox useWaveFx("Use WaveFX", true);
UISlider transition("Transition", 0.0f, 0.0f, 1.0f, 0.01f);
UIButton button("Trigger");
UISlider scale("Scale", 1.0f, 0.0f, 1.0f, 0.01f);
UISlider speed("Speed", 1.0f, 0.25f, 20.0f, 0.01f);
UISlider numberOfSteps("Number of Steps", 32.0f, 1.0f, 100.0f, 1.0f);

UICheckbox advancedFrame("Advanced Frame", true);
UIButton advancedFrameButton("Advanced Frame Button");
UISlider whichShape("Which Shape", 0.0f, 0.0f, shapes.size(), 1.0f);



//////////////////// LOOP SECTION /////////////////////////////


float getAnimationTime(uint32_t now) {
    return speed.value() * pointTransition.updatef(now) + transition.value();
}

void clearLeds() {
    memset(leds, 0, NUM_LEDS * sizeof(CRGB));
}

void loop() {

    const bool advanceFrame =
        advancedFrameButton.clicked() || static_cast<bool>(advancedFrame);
    if (!advanceFrame) {
        return;
    }

    // Your code here
    uint32_t now = millis();
    memset(leds, 0, NUM_LEDS * sizeof(CRGB));
    auto shape = getShape(whichShape.as<int>());
    shape->setScale(scale.value());

    float curr_alpha = getAnimationTime(now);
    static float s_prev_alpha = 0.0f;

    // unconditionally apply the circle.
    if (button) {
        // trigger the transition
        pointTransition.trigger(now);
        FASTLED_WARN("Transition triggered");
        curr_alpha = getAnimationTime(now);
        s_prev_alpha = curr_alpha;
    }

    // if (pointTransition.isActive(now)) {
    static uint32_t frame = 0;
    frame++;
    clearLeds();
        const CRGB purple = CRGB(255, 0, 255);
        subpixels.clear();
        const int number_of_steps = numberOfSteps.value();
        // const float prev_alpha = s_prev_alpha;
        for (int i = 0; i < number_of_steps; ++i) {
            float a = fl::map_range<float>(i, 0, number_of_steps, s_prev_alpha,
                                           curr_alpha);
            SubPixel2x2 subpixel = shape->at_subpixel(a);
            subpixels.push_back(subpixel);
        }
        s_prev_alpha = curr_alpha;
        raster.rasterize(subpixels);
        if (useWaveFx) {
            DrawRasterToWaveSimulator draw_wave_fx(&raster, &wave_fx);
            raster.draw(xyMap, draw_wave_fx);
        } else {
            raster.draw(purple, xyMap, leds);
        }


    int first = xyMap(1, 1);
    int last = xyMap(WIDTH - 2, HEIGHT - 2);

    leds[first] = CRGB(255, 0, 0);
    leds[last] = CRGB(0, 255, 0);
    if (useWaveFx) {
        // fxBlend.draw(Fx::DrawContext(now, leds));
        wave_fx.draw(Fx::DrawContext(now, leds));
    }

    uint32_t frame_time = millis() - now;
    FASTLED_WARN("Frame time: " << frame_time << "ms");

    FastLED.show();
}
