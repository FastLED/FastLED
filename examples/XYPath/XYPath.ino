

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

UIButton button("Trigger");
UISlider scale("Scale", 1.0f, 0.0f, 1.0f, 0.01f);

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
                    .factor = SUPER_SAMPLE_4X,
                    .half_duplex = true,
                    .speed = 0.18f,
                    .dampening = 9.0f,
                    .crgbMap = WaveCrgbGradientMapPtr::New(electricBlueFirePal),
                });

WaveFx waveFxUpper(
    xyRect, WaveFx::Args{
                .factor = SUPER_SAMPLE_4X,
                .half_duplex = true,
                .speed = 0.25f,
                .dampening = 3.0f,
                .crgbMap = WaveCrgbGradientMapPtr::New(electricGreenFirePal),
            });

Blend2d fxBlend(xyMap);


XYPathPtr shape = XYPath::NewLinePath(0, 0, WIDTH, HEIGHT);
TimeLinear pointTransition(10000);

void setup() {
    Serial.begin(115200);
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(screenmap);
    fxBlend.add(waveFxLower);
    fxBlend.add(waveFxUpper);
}

void loop() {
    // Your code here
    uint32_t now = millis();
    memset(leds, 0, NUM_LEDS * sizeof(CRGB));

    // FASTLED_WARN("Now: " << now);

    shape->setScale(scale.value());

    // unconditionally apply the circle.
    if (button) {
        // trigger the transition
        pointTransition.trigger(now);
        FASTLED_WARN("Transition triggered");
    }

    if (pointTransition.isActive(now)) {
        const CRGB white = CRGB(32, 32, 32);
        const CRGB black = CRGB(0, 0, 0);
        float curr_alpha = pointTransition.updatef(now);
        Tile3x3<float> tile;
        auto xy = shape->at_gaussian(curr_alpha, &tile);
        // waveFxLower.addf(xy.x, xy.y, 1.0f);
        // waveFxUpper.addf(xy.x, xy.y, 1.0f);

        for (int x = 0; x <= 2; ++x) {
            int xx = xy.x + x - 1;
            if (xx < 0 || xx >= WIDTH) {
                continue;
            }
            for (int y = 0; y <= 2; ++y) {
                int yy = xy.y + y -1;
                if (yy < 0 || yy >= HEIGHT) {
                    continue;
                }
                float val = tile.at(x, y);
                FASTLED_WARN("x: " << x << " y: " << y << " val: " << val);
                // waveFxLower.addf(xy.x, xy.y, val);
                // waveFxUpper.addf(xy.x, xy.y, val);
                if (xyMap.has(xx,yy)) {
                    int idx = xyMap(xx,yy);
                    CRGB color = white.lerp8(black, val * 255);
                    leds[idx] = color;
                }
            }
        }
    }

    //fxBlend.draw(Fx::DrawContext(now, leds));
    FastLED.show();
}