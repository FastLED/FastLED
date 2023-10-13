/// @file    Apa102HD.ino
/// @brief   Example showing how to use the APA102HD gamma correction.
///
///          We will draw a linear ramp between APA102HD which does it's
///          own gamma correction at the driver level, and the software
///          version of gamma correction, which used to be the status quo.
///
///          Why do we love gamma correction? It produces a better picture
///          that matches how our eye sees light. It's the standard lighting
///          method for your monitor or TVs for example and gives these
///          devices the range to produce very bright and very dim lights.
/// @example Apa102HD.ino

#include <Arduino.h>
#include <FastLED.h>
#include <lib8tion.h>

#define NUM_LEDS  256
// uint8_t DATA_PIN, uint8_t CLOCK_PIN,
#define STRIP_0_DATA_PIN 1
#define STRIP_0_CLOCK_PIN 2
#define STRIP_1_DATA_PIN 3
#define STRIP_1_CLOCK_PIN 4

CRGB leds_hd[NUM_LEDS] = {0};  // HD mode implies gamma.
CRGB leds[NUM_LEDS] = {0};     // Software gamma mode.

// This is the regular gamma correction function that we used to have
// to do. It's used here to showcase the difference between APA102HD
// mode which does the gamma correction for you.
CRGB software_gamma(const CRGB& in) {
    CRGB out;
    // dim8_raw are the old gamma correction functions.
    out.r = dim8_raw(in.r);
    out.g = dim8_raw(in.g);
    out.b = dim8_raw(in.b);
    return out;
}

void setup() {
    delay(500); // power-up safety delay
    // Two strips of LEDs, one in HD mode, one in software gamma mode.
    FastLED.addLeds<APA102HD, STRIP_0_DATA_PIN, STRIP_0_CLOCK_PIN, RGB>(leds_hd, NUM_LEDS);
    FastLED.addLeds<APA102,   STRIP_1_DATA_PIN, STRIP_1_CLOCK_PIN, RGB>(leds, NUM_LEDS);
}

uint8_t wrap_8bit(int i) {
    // Module % operator here wraps a large "i" so that it is
    // always in [0, 255] range when returned. For example, if
    // "i" is 256, then this will return 0. If "i" is 257
    // then this will return 1. No matter how big the "i" is, the
    // output range will always be [0, 255]
    return i % 256;
}

void loop() {
    // Draw a a linear ramp of brightnesses, showcasing the difference
    // between the two gamma correction modes. APA102HD should have far
    // greater range of brightnesses. Gamma correction mode in software
    // will have about 25% of the dimmest lights truncated to 0 brightness!
    for (int i = 0; i < NUM_LEDS; i++) {
        // Make sure the i value is always in 8 bit range.
        uint8_t brightness = wrap_8bit(i);
        CRGB c(brightness, brightness, brightness);  // Just make a shade of white.
        leds_hd[i] = c;  // The APA102HD leds are set to ungamma corrected values.
        CRGB c_gamma_corrected = software_gamma(c);
        leds[i] = c_gamma_corrected;  // Set the software gamma corrected
                                      // values to the other strip.
    }
    FastLED.show();  // All leds are now written out.
    delay(8);  // Wait 8 milliseconds until the next frame.
}

