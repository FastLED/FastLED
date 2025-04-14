

#define TRY_WAVE_FIX 1

#include "fl/ui.h"
#include "fx/2d/wave.h"
#include <Arduino.h>
#include <FastLED.h>

using namespace fl;

#define HEIGHT 100
#define WIDTH 100
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true

CRGB leds[NUM_LEDS];

UITitle title("FxWave2D Demo");
UIDescription description("Shows the use of the Wave2d effect.");

UIButton button("Trigger");
WaveSimulation2D waveSim(WIDTH, HEIGHT);

XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(xyMap);
}

void loop() {
    // Your code here
    if (button) {
        int x = random() % WIDTH;
        int y = random() % HEIGHT;
        for (int i = x-1; i <= x+1; i++) {
            if (i < 0 || i >= WIDTH) continue;
            for (int j = y-1; j <= y+1; j++) {
                if (j < 0 || j >= HEIGHT) continue;
                waveSim.set(i, j, 1);
            }
        }
        waveSim.set(x, y, 1);
    }
    waveSim.update();
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            float value = waveSim.getf(x, y);
            uint8_t value8 = static_cast<uint8_t>(value * 255);
            uint32_t idx = xyMap.mapToIndex(x, y);
            leds[idx] = CRGB(value8, value8, value8);
        }
    }
    FastLED.show();
}