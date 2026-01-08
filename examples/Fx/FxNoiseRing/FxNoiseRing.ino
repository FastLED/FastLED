// @filter: (memory is high)

/// @file    FxNoiseRing.ino
/// @brief   Noise effect on circular ring with ScreenMap
/// @example FxNoiseRing.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

// PCH-compatible include order: Arduino.h and FastLED.h must come first
#include <Arduino.h>
#include "FastLED.h"

// Now we can include other headers and do platform checks
#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/stl/math.h"
#include "fl/warn.h"
#include "noisegen.h"
#include "fl/screenmap.h"
#include "fl/slice.h"
#include "fl/ui.h"
#include "fl/sensors/pir.h"
#include "fl/stl/sstream.h"
#include "fl/stl/assert.h"
#include "fl/noise.h"

// Defines come after all includes
#ifndef DATA_PIN
#define DATA_PIN 3
#endif // DATA_PIN

#define COLOR_ORDER GRB  // Color order matters for a real device, web-compiler will ignore this.
#define NUM_LEDS 250
#define PIN_PIR 0

#define PIR_LATCH_MS 60000  // how long to keep the PIR sensor active after a trigger
#define PIR_RISING_TIME 1000  // how long to fade in the PIR sensor
#define PIR_FALLING_TIME 1000  // how long to fade out the PIR sensor

using namespace fl;

CRGB leds[NUM_LEDS];

// These sliders and checkboxes are dynamic when using the FastLED web compiler.
// When deployed to a real device these elements will always be the default value.
UISlider brightness("Brightness", 1, 0, 1);
UISlider scale("Scale", 4, .1, 4, .1);
UISlider timeBitshift("Time Bitshift", 5, 0, 16, 1);
UISlider timescale("Time Scale", 1, .1, 10, .1);
// This PIR type is special because it will bind to a pin for a real device,
// but also provides a UIButton when run in the simulator.
Pir pir(PIN_PIR, PIR_LATCH_MS, PIR_RISING_TIME, PIR_FALLING_TIME);
UICheckbox useDither("Use Binary Dither", true);

float current_brightness = 0;

// Save a pointer to the controller so that we can modify the dither in real time.
CLEDController* controller = nullptr;

void setup() {
    Serial.begin(115200);
    // ScreenMap is purely something that is needed for the sketch to correctly
    // show on the web display. For deployements to real devices, this essentially
    // becomes a no-op.
    ScreenMap xyMap = ScreenMap::Circle(NUM_LEDS, 2.0, 2.0);
    controller = &FastLED.addLeds<WS2811, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
                .setCorrection(TypicalLEDStrip)
                .setDither(DISABLE_DITHER)
                .setScreenMap(xyMap);
    FastLED.setBrightness(brightness);
    pir.activate(fl::millis());  // Activate the PIR sensor on startup.
}

void draw(uint32_t now) {
    double angle_offset = double(now) / 32000.0 * 2 * FL_M_PI;
    now = (now << timeBitshift.as<int>()) * timescale.as<double>();

    // get radius/zoom level from slider
    float noise_radius = scale.as<float>();

    // go in circular formation and set the leds
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = i * 2 * FL_M_PI / NUM_LEDS + angle_offset;

        // Use the new noiseRingHSV8 function to sample three z-slices for HSV components
        CHSV hsv = noiseRingHSV8(angle, now, noise_radius);

        // Apply same constraints as before: minimum saturation and adjusted value
        hsv.s = FL_MAX(128u, (unsigned)hsv.s);
        // Apply value mapping similar to original: map from 0-255 to -64-255, clamp to 0-255
        uint16_t val = hsv.v;
        int16_t adjusted_val = map(val, 0, 255, -64, 255);
        if (adjusted_val < 0) {
            adjusted_val = 0;
        }
        hsv.v = adjusted_val;

        leds[i] = hsv;
    }
}

void loop() {
    // Allow the dither to be enabled and disabled.
    controller->setDither(useDither ? BINARY_DITHER : DISABLE_DITHER);
    uint32_t now = fl::millis();
    uint8_t bri = pir.transition(now);
    FastLED.setBrightness(bri * brightness.as<float>());
    // Apply leds generation to the leds.
    draw(now);

    FastLED.show();
}
