#pragma once

#include <stdint.h>

#include "crgb.h"
#include "fl/geometry.h"
#include "fl/gradient.h"
#include "fl/namespace.h"
#include "fl/unused.h"
#include "fl/xymap.h"

namespace fl {

// Draws a uint8_t value to a CRGB array, blending it with the existing color.
struct XYDrawComposited {
    XYDrawComposited(const CRGB &color, const XYMap &xymap, CRGB *out);
    void draw(const vec2<int16_t> &pt, uint32_t index, uint8_t value);
    const CRGB mColor;
    const XYMap mXYMap;
    CRGB *mOut;
};

struct XYDrawGradient {
    XYDrawGradient(const Gradient &gradient, const XYMap &xymap, CRGB *out);
    void draw(const vec2<int16_t> &pt, uint32_t index, uint8_t value);
    const Gradient mGradient;
    const XYMap mXYMap;
    CRGB *mOut;
};

inline XYDrawComposited::XYDrawComposited(const CRGB &color, const XYMap &xymap,
                                          CRGB *out)
    : mColor(color), mXYMap(xymap), mOut(out) {}

inline void XYDrawComposited::draw(const vec2<int16_t> &pt, uint32_t index,
                                   uint8_t value) {
    FASTLED_UNUSED(pt);
    CRGB &c = mOut[index];
    CRGB blended = mColor;
    blended.fadeToBlackBy(255 - value);
    c = CRGB::blendAlphaMaxChannel(blended, c);
}

inline XYDrawGradient::XYDrawGradient(const Gradient &gradient,
                                      const XYMap &xymap, CRGB *out)
    : mGradient(gradient), mXYMap(xymap), mOut(out) {}

inline void XYDrawGradient::draw(const vec2<int16_t> &pt, uint32_t index,
                                 uint8_t value) {
    FASTLED_UNUSED(pt);
    CRGB c = mGradient.colorAt(value);
    mOut[index] = c;
}

} // namespace fl
