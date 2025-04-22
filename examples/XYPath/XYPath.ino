

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
WaveEffect wave_fx; // init in setup().
fl::vector<XYPathPtr> shapes = CreateXYPaths(WIDTH, HEIGHT);

// A vector for collecting subpixels, with overflow.
vector_inlined<SubPixel2x2, 32> subpixels;

TimeWarp time_warp;

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
UIButton trigger("Trigger");
UISlider whichShape("Which Shape", 0.0f, 0.0f, shapes.size() - 1, 1.0f);
UICheckbox useWaveFx("Use WaveFX", true);
UISlider transition("Transition", 0.0f, 0.0f, 1.0f, 0.01f);

UISlider scale("Scale", 1.0f, 0.0f, 1.0f, 0.01f);
UISlider speed("Speed", 1.0f, 0.25f, 20.0f, 0.01f);
UISlider numberOfSteps("Number of Steps", 32.0f, 1.0f, 100.0f, 1.0f);




//////////////////// LOOP SECTION /////////////////////////////

float getAnimationTime(uint32_t now) {
    float pointf = pointTransition.updatef(now);
    FASTLED_WARN("pointf: " << pointf);
    return pointf + transition.value();
}

void clearLeds() { memset(leds, 0, NUM_LEDS * sizeof(CRGB)); }


void loop() {
    // Your code here
    clearLeds();
    time_warp.setSpeed(speed.value());
    const uint32_t now = millis();
    uint32_t now_warped = time_warp.update(now);


    auto shape = getShape(whichShape.as<int>());
    shape->setScale(scale.value());

    float curr_alpha = getAnimationTime(now_warped);
    FASTLED_WARN("Alpha: " << curr_alpha);
    FASTLED_WARN("now_warped: " << now_warped);
    static float s_prev_alpha = 0.0f;

    // unconditionally apply the circle.
    if (trigger) {
        // trigger the transition
        time_warp.reset(now);
        now_warped = time_warp.update(now);
        pointTransition.trigger(now_warped);
        FASTLED_WARN("Transition triggered on " << shape->name());
        curr_alpha = getAnimationTime(now_warped);
        s_prev_alpha = curr_alpha;
    }

    const bool is_active = pointTransition.isActive(now_warped);

    // if (pointTransition.isActive(now)) {
    static uint32_t frame = 0;
    frame++;
    clearLeds();
    const CRGB purple = CRGB(255, 0, 255);
    subpixels.clear();
    const int number_of_steps = numberOfSteps.value();

    // float factor = s_prev_alpha;  // 0->1.f
    // factor = MIN(factor/4.0f, 0.05f);


    float diff = curr_alpha - s_prev_alpha;
    diff *= 1.0f;
    float factor = MAX(s_prev_alpha - diff, 0.f);

    for (int i = 0; i < number_of_steps; ++i) {
        float a = fl::map_range<float>(i, 0, number_of_steps-1, factor,
                                       curr_alpha);
        uint8_t alpha = fl::map_range<uint8_t>(i, 0.0f, number_of_steps - 1, 64, 255);
        if (!is_active) {
            alpha = 0;
        }
        SubPixel2x2 subpixel = shape->at_subpixel(a);
        subpixel.scale(alpha);
        subpixels.push_back(subpixel);
    }
    s_prev_alpha = curr_alpha;
    raster.rasterize(subpixels);
    if (useWaveFx && is_active) {
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

    EVERY_N_SECONDS(1) {
        uint32_t frame_time = millis() - now;
        FASTLED_WARN("Frame time: " << frame_time << "ms");
    }

    FastLED.show();
}
