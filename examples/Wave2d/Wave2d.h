/// @file    Wave2d.ino
/// @brief   2D wave effect demonstration
/// @example Wave2d.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

// Author: Stefan Petrick

/*
This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.
*/

#include "fl/ui.h"
#include "fl/fx/2d/wave.h"
#include <Arduino.h>
#include <FastLED.h>


#define HEIGHT 100
#define WIDTH 100
#define NUM_LEDS ((WIDTH) * (HEIGHT))
#define IS_SERPINTINE true

fl::CRGB leds[NUM_LEDS];

fl::UITitle title("Wave2D Demo");
fl::UIDescription description("Shows the use of the Wave2d effect. By default the wave is cyclical on the x-axis and waves will spill over to the other side.");

fl::UIButton button("Trigger");
fl::UICheckbox xCyclical("X Is Cyclical", true);  // The wave keeps on propagating across the x-axis, when true.

fl::UICheckbox autoTrigger("Auto Trigger", true);
fl::UISlider extraFrames("Extra Frames", 0.0f, 0.0f, 8.0f, 1.0f);
fl::UISlider slider("Speed", 0.18f, 0.0f, 1.0f);
fl::UISlider dampening("Dampening", 9.0f, 0.0f, 20.0f, 0.1f);
fl::UICheckbox halfDuplex("Half Duplex", true);
fl::UISlider superSample("SuperSampleExponent", 1.f, 0.f, 3.f, 1.f);

// Group related UI elements using fl::UIGroup template multi-argument constructor
fl::UIGroup waveSimControls("Wave Simulation", slider, dampening, halfDuplex, superSample, xCyclical);
fl::UIGroup triggerControls("Trigger Controls", button, autoTrigger, extraFrames);

fl::WaveSimulation2D waveSim(WIDTH, HEIGHT, fl::SUPER_SAMPLE_4X);

fl::XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);



fl::SuperSample getSuperSample() {
    switch (int(superSample)) {
    case 0:
        return fl::SuperSample::SUPER_SAMPLE_NONE;
    case 1:
        return fl::SuperSample::SUPER_SAMPLE_2X;
    case 2:
        return fl::SuperSample::SUPER_SAMPLE_4X;
    case 3:
        return fl::SuperSample::SUPER_SAMPLE_8X;
    default:
        return fl::SuperSample::SUPER_SAMPLE_NONE;
    }
}

void triggerRipple(fl::WaveSimulation2D &waveSim) {
    int x = random(WIDTH);
    int y = random(HEIGHT);
    waveSim.setf(x, y, 1);
}

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(xyMap);
}

void loop() {
    // Your code here
    waveSim.setXCylindrical(xCyclical.value());
    waveSim.setSpeed(slider);
    waveSim.setDampening(dampening);
    waveSim.setHalfDuplex(halfDuplex);
    waveSim.setSuperSample(getSuperSample());
    if (button) {
        triggerRipple(waveSim);
    }


    EVERY_N_MILLISECONDS(400) {
        if (autoTrigger) {
            triggerRipple(waveSim);
        }
    }

    waveSim.update();
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            uint8_t value8 = waveSim.getu8(x, y);
            uint32_t idx = xyMap.mapToIndex(x, y);
            leds[idx] = fl::CRGB(value8, value8, value8);
        }
    }
    FastLED.show();
}
