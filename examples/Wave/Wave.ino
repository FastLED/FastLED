

#define TRY_WAVE_FIX 1

#include "fl/math_macros.h"
#include "fl/ui.h"
#include "fl/wave_simulation.h"
#include <Arduino.h>
#include <FastLED.h>

using namespace fl;

#define NUM_LEDS 100
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];

UITitle title("Wave Demo");
UIDescription description("Shows the use of the Wave1D effect.");

UIButton button("Trigger");
WaveSimulation1D waveSim(NUM_LEDS, SuperSample::SUPER_SAMPLE_2X);

UISlider slider("Speed", 0.18f, 0.0f, 1.0f);
UISlider extraFrames("Extra Frames", 1.0f, 0.0f, 8.0f, 1.0f);
UISlider dampening("Dampening", 6.0f, 0.0f, 10.0f, 0.1f);
UICheckbox positiveOnly("Positive Only", false);
UISlider superSample("SuperSampleExponent", 0.f, 0.f, 3.f, 1.f);

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS);
}

void triggerRipple(WaveSimulation1D &waveSim, int x) {

    for (int i = x - 1; i <= x + 1; i++) {
        if (i < 0 || i >= NUM_LEDS)
            continue;
        FASTLED_WARN("Setting " << i);
        waveSim.set(i, -1.f);
    }
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

void loop() {
    // Your code here
    waveSim.setSpeed(slider);
    waveSim.setDampenening(dampening);
    waveSim.setOnlyPositive(positiveOnly);
    waveSim.setSuperSample(getSuperSample());
    static int x = 0;
    if (button.clicked()) {
        x = random() % NUM_LEDS;
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