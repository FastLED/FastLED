#pragma once

#include "FastLED.h"
#include "fl/namespace.h"
#include "fl/str.h"
#include "fx/fx1d.h"

namespace fl {

/// @file    TwinkleFox.hpp
/// @brief   Twinkling "holiday" lights that fade in and out.
/// @example TwinkleFox.ino

//  TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
//  Colors are chosen from a palette; a few palettes are provided.
//
//  This December 2015 implementation improves on the December 2014 version
//  in several ways:
//  - smoother fading, compatible with any colors and any palettes
//  - easier control of twinkle speed and twinkle density
//  - supports an optional 'background color'
//  - takes even less RAM: zero RAM overhead per pixel
//  - illustrates a couple of interesting techniques (uh oh...)
//
//  The idea behind this (new) implementation is that there's one
//  basic, repeating pattern that each pixel follows like a waveform:
//  The brightness rises from 0..255 and then falls back down to 0.
//  The brightness at any given point in time can be determined as
//  as a function of time, for example:
//    brightness = sine( time ); // a sine wave of brightness over time
//
//  So the way this implementation works is that every pixel follows
//  the exact same wave function over time.  In this particular case,
//  I chose a sawtooth triangle wave (triwave8) rather than a sine wave,
//  but the idea is the same: brightness = triwave8( time ).
//
//  The triangle wave function is a piecewise linear function that looks like:
//
//     / \\ 
//    /     \\ 
//   /         \\ 
//  /             \\ 
//
//  Of course, if all the pixels used the exact same wave form, and
//  if they all used the exact same 'clock' for their 'time base', all
//  the pixels would brighten and dim at once -- which does not look
//  like twinkling at all.
//
//  So to achieve random-looking twinkling, each pixel is given a
//  slightly different 'clock' signal.  Some of the clocks run faster,
//  some run slower, and each 'clock' also has a random offset from zero.
//  The net result is that the 'clocks' for all the pixels are always out
//  of sync from each other, producing a nice random distribution
//  of twinkles.
//
//  The 'clock speed adjustment' and 'time offset' for each pixel
//  are generated randomly.  One (normal) approach to implementing that
//  would be to randomly generate the clock parameters for each pixel
//  at startup, and store them in some arrays.  However, that consumes
//  a great deal of precious RAM, and it turns out to be totally
//  unnessary!  If the random number generate is 'seeded' with the
//  same starting value every time, it will generate the same sequence
//  of values every time.  So the clock adjustment parameters for each
//  pixel are 'stored' in a pseudo-random number generator!  The PRNG
//  is reset, and then the first numbers out of it are the clock
//  adjustment parameters for the first pixel, the second numbers out
//  of it are the parameters for the second pixel, and so on.
//  In this way, we can 'store' a stable sequence of thousands of
//  random clock adjustment parameters in literally two bytes of RAM.
//
//  There's a little bit of fixed-point math involved in applying the
//  clock speed adjustments, which are expressed in eighths.  Each pixel's
//  clock speed ranges from 8/8ths of the system clock (i.e. 1x) to
//  23/8ths of the system clock (i.e. nearly 3x).
//
//  On a basic Arduino Uno or Leonardo, this code can twinkle 300+ pixels
//  smoothly at over 50 updates per seond.
//
//  -Mark Kriegsman, December 2015

//  Adapted for FastLED 3.x in August 2023 by Marlin Unruh

// TwinkleFox effect parameters
// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).
// 4, 5, and 6 are recommended, default is 4.
#define TWINKLE_SPEED 4

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).
// Default is 5.
#define TWINKLE_DENSITY 5

// How often to change color palettes.
#define SECONDS_PER_PALETTE 30

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 0

// If COOL_LIKE_INCANDESCENT is set to 1, colors will
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 1

FASTLED_SMART_PTR(TwinkleFox);

class TwinkleFox : public Fx1d {
  public:
    CRGBPalette16 targetPalette;
    CRGBPalette16 currentPalette;

    TwinkleFox(uint16_t num_leds)
        : Fx1d(num_leds), backgroundColor(CRGB::Black),
          twinkleSpeed(TWINKLE_SPEED), twinkleDensity(TWINKLE_DENSITY),
          coolLikeIncandescent(COOL_LIKE_INCANDESCENT),
          autoSelectBackgroundColor(AUTO_SELECT_BACKGROUND_COLOR) {
        chooseNextColorPalette(targetPalette);
    }

    void draw(DrawContext context) override {
        EVERY_N_MILLISECONDS(10) {
            nblendPaletteTowardPalette(currentPalette, targetPalette, 12);
        }
        drawTwinkleFox(context.leds);
    }

    void chooseNextColorPalette(CRGBPalette16 &pal);
    fl::string fxName() const override { return "TwinkleFox"; }

  private:
    CRGB backgroundColor;
    uint8_t twinkleSpeed;
    uint8_t twinkleDensity;
    bool coolLikeIncandescent;
    bool autoSelectBackgroundColor;

    void drawTwinkleFox(CRGB *leds) {
        // "PRNG16" is the pseudorandom number generator
        // It MUST be reset to the same starting value each time
        // this function is called, so that the sequence of 'random'
        // numbers that it generates is (paradoxically) stable.
        uint16_t PRNG16 = 11337;
        fl::u32 clock32 = millis();

        CRGB bg = backgroundColor;
        if (autoSelectBackgroundColor &&
            currentPalette[0] == currentPalette[1]) {
            bg = currentPalette[0];
            uint8_t bglight = bg.getAverageLight();
            if (bglight > 64) {
                bg.nscale8_video(16);
            } else if (bglight > 16) {
                bg.nscale8_video(64);
            } else {
                bg.nscale8_video(86);
            }
        }

        uint8_t backgroundBrightness = bg.getAverageLight();

        for (uint16_t i = 0; i < mNumLeds; i++) {
            PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384;
            uint16_t myclockoffset16 = PRNG16;
            PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384;
            uint8_t myspeedmultiplierQ5_3 =
                ((((PRNG16 & 0xFF) >> 4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
            fl::u32 myclock30 =
                (fl::u32)((clock32 * myspeedmultiplierQ5_3) >> 3) +
                myclockoffset16;
            uint8_t myunique8 = PRNG16 >> 8;

            CRGB c = computeOneTwinkle(myclock30, myunique8);

            uint8_t cbright = c.getAverageLight();
            int16_t deltabright = cbright - backgroundBrightness;
            if (deltabright >= 32 || (!bg)) {
                leds[i] = c;
            } else if (deltabright > 0) {
                leds[i] = blend(bg, c, deltabright * 8);
            } else {
                leds[i] = bg;
            }
        }
    }

    CRGB computeOneTwinkle(fl::u32 ms, uint8_t salt) {
        uint16_t ticks = ms >> (8 - twinkleSpeed);
        uint8_t fastcycle8 = ticks;
        uint16_t slowcycle16 = (ticks >> 8) + salt;
        slowcycle16 += sin8(slowcycle16);
        slowcycle16 = (slowcycle16 * 2053) + 1384;
        uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);

        uint8_t bright = 0;
        if (((slowcycle8 & 0x0E) / 2) < twinkleDensity) {
            bright = attackDecayWave8(fastcycle8);
        }

        uint8_t hue = slowcycle8 - salt;
        CRGB c;
        if (bright > 0) {
            c = ColorFromPalette(currentPalette, hue, bright, NOBLEND);
            if (coolLikeIncandescent) {
                coolLikeIncandescentFunction(c, fastcycle8);
            }
        } else {
            c = CRGB::Black;
        }
        return c;
    }

    uint8_t attackDecayWave8(uint8_t i) {
        if (i < 86) {
            return i * 3;
        } else {
            i -= 86;
            return 255 - (i + (i / 2));
        }
    }

    void coolLikeIncandescentFunction(CRGB &c, uint8_t phase) {
        if (phase < 128)
            return;

        uint8_t cooling = (phase - 128) >> 4;
        c.g = qsub8(c.g, cooling);
        c.b = qsub8(c.b, cooling * 2);
    }
};

// Color palettes
// Color palette definitions
// A mostly red palette with green accents and white trim.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 RedGreenWhite_p FL_PROGMEM = {
    CRGB::Red,   CRGB::Red,   CRGB::Red,   CRGB::Red,  CRGB::Red,  CRGB::Red,
    CRGB::Red,   CRGB::Red,   CRGB::Red,   CRGB::Red,  CRGB::Gray, CRGB::Gray,
    CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green};

const TProgmemRGBPalette16 Holly_p FL_PROGMEM = {
    0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c,
    0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c, 0x00580c,
    0x00580c, 0x00580c, 0x00580c, 0xB00402};

const TProgmemRGBPalette16 RedWhite_p FL_PROGMEM = {
    CRGB::Red,  CRGB::Red,  CRGB::Red,  CRGB::Red, CRGB::Gray, CRGB::Gray,
    CRGB::Gray, CRGB::Gray, CRGB::Red,  CRGB::Red, CRGB::Red,  CRGB::Red,
    CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray};

const TProgmemRGBPalette16 BlueWhite_p FL_PROGMEM = {
    CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
    CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
    CRGB::Blue, CRGB::Gray, CRGB::Gray, CRGB::Gray};

const TProgmemRGBPalette16 FairyLight_p = {
    CRGB::FairyLight,
    CRGB::FairyLight,
    CRGB::FairyLight,
    CRGB::FairyLight,
    CRGB(CRGB::FairyLight).nscale8_constexpr(uint8_t(128)).as_uint32_t(),
    CRGB(CRGB::FairyLight).nscale8_constexpr(uint8_t(128)).as_uint32_t(),
    CRGB::FairyLight,
    CRGB::FairyLight,
    CRGB(CRGB::FairyLight).nscale8_constexpr(64).as_uint32_t(),
    CRGB(CRGB::FairyLight).nscale8_constexpr(64).as_uint32_t(),
    CRGB::FairyLight,
    CRGB::FairyLight,
    CRGB::FairyLight,
    CRGB::FairyLight,
    CRGB::FairyLight,
    CRGB::FairyLight};

const TProgmemRGBPalette16 Snow_p FL_PROGMEM = {
    0x304048, 0x304048, 0x304048, 0x304048, 0x304048, 0x304048,
    0x304048, 0x304048, 0x304048, 0x304048, 0x304048, 0x304048,
    0x304048, 0x304048, 0x304048, 0xE0F0FF};

const TProgmemRGBPalette16 RetroC9_p FL_PROGMEM = {
    0xB80400, 0x902C02, 0xB80400, 0x902C02, 0x902C02, 0xB80400,
    0x902C02, 0xB80400, 0x046002, 0x046002, 0x046002, 0x046002,
    0x070758, 0x070758, 0x070758, 0x606820};

const TProgmemRGBPalette16 Ice_p FL_PROGMEM = {
    0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
    0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
    0x182080, 0x182080, 0x182080, 0x5080C0};

// Add or remove palette names from this list to control which color
// palettes are used, and in what order.
const TProgmemRGBPalette16 *ActivePaletteList[] = {
    &RetroC9_p,       &BlueWhite_p,   &RainbowColors_p, &FairyLight_p,
    &RedGreenWhite_p, &PartyColors_p, &RedWhite_p,      &Snow_p,
    &Holly_p,         &Ice_p};

void TwinkleFox::chooseNextColorPalette(CRGBPalette16 &pal) {
    const uint8_t numberOfPalettes =
        sizeof(ActivePaletteList) / sizeof(ActivePaletteList[0]);
    static uint8_t whichPalette = -1;
    whichPalette = addmod8(whichPalette, 1, numberOfPalettes);
    pal = *(ActivePaletteList[whichPalette]);
}

} // namespace fl
