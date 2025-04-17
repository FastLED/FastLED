

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
#include "fx/2d/blend.h"



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
UICheckbox easeModeSqrt("Ease Mode Sqrt", false);
UISlider blurAmount("Blur Amount", 0, 0, 172, 1);
// UISlider blurPasses("Blur Passes", 1, 1, 10, 1);


UISlider speedUpper("Wave Upper: Speed", 0.12f, 0.0f, 1.0f);
UISlider dampeningUpper("Wave Upper: Dampening", 8.9f, 0.0f, 20.0f, 0.1f);
UICheckbox halfDuplexUpper("Wave Upper: Half Duplex", true);
UISlider superSampleUpper("Wave Upper: SuperSampleExponent", 1.f, 0.f, 3.f, 1.f);


UISlider speedLower("Wave Lower: Speed", 0.26f, 0.0f, 1.0f);
UISlider dampeningLower("Wave Lower: Dampening", 9.0f, 0.0f, 20.0f, 0.1f);
UICheckbox halfDuplexLower("Wave Lower: Half Duplex", true);
UISlider superSampleLower("Wave Lower: SuperSampleExponent", 1.f, 0.f, 3.f, 1.f);

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   // Black
    32,  0,   0,   70,  // Dark blue
    128, 20,  57,  255, // Electric blue
    255, 255, 255, 255  // White
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    0,   0,   0,   0,  // black
    8,  128, 64, 64, // green
    16, 255, 222, 222, // red
    64, 255, 255, 255, // white
    255, 255, 255, 255 // white
};

XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);
XYMap xyRect(WIDTH, HEIGHT, false);
WaveFx waveFxLower(xyRect, WaveFx::Args{
    .factor = SUPER_SAMPLE_4X,
    .half_duplex = true,
    .speed = 0.18f,
    .dampening = 9.0f,
    .crgbMap = WaveCrgbGradientMapPtr::New(electricBlueFirePal),
});

WaveFx waveFxUpper(xyRect, WaveFx::Args{
    .factor = SUPER_SAMPLE_4X,
    .half_duplex = true,
    .speed = 0.25f,
    .dampening = 3.0f,
    .crgbMap = WaveCrgbGradientMapPtr::New(electricGreenFirePal),
});

Blend2d fxBlend(xyMap);

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(xyMap);
    fxBlend.add(waveFxLower);
    fxBlend.add(waveFxUpper);
}

SuperSample getSuperSample() {
    switch (int(superSampleLower)) {
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

void triggerRipple() {
    int x = random() % WIDTH;
    int y = random() % HEIGHT;
    waveFxLower.setf(x, y, 1);
    waveFxUpper.setf(x, y, 1);
}

bool ui() {
    U8EasingFunction easeMode = easeModeSqrt ? U8EasingFunction::WAVE_U8_MODE_SQRT : U8EasingFunction::WAVE_U8_MODE_LINEAR;
    waveFxLower.setSpeed(speedLower);
    waveFxLower.setDampening(dampeningLower);
    waveFxLower.setHalfDuplex(halfDuplexLower);
    waveFxLower.setSuperSample(getSuperSample());
    waveFxLower.setEasingMode(easeMode);

    waveFxUpper.setSpeed(speedUpper);
    waveFxUpper.setDampening(dampeningUpper);
    waveFxUpper.setHalfDuplex(halfDuplexUpper);
    waveFxUpper.setSuperSample(getSuperSample());
    waveFxUpper.setEasingMode(easeMode);
    fxBlend.setBlurAmount(blurAmount);
    //fxBlend.setBlurPasses(blurPasses);
    return button;
}

void loop() {
    // Your code here
    bool triggered = ui();
    if (triggered) {
        triggerRipple();
    }

    EVERY_N_MILLISECONDS_RANDOM(400, 2000) {
        if (autoTrigger) {
            triggerRipple();
        }
    }

    Fx::DrawContext ctx(millis(), leds);
    fxBlend.draw(ctx);
    FastLED.show();
}