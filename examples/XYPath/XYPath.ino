

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
#include "fx/2d/blend.h"
#include "fx/2d/wave.h"

using namespace fl;

#define HEIGHT 64
#define WIDTH 64
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];

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

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   // Black
    32,  0,   0,   70,  // Dark blue
    128, 20,  57,  255, // Electric blue
    255, 255, 255, 255  // White
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    0,   0,   0,   0,   // black
    8,   128, 64,  64,  // green
    16,  255, 222, 222, // red
    64,  255, 255, 255, // white
    255, 255, 255, 255  // white
};

XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);
XYMap xyRect(WIDTH, HEIGHT, false);
WaveFx
    waveFxLower(xyRect,
                WaveFx::Args{
                    .factor = SUPER_SAMPLE_2X,
                    .half_duplex = true,
                    .speed = 0.18f,
                    .dampening = 9.0f,
                    .crgbMap = WaveCrgbGradientMapPtr::New(electricBlueFirePal),
                });

WaveFx waveFxUpper(
    xyRect, WaveFx::Args{
                .factor = SUPER_SAMPLE_2X,
                .half_duplex = true,
                .speed = 0.25f,
                .dampening = 3.0f,
                .crgbMap = WaveCrgbGradientMapPtr::New(electricGreenFirePal),
            });

Blend2d fxBlend(xyMap);

XYPathPtr shape = XYPath::NewArchimedeanSpiralPath(WIDTH, HEIGHT);
TimeLinear pointTransition(10000);
// Speed up writing to the super sampled waveFx by writing
// to a raster. This will allow duplicate writes to be removed.
Raster raster;

void setup() {
    Serial.begin(115200);
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(screenmap);
    fxBlend.add(waveFxLower);
    fxBlend.add(waveFxUpper);
    // shape->setDrawBounds(WIDTH, HEIGHT);
}

float getAlpha(uint32_t now) {
    return speed.value() * pointTransition.updatef(now) + transition.value();
}

void loop() {

    FASTLED_WARN("Loop");

    const bool advanceFrame =
        advancedFrameButton.clicked() || static_cast<bool>(advancedFrame);
    if (!advanceFrame) {
        return;
    }

    // Your code here
    uint32_t now = millis();
    memset(leds, 0, NUM_LEDS * sizeof(CRGB));

    shape->setScale(scale.value());

    float curr_alpha = getAlpha(now);
    static float s_prev_alpha = 0.0f;

    // unconditionally apply the circle.
    if (button) {
        // trigger the transition
        pointTransition.trigger(now);
        FASTLED_WARN("Transition triggered");
        curr_alpha = getAlpha(now);
        s_prev_alpha = curr_alpha;
    }

    // if (pointTransition.isActive(now)) {
    static uint32_t frame = 0;
    frame++;
    memset(leds, 0, NUM_LEDS * sizeof(CRGB));
    if (true) {
        const CRGB purple = CRGB(255, 0, 255);

        static vector_inlined<SubPixel2x2, 32> subpixels;
        subpixels.clear();
        const int number_of_steps = numberOfSteps.value();

        // const float prev_alpha = s_prev_alpha;
        for (int i = 0; i < number_of_steps; ++i) {
            float a = fl::map_range<float>(i, 0, number_of_steps, s_prev_alpha,
                                           curr_alpha);
            SubPixel2x2 subpixel = shape->at_subpixel(a);
            subpixels.push_back(subpixel);
        }
        raster.rasterize(subpixels);
        s_prev_alpha = curr_alpha;
        if (useWaveFx) {
            auto origin = raster.origin();
            auto width = raster.width();
            auto height = raster.height();
            for (uint16_t x = 0; x < raster.width(); ++x) {
                for (uint16_t y = 0; y < raster.height(); ++y) {
                    uint8_t value = raster.at(x, y);
                    if (value > 0) {
                        int xx = x + origin.x;
                        int yy = y + origin.y;
                        if (xyMap.has(xx, yy)) {
                            float valuef = value / 255.0f;
                            int idx = xyMap(xx, yy);
                            waveFxLower.addf(origin.x + x, origin.y + y,
                                             valuef);
                        }
                    }
                }
            }
        } else {
            raster.draw(purple, xyMap, leds);
        }
    }

    int first = xyMap(1, 1);
    int last = xyMap(WIDTH - 2, HEIGHT - 2);

    leds[first] = CRGB(255, 0, 0);
    leds[last] = CRGB(0, 255, 0);
    if (useWaveFx) {
        fxBlend.draw(Fx::DrawContext(now, leds));
    }

    uint32_t frame_time = millis() - now;
    FASTLED_WARN("Frame time: " << frame_time << "ms");

    FastLED.show();
}