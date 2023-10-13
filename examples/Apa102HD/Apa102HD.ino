/// @file    Apa102HD.ino
/// @brief   Example showing how to use the APA102HD gamma correction.
///
///          In this example we compare two strips of LEDs.
///          One strip is in HD mode, the other is in software gamma mode.
///
///          Each strip is a linear ramp of brightnesses, from 0 to 255.
///          Showcasing all the different brightnesses.
///
///          Why do we love gamma correction? Gamma correction more closely
///          matches how humans see light. Led values are measured in fractions
///          of max power output (1/255, 2/255, etc.), while humans see light
///          in a logarithmic way. Gamma correction converts to this eye friendly
///          curve. Gamma correction wants a LED with a high bit depth. The APA102
///          gives us the standard 3 components (red, green, blue) with 8 bits each, it
///          *also* has a 5 bit brightness component. This gives us a total of 13 bits,
///          which allows us to achieve a higher dynamic range. This means deeper fades.
///
///          Example:
///            CRGB leds[NUM_LEDS] = {0};
///            void setup() {
///              FastLED.addLeds<
///                APA102HD, // <--- This selects HD mode.
///                STRIP_0_DATA_PIN,
///                STRIP_0_CLOCK_PIN,
///                RGB
///              >(leds, NUM_LEDS);
///            }


#include <Arduino.h>
#include <FastLED.h>
#include <lib8tion.h>

#define NUM_LEDS  20
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
    // Draw a a linear ramp of brightnesses to showcase the difference between
    // the HD and non-HD mode.
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = map(i, 0, NUM_LEDS - 1, 0, 255);
        CRGB c(brightness, brightness, brightness);  // Just make a shade of white.
        leds_hd[i] = c;  // The APA102HD leds do their own gamma correction.
        CRGB c_gamma_corrected = software_gamma(c);
        leds[i] = c_gamma_corrected;  // Set the software gamma corrected
                                      // values to the other strip.
    }
    FastLED.show();  // All leds are now written out.
    delay(8);  // Wait 8 milliseconds until the next frame.
}

