

#define TRY_WAVE_FIX 1

#include "fl/ui.h"
#include "fl/wave_simulation.h"
#include "fl/math_macros.h"
#include <Arduino.h>
#include <FastLED.h>

using namespace fl;

#define NUM_LEDS 100
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];

UITitle title("Wave Demo");
UIDescription description("Shows the use of the Wave1D effect.");

UIButton button("Trigger");
WaveSimulation1D waveSim(NUM_LEDS);

UISlider slider("Speed", 0.18f, 0.0f, 1.0f);
UISlider extraFrames("Extra Frames", 1.0f, 0.0f, 8.0f, 1.0f);
UISlider dampening("Dampening", 6.0f, 0.0f, 10.0f, 0.1f);

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

void loop() {
    // Your code here
    waveSim.setSpeed(slider);
    waveSim.setDampenening(dampening);

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
        int16_t value16 = waveSim.geti16(x);
        uint8_t value8 = map(value16, -32768, 32767, 0, 255);
        leds[x] = CRGB(value8, value8, value8);
    }
    FastLED.show();
}