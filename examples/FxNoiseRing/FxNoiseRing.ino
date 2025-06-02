

/// @file    NoiseRing.ino
/// @brief   Shows how to use a circular noise generator to have a continuous noise effect on a ring of LEDs.
/// @author  Zach Vorhies
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <Arduino.h>

#include <stdio.h>

#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/warn.h"
#include "noisegen.h"
#include "fl/screenmap.h"
#include "fl/slice.h"
#include "fl/ui.h"
#include "FastLED.h"
#include "sensors/pir.h"
#include "timer.h"

#define LED_PIN 2
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

Timer timer;
float current_brightness = 0;

// Save a pointer to the controller so that we can modify the dither in real time.
CLEDController* controller = nullptr;


void setup() {
    Serial.begin(115200);
    // ScreenMap is purely something that is needed for the sketch to correctly
    // show on the web display. For deployements to real devices, this essentially
    // becomes a no-op.
    ScreenMap xyMap = ScreenMap::Circle(NUM_LEDS, 2.0, 2.0);
    controller = &FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
                .setCorrection(TypicalLEDStrip)
                .setDither(DISABLE_DITHER)
                .setScreenMap(xyMap);
    FastLED.setBrightness(brightness);
    pir.activate(millis());  // Activate the PIR sensor on startup.
}

void draw(uint32_t now) {
    double angle_offset = double(now) / 32000.0 * 2 * M_PI;
    now = (now << timeBitshift.as<int>()) * timescale.as<double>();
    // go in circular formation and set the leds
    for (int i = 0; i < NUM_LEDS; i++) {
        // Sorry this is a little convoluted but we are applying noise
        // in the 16-bit space and then mapping it back to 8-bit space.
        // All the constants were experimentally determined.
        float angle = i * 2 * M_PI / NUM_LEDS + angle_offset;
        float x = cos(angle);
        float y = sin(angle);
        x *= 0xffff * scale.as<double>();
        y *= 0xffff * scale.as<double>();
        uint16_t noise = inoise16(x, y, now);
        uint16_t noise2 = inoise16(x, y, 0xfff + now);
        uint16_t noise3 = inoise16(x, y, 0xffff + now);
        noise3 = noise3 >> 8;
        int16_t noise4 = map(noise3, 0, 255, -64, 255);
        if (noise4 < 0) {  // Clamp negative values to 0.
            noise4 = 0;
        }
        // Shift back to 8-bit space.
        leds[i] = CHSV(noise >> 8, MAX(128, noise2 >> 8), noise4);
    }
}

void loop() {
    // Allow the dither to be enabled and disabled.
    controller->setDither(useDither ? BINARY_DITHER : DISABLE_DITHER);
    uint32_t now = millis();
    uint8_t bri = pir.transition(now);
    FastLED.setBrightness(bri * brightness.as<float>());
    // Apply leds generation to the leds.
    draw(now);
    FastLED.show();
}
