#pragma once

#include <FastLED.h>

FASTLED_USING_NAMESPACE

// FastLED "100-lines-of-code" demo reel, showing just a few 
// of the kinds of animation patterns you can quickly and easily 
// compose using FastLED.  
//
// This example also shows one easy way to define multiple 
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014

struct DemoReel100Data {
    CRGB* leds = nullptr;
    uint16_t num_leds = 0;
    uint8_t brightness = 96;
    uint8_t current_pattern_number = 0;
    uint8_t hue = 0;

    // constructor
    DemoReel100Data(CRGB* leds, uint16_t num_leds, uint8_t brightness = 96)
        : leds(leds), num_leds(num_leds), brightness(brightness) {}
};

// Function prototypes
void rainbow(DemoReel100Data& self);
void rainbowWithGlitter(DemoReel100Data& self);
void addGlitter(DemoReel100Data& self, fract8 chanceOfGlitter);
void confetti(DemoReel100Data& self);
void sinelon(DemoReel100Data& self);
void bpm(DemoReel100Data& self);
void juggle(DemoReel100Data& self);

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])(DemoReel100Data&);
SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern(DemoReel100Data& self)
{
    // add one to the current pattern number, and wrap around at the end
    self.current_pattern_number = (self.current_pattern_number + 1) % ARRAY_SIZE(gPatterns);
}

void DemoReel100Loop(DemoReel100Data& self)
{
    // Call the current pattern function once, updating the 'leds' array
    gPatterns[self.current_pattern_number](self);


    // do some periodic updates
    EVERY_N_MILLISECONDS( 20 ) { self.hue++; } // slowly cycle the "base color" through the rainbow
    EVERY_N_SECONDS( 10 ) { nextPattern(self); } // change patterns periodically
}

void rainbow(DemoReel100Data& self) 
{
    // FastLED's built-in rainbow generator
    fill_rainbow(self.leds, self.num_leds, self.hue, 7);
}

void rainbowWithGlitter(DemoReel100Data& self) 
{
    // built-in FastLED rainbow, plus some random sparkly glitter
    rainbow(self);
    addGlitter(self, 80);
}

void addGlitter(DemoReel100Data& self, fract8 chanceOfGlitter) 
{
    if(random8() < chanceOfGlitter) {
        self.leds[random16(self.num_leds)] += CRGB::White;
    }
}

void confetti(DemoReel100Data& self) 
{
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(self.leds, self.num_leds, 10);
    int pos = random16(self.num_leds);
    self.leds[pos] += CHSV(self.hue + random8(64), 200, 255);
}

void sinelon(DemoReel100Data& self)
{
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy(self.leds, self.num_leds, 20);
    int pos = beatsin16(13, 0, self.num_leds-1);
    self.leds[pos] += CHSV(self.hue, 255, 192);
}

void bpm(DemoReel100Data& self)
{
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for(int i = 0; i < self.num_leds; i++) {
        self.leds[i] = ColorFromPalette(palette, self.hue+(i*2), beat-self.hue+(i*10));
    }
}

void juggle(DemoReel100Data& self) {
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy(self.leds, self.num_leds, 20);
    uint8_t dothue = 0;
    for(int i = 0; i < 8; i++) {
        self.leds[beatsin16(i+7, 0, self.num_leds-1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}

