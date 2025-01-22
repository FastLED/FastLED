/// @file    scale_up.h
/// @brief   Expands a grid using bilinear interpolation and scaling up. This is
/// useful for
///          under powered devices that can't handle the full resolution of the
///          grid, or if you suddenly need to increase the size of the grid and
///          don't want to re-create new assets at the new resolution.

#pragma once

#include <stdint.h>

#include "bilinear_expansion.h"
#include "fl/ptr.h"
#include "fx/fx2d.h"
#include "lib8tion/random8.h"
#include "noise.h"
#include "fl/xymap.h"

// Optimized for 2^n grid sizes in terms of both memory and performance.
// If you are somehow running this on AVR then you probably want this if
// you can make your grid size a power of 2.
#define FASTLED_SCALE_UP_ALWAYS_POWER_OF_2 0 // 0 for always power of 2.
// Uses more memory than FASTLED_SCALE_UP_ALWAYS_POWER_OF_2 but can handle
// arbitrary grid sizes.
#define FASTLED_SCALE_UP_HIGH_PRECISION 1 // 1 for always choose high precision.
// Uses the most executable memory because both low and high precision versions
// are compiled in. If the grid size is a power of 2 then the faster version is
// used. Note that the floating point version has to be directly specified
// because in testing it offered no benefits over the integer versions.
#define FASTLED_SCALE_UP_DECIDE_AT_RUNTIME 2 // 2 for runtime decision.

#define FASTLED_SCALE_UP_FORCE_FLOATING_POINT 3 // Warning, this is slow.

#ifndef FASTLED_SCALE_UP
#define FASTLED_SCALE_UP FASTLED_SCALE_UP_DECIDE_AT_RUNTIME
#endif

namespace fl {

FASTLED_SMART_PTR(ScaleUp);

// Uses bilearn filtering to double the size of the grid.
class ScaleUp : public Fx2d {
  public:
    ScaleUp(XYMap xymap, Fx2dPtr fx);
    void draw(DrawContext context) override;

    void expand(const CRGB *input, CRGB *output, uint16_t width,
                uint16_t height, XYMap mXyMap);

    fl::Str fxName() const override { return "scale_up"; }

  private:
    // No expansion needed. Also useful for debugging.
    void noExpand(const CRGB *input, CRGB *output, uint16_t width,
                  uint16_t height);
    Fx2dPtr mDelegate;
    fl::scoped_array<CRGB> mSurface;
};

} // namespace fl
