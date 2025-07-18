
#include "fl/stdint.h"

#define FASTLED_INTERNAL
#include "FastLED.h"
#include "fl/upscale.h"
#include "fl/memory.h"
#include "fl/xymap.h"
#include "fx/fx2d.h"
#include "lib8tion/random8.h"
#include "noise.h"

// Include here so that #define PI used in Arduino.h does not produce a warning.
#include "scale_up.h"

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

ScaleUp::ScaleUp(const XYMap& xymap, Fx2dPtr fx) : Fx2d(xymap), mDelegate(fx) {
    // Turn off re-mapping of the delegate's XYMap, since bilinearExpand needs
    // to work in screen coordinates. The final mapping will for this class will
    // still be performed.
    mDelegate->getXYMap().setRectangularGrid();
}

void ScaleUp::draw(DrawContext context) {
    if (mSurface.empty()) {
        mSurface.resize(mDelegate->getNumLeds());
    }
    DrawContext delegateContext = context;
    delegateContext.leds = mSurface.data();
    mDelegate->draw(delegateContext);

    uint16_t in_w = mDelegate->getWidth();
    uint16_t in_h = mDelegate->getHeight();
    uint16_t out_w = getWidth();
    uint16_t out_h = getHeight();
    ;
    if (in_w == out_w && in_h == out_h) {
        noExpand(mSurface.data(), context.leds, in_w, in_h);
    } else {
        expand(mSurface.data(), context.leds, in_w, in_h, mXyMap);
    }
}

void ScaleUp::expand(const CRGB *input, CRGB *output, uint16_t width,
                     uint16_t height, const XYMap& mXyMap) {
#if FASTLED_SCALE_UP == FASTLED_SCALE_UP_ALWAYS_POWER_OF_2
    fl::upscalePowerOf2(input, output, static_cast<uint8_t>(width), static_cast<uint8_t>(height), mXyMap);
#elif FASTLED_SCALE_UP == FASTLED_SCALE_UP_HIGH_PRECISION
    fl::upscaleArbitrary(input, output, width, height, mXyMap);
#elif FASTLED_SCALE_UP == FASTLED_SCALE_UP_DECIDE_AT_RUNTIME
    fl::upscale(input, output, width, height, mXyMap);
#elif FASTLED_SCALE_UP == FASTLED_SCALE_UP_FORCE_FLOATING_POINT
    fl::upscaleFloat(input, output, static_cast<uint8_t>(width), static_cast<uint8_t>(height), mXyMap);
#else
#error "Invalid FASTLED_SCALE_UP"
#endif
}

void ScaleUp::noExpand(const CRGB *input, CRGB *output, uint16_t width,
                       uint16_t height) {
    uint16_t n = mXyMap.getTotal();
    for (uint16_t w = 0; w < width; w++) {
        for (uint16_t h = 0; h < height; h++) {
            uint16_t idx = mXyMap.mapToIndex(w, h);
            if (idx < n) {
                output[idx] = input[w * height + h];
            }
        }
    }
}

} // namespace fl
