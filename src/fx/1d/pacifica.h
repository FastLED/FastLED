#pragma once

#include "FastLED.h"
#include "fl/namespace.h"
#include "fx/fx1d.h"

namespace fl {

/// @file    pacifica.hpp
/// @brief   An animation that simulates gentle, blue-green ocean waves
/// @example Pacifica.ino

FASTLED_SMART_PTR(Pacifica);

class Pacifica : public Fx1d {
  public:
    Pacifica(uint16_t num_leds) : Fx1d(num_leds) {}

    void draw(DrawContext context) override;
    fl::string fxName() const override { return "Pacifica"; }

  private:
    uint16_t sCIStart1 = 0, sCIStart2 = 0, sCIStart3 = 0, sCIStart4 = 0;
    fl::u32 sLastms = 0;

    CRGBPalette16 pacifica_palette_1 = {0x000507, 0x000409, 0x00030B, 0x00030D,
                                        0x000210, 0x000212, 0x000114, 0x000117,
                                        0x000019, 0x00001C, 0x000026, 0x000031,
                                        0x00003B, 0x000046, 0x14554B, 0x28AA50};
    CRGBPalette16 pacifica_palette_2 = {0x000507, 0x000409, 0x00030B, 0x00030D,
                                        0x000210, 0x000212, 0x000114, 0x000117,
                                        0x000019, 0x00001C, 0x000026, 0x000031,
                                        0x00003B, 0x000046, 0x0C5F52, 0x19BE5F};
    CRGBPalette16 pacifica_palette_3 = {0x000208, 0x00030E, 0x000514, 0x00061A,
                                        0x000820, 0x000927, 0x000B2D, 0x000C33,
                                        0x000E39, 0x001040, 0x001450, 0x001860,
                                        0x001C70, 0x002080, 0x1040BF, 0x2060FF};

    void pacifica_one_layer(CRGB *leds, CRGBPalette16 &p, uint16_t cistart,
                            uint16_t wavescale, uint8_t bri, uint16_t ioff);
    void pacifica_add_whitecaps(CRGB *leds);
    void pacifica_deepen_colors(CRGB *leds);
};

void Pacifica::draw(DrawContext ctx) {
    CRGB *leds = ctx.leds;
    fl::u32 now = ctx.now;
    if (leds == nullptr || mNumLeds == 0) {
        return;
    }

    // Update the hue each time through the loop
    fl::u32 ms = now;
    fl::u32 deltams = ms - sLastms;
    sLastms = ms;
    uint16_t speedfactor1 = beatsin16(3, 179, 269);
    uint16_t speedfactor2 = beatsin16(4, 179, 269);
    fl::u32 deltams1 = (deltams * speedfactor1) / 256;
    fl::u32 deltams2 = (deltams * speedfactor2) / 256;
    fl::u32 deltams21 = (deltams1 + deltams2) / 2;
    sCIStart1 += (deltams1 * beatsin88(1011, 10, 13));
    sCIStart2 -= (deltams21 * beatsin88(777, 8, 11));
    sCIStart3 -= (deltams1 * beatsin88(501, 5, 7));
    sCIStart4 -= (deltams2 * beatsin88(257, 4, 6));

    // Clear out the LED array to a dim background blue-green
    fill_solid(leds, mNumLeds, CRGB(2, 6, 10));

    // Render each of four layers, with different scales and speeds, that vary
    // over time
    pacifica_one_layer(leds, pacifica_palette_1, sCIStart1,
                       beatsin16(3, 11 * 256, 14 * 256), beatsin8(10, 70, 130),
                       0 - beat16(301));
    pacifica_one_layer(leds, pacifica_palette_2, sCIStart2,
                       beatsin16(4, 6 * 256, 9 * 256), beatsin8(17, 40, 80),
                       beat16(401));
    pacifica_one_layer(leds, pacifica_palette_3, sCIStart3, 6 * 256,
                       beatsin8(9, 10, 38), 0 - beat16(503));
    pacifica_one_layer(leds, pacifica_palette_3, sCIStart4, 5 * 256,
                       beatsin8(8, 10, 28), beat16(601));

    // Add brighter 'whitecaps' where the waves lines up more
    pacifica_add_whitecaps(leds);

    // Deepen the blues and greens a bit
    pacifica_deepen_colors(leds);
}

// Add one layer of waves into the led array
void Pacifica::pacifica_one_layer(CRGB *leds, CRGBPalette16 &p,
                                  uint16_t cistart, uint16_t wavescale,
                                  uint8_t bri, uint16_t ioff) {
    uint16_t ci = cistart;
    uint16_t waveangle = ioff;
    uint16_t wavescale_half = (wavescale / 2) + 20;
    for (uint16_t i = 0; i < mNumLeds; i++) {
        waveangle += 250;
        uint16_t s16 = sin16(waveangle) + 32768;
        uint16_t cs = scale16(s16, wavescale_half) + wavescale_half;
        ci += cs;
        uint16_t sindex16 = sin16(ci) + 32768;
        uint8_t sindex8 = scale16(sindex16, 240);
        CRGB c = ColorFromPalette(p, sindex8, bri, LINEARBLEND);
        leds[i] += c;
    }
}

// Add extra 'white' to areas where the four layers of light have lined up
// brightly
void Pacifica::pacifica_add_whitecaps(CRGB *leds) {
    uint8_t basethreshold = beatsin8(9, 55, 65);
    uint8_t wave = beat8(7);

    for (uint16_t i = 0; i < mNumLeds; i++) {
        uint8_t threshold = scale8(sin8(wave), 20) + basethreshold;
        wave += 7;
        uint8_t l = leds[i].getAverageLight();
        if (l > threshold) {
            uint8_t overage = l - threshold;
            uint8_t overage2 = qadd8(overage, overage);
            leds[i] += CRGB(overage, overage2, qadd8(overage2, overage2));
        }
    }
}

// Deepen the blues and greens
void Pacifica::pacifica_deepen_colors(CRGB *leds) {
    for (uint16_t i = 0; i < mNumLeds; i++) {
        leds[i].blue = scale8(leds[i].blue, 145);
        leds[i].green = scale8(leds[i].green, 200);
        leds[i] |= CRGB(2, 5, 7);
    }
}

} // namespace fl
