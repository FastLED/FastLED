#pragma once

/// @file    pride2015.hpp
/// @brief   Animated, ever-changing rainbows (Pride2015 effect)
/// @example Pride2015.ino

// Pride2015
// Animated, ever-changing rainbows.
// by Mark Kriegsman

#include "FastLED.h"

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

struct Pride2015Data {
    CRGB* leds = nullptr;
    uint16_t num_leds = 0;
    uint16_t sPseudotime = 0;
    uint16_t sLastMillis = 0;
    uint16_t sHue16 = 0;

    // constructor
    Pride2015Data(CRGB* leds, uint16_t num_leds)
        : leds(leds), num_leds(num_leds) {}
};

// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void Pride2015Loop(Pride2015Data& self) {
    if (self.leds == nullptr || self.num_leds == 0) {
        return;
    }

    // tell FastLED about the LED strip configuration

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = self.sHue16;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);
  
    uint16_t ms = millis();
    uint16_t deltams = ms - self.sLastMillis;
    self.sLastMillis = ms;
    self.sPseudotime += deltams * msmultiplier;
    self.sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = self.sPseudotime;
  
    // set master brightness control
    for (uint16_t i = 0; i < self.num_leds; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);
    
        CRGB newcolor = CHSV(hue8, sat8, bri8);
    
        uint16_t pixelnumber = (self.num_leds - 1) - i;
    
        nblend(self.leds[pixelnumber], newcolor, 64);
    }
}
