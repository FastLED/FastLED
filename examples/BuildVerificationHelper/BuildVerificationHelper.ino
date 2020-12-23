#include "FastLED.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// !!! WARNING: THIS IS NOT INTENDED TO BE A FUNCTIONING SKETCH. !!!
//
// This instantiates LED controllers of every potential type.
// It re-uses the ***SAME*** pins for each of the LED controllers,
// which obviously does not correlate with a real-life configuration.
//
// This is used SOLELY for the purpose of helping verify that the
// library builds for various platform / controller combinations
// (at least with the noted pin).
//
// A later goal may be to find a way to enable conditional compilation,
// based on whether a pin is valid.  Unfortunately, while
// 'constexpr static bool FastPin<uint_8 PIN>:ValidPin()' is defined,
// it cannot be used in the following way:
//
// #if FastPin<24>:ValidPin()
//     FastLED.addLeds<TM1803, 24, RGB>(leds, NUM_LEDS);
// #endif
//
// This is unfortunate, but of course if desired, it would not be
// difficult to add additional per-board controller combinations...
// 
////////////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_LEDS  6
#define DATA_PIN  3
#define CLOCK_PIN 2 // Clock pin only needed for SPI based chipsets when not using hardware SPI

CRGB leds[NUM_LEDS];

void setup() {
    // sanity check delay - allows reprogramming if accidently blowing power w/leds
    delay(2000);

    // Clockless
    FastLED.addLeds<TM1803,     DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<TM1804,     DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<TM1809,     DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<WS2811,     DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<WS2812,     DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<WS2812B,    DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.addLeds<GW6205,     DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<GW6205_400, DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<UCS1903,    DATA_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<UCS1903B,   DATA_PIN, RGB>(leds, NUM_LEDS);

    // Software SPI
    FastLED.addLeds<LPD6803,    DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<LPD8806,    DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<WS2801,     DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<WS2803,     DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<SM16716,    DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<P9813,      DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<APA102,     DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<SK9822,     DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<DOTSTAR,    DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    
#ifdef SPI_DATA 
    // Hardware SPI (DATA_PIN and CLOCK_PIN are optional for hardware SPI)
    FastLED.addLeds<LPD6803,    RGB>(leds, NUM_LEDS);
    FastLED.addLeds<LPD8806,    RGB>(leds, NUM_LEDS);
    FastLED.addLeds<WS2801,     RGB>(leds, NUM_LEDS);
    FastLED.addLeds<WS2803,     RGB>(leds, NUM_LEDS);
    FastLED.addLeds<SM16716,    RGB>(leds, NUM_LEDS);
    FastLED.addLeds<P9813,      RGB>(leds, NUM_LEDS);
    FastLED.addLeds<APA102,     RGB>(leds, NUM_LEDS);
    FastLED.addLeds<SK9822,     RGB>(leds, NUM_LEDS);
    FastLED.addLeds<DOTSTAR,    RGB>(leds, NUM_LEDS);
#endif SPI_DATA    

    FastLED.setBrightness(CRGB(255,255,255));
}

void loop() {
    leds[0] = CRGB(255,0,0);
    leds[1] = CRGB(0,255,0);
    leds[2] = CRGB(0,255,0);
    leds[3] = CRGB(0,0,255);
    leds[4] = CRGB(0,0,255);
    leds[5] = CRGB(0,0,255);
    leds[6] = CRGB(0,0,0);
    FastLED.show();
    delay(1000);
}