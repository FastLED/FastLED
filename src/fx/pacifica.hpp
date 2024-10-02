#pragma once

/// @brief   An animation that simulates gentle, blue-green ocean waves
/// @example Pacifica.ino
//////////////////////////////////////////////////////////////////////////
//
// The code for this animation is more complicated than other examples, and 
// while it is "ready to run", and documented in general, it is probably not 
// the best starting point for learning.  Nevertheless, it does illustrate some
// useful techniques.
//
//////////////////////////////////////////////////////////////////////////
//
// In this animation, there are four "layers" of waves of light.  
//
// Each layer moves independently, and each is scaled separately.
//
// All four wave layers are added together on top of each other, and then 
// another filter is applied that adds "whitecaps" of brightness where the 
// waves line up with each other more.  Finally, another pass is taken
// over the led array to 'deepen' (dim) the blues and greens.
//
// The speed and scale and motion each layer varies slowly within independent 
// hand-chosen ranges, which is why the code has a lot of low-speed 'beatsin8' functions
// with a lot of oddly specific numeric ranges.
//
// These three custom blue-green color palettes were inspired by the colors found in
// the waters off the southern coast of California, https://goo.gl/maps/QQgd97jjHesHZVxQ7
//


#include "FastLED.h"

struct PacificaData {
    CRGB* leds = nullptr;
    uint16_t num_leds = 0;
    uint16_t sCIStart1 = 0, sCIStart2 = 0, sCIStart3 = 0, sCIStart4 = 0;
    uint32_t sLastms = 0;

    CRGBPalette16 pacifica_palette_1 = 
        { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
          0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50 };
    CRGBPalette16 pacifica_palette_2 = 
        { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
          0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F };
    CRGBPalette16 pacifica_palette_3 = 
        { 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33, 
          0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF };

    // constructor
    PacificaData(CRGB* leds, uint16_t num_leds)
        : leds(leds), num_leds(num_leds) {}
};

void PacificaLoop(PacificaData& self);

// Add one layer of waves into the led array
void pacifica_one_layer(PacificaData& self, CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff);

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps(PacificaData& self);

// Deepen the blues and greens
void pacifica_deepen_colors(PacificaData& self);

void PacificaLoop(PacificaData& self) {
    // Update the hue each time through the loop
    uint32_t ms = millis();
    uint32_t deltams = ms - self.sLastms;
    self.sLastms = ms;
    uint16_t speedfactor1 = beatsin16(3, 179, 269);
    uint16_t speedfactor2 = beatsin16(4, 179, 269);
    uint32_t deltams1 = (deltams * speedfactor1) / 256;
    uint32_t deltams2 = (deltams * speedfactor2) / 256;
    uint32_t deltams21 = (deltams1 + deltams2) / 2;
    self.sCIStart1 += (deltams1 * beatsin88(1011,10,13));
    self.sCIStart2 -= (deltams21 * beatsin88(777,8,11));
    self.sCIStart3 -= (deltams1 * beatsin88(501,5,7));
    self.sCIStart4 -= (deltams2 * beatsin88(257,4,6));

    // Clear out the LED array to a dim background blue-green
    fill_solid(self.leds, self.num_leds, CRGB(2, 6, 10));

    // Render each of four layers, with different scales and speeds, that vary over time
    pacifica_one_layer(self, self.pacifica_palette_1, self.sCIStart1, beatsin16(3, 11 * 256, 14 * 256), beatsin8(10, 70, 130), 0-beat16(301));
    pacifica_one_layer(self, self.pacifica_palette_2, self.sCIStart2, beatsin16(4,  6 * 256,  9 * 256), beatsin8(17, 40,  80), beat16(401));
    pacifica_one_layer(self, self.pacifica_palette_3, self.sCIStart3, 6 * 256, beatsin8(9, 10,38), 0-beat16(503));
    pacifica_one_layer(self, self.pacifica_palette_3, self.sCIStart4, 5 * 256, beatsin8(8, 10,28), beat16(601));

    // Add brighter 'whitecaps' where the waves lines up more
    pacifica_add_whitecaps(self);

    // Deepen the blues and greens a bit
    pacifica_deepen_colors(self);
}

// Add one layer of waves into the led array
void pacifica_one_layer(PacificaData& self, CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff) {
    uint16_t ci = cistart;
    uint16_t waveangle = ioff;
    uint16_t wavescale_half = (wavescale / 2) + 20;
    for (uint16_t i = 0; i < self.num_leds; i++) {
        waveangle += 250;
        uint16_t s16 = sin16(waveangle) + 32768;
        uint16_t cs = scale16(s16, wavescale_half) + wavescale_half;
        ci += cs;
        uint16_t sindex16 = sin16(ci) + 32768;
        uint8_t sindex8 = scale16(sindex16, 240);
        CRGB c = ColorFromPalette(p, sindex8, bri, LINEARBLEND);
        self.leds[i] += c;
    }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps(PacificaData& self) {
    uint8_t basethreshold = beatsin8(9, 55, 65);
    uint8_t wave = beat8(7);
    
    for (uint16_t i = 0; i < self.num_leds; i++) {
        uint8_t threshold = scale8(sin8(wave), 20) + basethreshold;
        wave += 7;
        uint8_t l = self.leds[i].getAverageLight();
        if (l > threshold) {
            uint8_t overage = l - threshold;
            uint8_t overage2 = qadd8(overage, overage);
            self.leds[i] += CRGB(overage, overage2, qadd8(overage2, overage2));
        }
    }
}

// Deepen the blues and greens
void pacifica_deepen_colors(PacificaData& self) {
    for (uint16_t i = 0; i < self.num_leds; i++) {
        self.leds[i].blue = scale8(self.leds[i].blue,  145); 
        self.leds[i].green = scale8(self.leds[i].green, 200); 
        self.leds[i] |= CRGB(2, 5, 7);
    }
}
