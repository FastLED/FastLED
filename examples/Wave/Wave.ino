

#define TRY_WAVE_FIX 1

#include "fl/ui.h"
#include "fl/wave_simulation.h"
#include <Arduino.h>
#include <FastLED.h>

using namespace fl;

#define NUM_LEDS 200
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];

UITitle title("Wave Demo");
UIDescription description("Shows the use of the Wave1D effect.");

UIButton button("Trigger");
using MyWaveSimulation1D = fl::WaveSimulation1D<NUM_LEDS>;
MyWaveSimulation1D waveSim;


void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS);
}

void triggerRipple(MyWaveSimulation1D &waveSim) {
    int x = random() % NUM_LEDS;
    waveSim.set(x, 1.f);
}

void loop() {
    // Your code here
    if (button) {
        triggerRipple(waveSim);
    }
    waveSim.update();

    for (int x = 0; x < NUM_LEDS; x++) {
        float value = waveSim.get(x);
        uint8_t value8 = static_cast<uint8_t>(value * 255);
        leds[x] = CRGB(value8, value8, value8);
        // FASTLED_WARN("leds[" << x << "] = " << value8);
    }

    FastLED.show();
}