

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

#include "fl/downscale.h"
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
#define TIME_ANIMATION 1000 // ms

CRGB leds[NUM_LEDS];
CRGB leds_downscaled[NUM_LEDS / 4]; // Downscaled buffer

XYMap xyMap(WIDTH, HEIGHT, false);
XYMap xyMap_Dst(WIDTH / 2, HEIGHT / 2,
                false); // Framebuffer is regular rectangle LED matrix.
// XYPathPtr shape = XYPath::NewRosePath(WIDTH, HEIGHT);

// Speed up writing to the super sampled waveFx by writing
// to a raster. This will allow duplicate writes to be removed.

WaveEffect wave_fx; // init in setup().
fl::vector<XYPathPtr> shapes = CreateXYPaths(WIDTH, HEIGHT);

XYRaster raster(WIDTH, HEIGHT);
TimeWarp time_warp;

XYPathPtr getShape(int which) {
    int len = shapes.size();
    which = which % len;
    if (which < 0) {
        which += len;
    }
    return shapes[which];
}

//////////////////// UI Section /////////////////////////////
UITitle title("XYPath Demo");
UIDescription description("Use a path on the WaveFx");
UIButton trigger("Trigger");
UISlider whichShape("Which Shape", 0.0f, 0.0f, shapes.size() - 1, 1.0f);
UICheckbox useWaveFx("Use WaveFX", true);
UISlider transition("Transition", 0.0f, 0.0f, 1.0f, 0.01f);

UISlider scale("Scale", 1.0f, 0.0f, 1.0f, 0.01f);
UISlider speed("Speed", 1.0f, -20.0f, 20.0f, 0.01f);
UISlider numberOfSteps("Number of Steps", 32.0f, 1.0f, 100.0f, 1.0f);
UISlider maxAnimation("Max Animation", 1.0f, 5.0f, 20.0f, 1.f);

TimeClampedTransition shapeProgress(TIME_ANIMATION);

void setupUiCallbacks() {
    speed.onChanged([](UISlider& slider) {
        time_warp.setSpeed(slider.value());
    });
    maxAnimation.onChanged(
        [](UISlider& slider) {
            shapeProgress.set_max_clamp(slider.value());
        });

    trigger.onClicked([]() {
        // shapeProgress.trigger(millis());
        FASTLED_WARN("Trigger pressed");
    });
    useWaveFx.onChanged([](fl::UICheckbox &checkbox) {
        if (checkbox.value()) {
            FASTLED_WARN("WaveFX enabled");
        } else {
            FASTLED_WARN("WaveFX disabled");
        }
    });
}

void setup() {
    Serial.begin(115200);
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);
    FastLED.addLeds<NEOPIXEL, 2>(leds, xyMap.getTotal())
        .setScreenMap(screenmap);
    auto screenmap2 = xyMap_Dst.toScreenMap();
    screenmap.setDiameter(.5);
    screenmap2.addOffsetY(-HEIGHT / 2);
    FastLED.addLeds<NEOPIXEL, 3>(leds_downscaled, xyMap_Dst.getTotal())
        .setScreenMap(screenmap2);
    setupUiCallbacks();
    // Initialize wave simulation. Please don't use static constructors, keep it
    // in setup().
    trigger.click();
    wave_fx = NewWaveSimulation2D(xyMap);
}

//////////////////// LOOP SECTION /////////////////////////////

float getAnimationTime(uint32_t now) {
    float pointf = shapeProgress.updatef(now);
    return pointf + transition.value();
}

void clearLeds() {
    fl::clear(leds);
    fl::clear(leds_downscaled);
};

void loop() {
    // Your code here
    clearLeds();
    const uint32_t now = millis();
    uint32_t now_warped = time_warp.update(now);

    auto shape = getShape(whichShape.as<int>());
    shape->setScale(scale.value());

    float curr_alpha = getAnimationTime(now_warped);
    static float s_prev_alpha = 0.0f;

    // unconditionally apply the circle.
    if (trigger) {
        // trigger the transition
        time_warp.reset(now);
        now_warped = time_warp.update(now);
        shapeProgress.trigger(now_warped);
        FASTLED_WARN("Transition triggered on " << shape->name());
        curr_alpha = getAnimationTime(now_warped);
        s_prev_alpha = curr_alpha;
    }

    clearLeds();
    const CRGB purple = CRGB(255, 0, 255);
    const int number_of_steps = numberOfSteps.value();
    raster.reset();

    float diff = curr_alpha - s_prev_alpha;
    diff *= 1.0f;
    float factor = MAX(s_prev_alpha - diff, 0.f);

    for (int i = 0; i < number_of_steps; ++i) {
        float a =
            fl::map_range<float>(i, 0, number_of_steps - 1, factor, curr_alpha);
        if (a < .04) {
            // shorter tails at first.
            a = map_range<float>(a, 0.0f, .04f, 0.0f, .04f);
        }
        float diff_max_alpha = maxAnimation.value() - curr_alpha;
        if (diff_max_alpha < 0.94) {
            // shorter tails at the end.
            a = map_range<float>(a, curr_alpha, maxAnimation.value(),
                                 curr_alpha, maxAnimation.value());
        }
        uint8_t alpha =
            fl::map_range<uint8_t>(i, 0.0f, number_of_steps - 1, 64, 255);
        Tile2x2_u8 subpixel = shape->at_subpixel(a);
        subpixel.scale(alpha);
        // subpixels.push_back(subpixel);
        raster.rasterize(subpixel);
    }

    s_prev_alpha = curr_alpha;

    if (useWaveFx) {
        DrawRasterToWaveSimulator draw_wave_fx(&wave_fx);
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

    // downscaleBilinear(leds, WIDTH, HEIGHT, leds_downscaled, WIDTH / 2,
    //                   HEIGHT / 2);

    downscaleHalf(leds, xyMap, leds_downscaled, xyMap_Dst);

    // Print out the first 10 pixels of the original and downscaled
    fl::vector_inlined<CRGB, 10> downscaled_pixels;
    fl::vector_inlined<CRGB, 10> original_pixels;
    for (int i = 0; i < 10; ++i) {
        original_pixels.push_back(leds[i]);
        downscaled_pixels.push_back(leds_downscaled[i]);
    }

    FastLED.show();
}
