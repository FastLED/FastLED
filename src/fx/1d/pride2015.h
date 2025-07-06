#pragma once

#include "FastLED.h"
#include "fl/namespace.h"
#include "fx/fx1d.h"

namespace fl {

/// @file    pride2015.hpp
/// @brief   Animated, ever-changing rainbows (Pride2015 effect)
/// @example Pride2015.ino

// Pride2015
// Animated, ever-changing rainbows.
// by Mark Kriegsman

FASTLED_SMART_PTR(Pride2015);

class Pride2015 : public Fx1d {
  public:
    Pride2015(uint16_t num_leds) : Fx1d(num_leds) {}

    void draw(Fx::DrawContext context) override;
    fl::string fxName() const override { return "Pride2015"; }

  private:
    uint16_t mPseudotime = 0;
    uint16_t mLastMillis = 0;
    uint16_t mHue16 = 0;
};

// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void Pride2015::draw(Fx::DrawContext ctx) {
    if (ctx.leds == nullptr || mNumLeds == 0) {
        return;
    }

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = mHue16;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - mLastMillis;
    mLastMillis = ms;
    mPseudotime += deltams * msmultiplier;
    mHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = mPseudotime;

    // set master brightness control
    for (uint16_t i = 0; i < mNumLeds; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (fl::u32)((fl::u32)b16 * (fl::u32)b16) / 65536;
        uint8_t bri8 = (fl::u32)(((fl::u32)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = (mNumLeds - 1) - i;

        nblend(ctx.leds[pixelnumber], newcolor, 64);
    }
}

} // namespace fl
