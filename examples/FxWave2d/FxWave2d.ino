

/*
This demo is best viewed using the FastLED compiler.
Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

#include <Arduino.h>
#include <FastLED.h>

#include "fl/ui.h"
#include "fx/2d/wave.h"



using namespace fl;

#define HEIGHT 100
#define WIDTH 100
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];

UITitle title("Wave2D Demo");
UIDescription description("Shows the use of the Wave2d effect.");

UIButton button("Trigger");
UICheckbox autoTrigger("Auto Trigger", true);
UISlider extraFrames("Extra Frames", 0.0f, 0.0f, 8.0f, 1.0f);
UISlider slider("Speed", 0.18f, 0.0f, 1.0f);
UISlider dampening("Dampening", 9.0f, 0.0f, 20.0f, 0.1f);
UICheckbox halfDuplex("Half Duplex", true);
UISlider superSample("SuperSampleExponent", 1.f, 0.f, 3.f, 1.f);


DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   // Black
    32,  0,   0,   70,  // Dark blue
    128, 20,  57,  255, // Electric blue
    255, 255, 255, 255  // White
};

XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);
WaveFx waveFx(xyMap, WaveFx::Args{
    .factor = SUPER_SAMPLE_4X,
    .half_duplex = true,
    .speed = 0.18f,
    .dampening = 9.0f,
    .crgbMap = WaveCrgbGradientMapPtr::New(electricBlueFirePal),
    
});

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(xyMap);
}

SuperSample getSuperSample() {
    switch (int(superSample)) {
    case 0:
        return SuperSample::SUPER_SAMPLE_NONE;
    case 1:
        return SuperSample::SUPER_SAMPLE_2X;
    case 2:
        return SuperSample::SUPER_SAMPLE_4X;
    case 3:
        return SuperSample::SUPER_SAMPLE_8X;
    default:
        return SuperSample::SUPER_SAMPLE_NONE;
    }
}

void triggerRipple(WaveFx &waveFx) {
    int x = random() % WIDTH;
    int y = random() % HEIGHT;
    for (int i = x - 1; i <= x + 1; i++) {
        if (i < 0 || i >= WIDTH)
            continue;
        for (int j = y - 1; j <= y + 1; j++) {
            if (j < 0 || j >= HEIGHT)
                continue;
            waveFx.set(i, j, 1);
        }
    }
    waveFx.set(x, y, 1);
}

void loop() {
    // Your code here
    waveFx.setSpeed(slider);
    waveFx.setDampening(dampening);
    waveFx.setHalfDuplex(halfDuplex);
    waveFx.setSuperSample(getSuperSample());
    if (button) {
        triggerRipple(waveFx);
    }

    EVERY_N_MILLISECONDS_RANDOM(400, 2000) {
        if (autoTrigger) {
            triggerRipple(waveFx);
        }
    }

    Fx::DrawContext ctx(millis(), leds);
    waveFx.draw(ctx);
    FastLED.show();
}