
/*

This is a 1D wave simluation!

This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

#include "fl/math_macros.h"
#include "fl/ui.h"
#include "fl/wave_simulation.h"
#include <Arduino.h>
#include <FastLED.h>

using namespace fl;

#define NUM_LEDS 100
#define IS_SERPINTINE true  // Type of matrix display, you probably have this.

CRGB leds[NUM_LEDS];

UITitle title("Wave Demo");
UIDescription description("Shows the use of the Wave1D effect.");

UIButton button("Trigger");
WaveSimulation1D waveSim(NUM_LEDS, SuperSample::SUPER_SAMPLE_2X);

UISlider slider("Speed", 0.18f, 0.0f, 1.0f);
UISlider extraFrames("Extra Frames", 1.0f, 0.0f, 8.0f, 1.0f);
UISlider dampening("Dampening", 6.0f, 0.0f, 10.0f, 0.1f);
UICheckbox halfDuplex("Half Duplex", false);
UISlider superSample("SuperSampleExponent", 0.f, 0.f, 3.f, 1.f);

void setup() {
    Serial.begin(115200);
    // No ScreenMap necessary for strips.
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS);
}

void triggerRipple(WaveSimulation1D &waveSim, int x) {

    for (int i = x - 1; i <= x + 1; i++) {
        if (i < 0 || i >= NUM_LEDS)
            continue;
        waveSim.setf(i, -1.f);
    }
}

// Wave simulation looks better when you render at a higher resolution then
// downscale the result to the display resolution.
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

void loop() {
    // Allow the waveSimulator to respond to the current slider value each frame.
    waveSim.setSpeed(slider);
    waveSim.setDampening(dampening);
    // Pretty much you always want half duplex to be true, otherwise you get a gray
    // wave effect that doesn't look good.
    waveSim.setHalfDuplex(halfDuplex);
    waveSim.setSuperSample(getSuperSample());
    static int x = 0;
    if (button.clicked()) {
        // If button click then select a random position in the wave.
        x = random(NUM_LEDS);
    }
    if (button.isPressed()) {
        FASTLED_WARN("Button is pressed at " << x);
        triggerRipple(waveSim, x);
    }
    waveSim.update();
    for (int i = 0; i < extraFrames.value(); i++) {
        waveSim.update();
    }
    for (int x = 0; x < NUM_LEDS; x++) {
        // float value = waveSim.get(x);
        uint8_t value8 = waveSim.getu8(x);
        leds[x] = CRGB(value8, value8, value8);
    }
    FastLED.show();
}
