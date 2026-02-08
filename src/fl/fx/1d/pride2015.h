#pragma once

#include "fl/fastled.h"
#include "fl/fx/fx1d.h"

namespace fl {

/// @file    pride2015.hpp
/// @brief   Animated, ever-changing rainbows (Pride2015 effect)
/// @example Pride2015.ino

// Pride2015
// Animated, ever-changing rainbows.
// by Mark Kriegsman

FASTLED_SHARED_PTR(Pride2015);

class Pride2015 : public Fx1d {
  public:
    Pride2015(u16 num_leds) : Fx1d(num_leds) {}

    void draw(Fx::DrawContext context) override;
    fl::string fxName() const override { return "Pride2015"; }

  private:
    u16 mPseudotime = 0;
    u16 mLastMillis = 0;
    u16 mHue16 = 0;
};

// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void Pride2015::draw(Fx::DrawContext ctx) {
    if (ctx.leds == nullptr || mNumLeds == 0) {
        return;
    }

    u8 sat8 = beatsin88(87, 220, 250);
    u8 brightdepth = beatsin88(341, 96, 224);
    u16 brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    u8 msmultiplier = beatsin88(147, 23, 60);

    u16 hue16 = mHue16;
    u16 hueinc16 = beatsin88(113, 1, 3000);

    u16 ms = fl::millis();
    u16 deltams = ms - mLastMillis;
    mLastMillis = ms;
    mPseudotime += deltams * msmultiplier;
    mHue16 += deltams * beatsin88(400, 5, 9);
    u16 brightnesstheta16 = mPseudotime;

    // set master brightness control
    for (u16 i = 0; i < mNumLeds; i++) {
        hue16 += hueinc16;
        u8 hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        u16 b16 = sin16(brightnesstheta16) + 32768;

        u16 bri16 = (fl::u32)((fl::u32)b16 * (fl::u32)b16) / 65536;
        u8 bri8 = (fl::u32)(((fl::u32)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        u16 pixelnumber = (mNumLeds - 1) - i;

        nblend(ctx.leds[pixelnumber], newcolor, 64);
    }
}

} // namespace fl
