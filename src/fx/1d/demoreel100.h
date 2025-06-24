#pragma once

#include "FastLED.h"
#include "fl/namespace.h"
#include "fx/fx1d.h"

namespace fl {

// FastLED "100-lines-of-code" demo reel, showing just a few
// of the kinds of animation patterns you can quickly and easily
// compose using FastLED.
//
// This example also shows one easy way to define multiple
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014

FASTLED_SMART_PTR(DemoReel100);

class DemoReel100 : public Fx1d {
  public:
    DemoReel100(uint16_t num_leds) : Fx1d(num_leds) {}

    void draw(DrawContext context) override {
        CRGB *leds = context.leds;
        if (leds == nullptr || mNumLeds == 0) {
            return;
        }
        if (start_time == 0) {
            start_time = millis();
        }

        // Call the current pattern function once, updating the 'leds' array
        runPattern(leds);

        // do some periodic updates
        EVERY_N_MILLISECONDS(20) {
            hue++;
        } // slowly cycle the "base color" through the rainbow
        EVERY_N_SECONDS(10) { nextPattern(); } // change patterns periodically
    }

    fl::string fxName() const override { return "DemoReel100"; }

  private:
    uint8_t current_pattern_number = 0;
    uint8_t hue = 0;
    unsigned long start_time = 0;

    void nextPattern() {
        // add one to the current pattern number, and wrap around at the end
        current_pattern_number =
            (current_pattern_number + 1) % 6; // 6 is the number of patterns
    }

    void runPattern(CRGB *leds) {
        switch (current_pattern_number) {
        case 0:
            rainbow(leds);
            break;
        case 1:
            rainbowWithGlitter(leds);
            break;
        case 2:
            confetti(leds);
            break;
        case 3:
            sinelon(leds);
            break;
        case 4:
            juggle(leds);
            break;
        case 5:
            bpm(leds);
            break;
        }
    }

    void rainbow(CRGB *leds) {
        // FastLED's built-in rainbow generator
        fill_rainbow(leds, mNumLeds, hue, 7);
    }

    void rainbowWithGlitter(CRGB *leds) {
        // built-in FastLED rainbow, plus some random sparkly glitter
        rainbow(leds);
        addGlitter(80, leds);
    }

    void addGlitter(fract8 chanceOfGlitter, CRGB *leds) {
        if (random8() < chanceOfGlitter) {
            leds[random16(mNumLeds)] += CRGB::White;
        }
    }

    void confetti(CRGB *leds) {
        // random colored speckles that blink in and fade smoothly
        fadeToBlackBy(leds, mNumLeds, 10);
        int pos = random16(mNumLeds);
        leds[pos] += CHSV(hue + random8(64), 200, 255);
    }

    void sinelon(CRGB *leds) {
        // a colored dot sweeping back and forth, with fading trails
        fadeToBlackBy(leds, mNumLeds, 20);
        int pos = beatsin16(13, 0, mNumLeds - 1);
        leds[pos] += CHSV(hue, 255, 192);
    }

    void bpm(CRGB *leds) {
        // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
        uint8_t BeatsPerMinute = 62;
        CRGBPalette16 palette = PartyColors_p;
        uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
        for (uint16_t i = 0; i < mNumLeds; i++) {
            leds[i] =
                ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
        }
    }

    void juggle(CRGB *leds) {
        // eight colored dots, weaving in and out of sync with each other
        fadeToBlackBy(leds, mNumLeds, 20);
        uint8_t dothue = 0;
        for (uint16_t i = 0; i < 8; i++) {
            leds[beatsin16(i + 7, 0, mNumLeds - 1)] |= CHSV(dothue, 200, 255);
            dothue += 32;
        }
    }
};

} // namespace fl
