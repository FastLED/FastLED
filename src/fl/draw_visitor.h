#pragma once

#include "crgb.h"
#include "fl/geometry.h"
#include "fl/namespace.h"
#include "fl/unused.h"
#include "fl/xymap.h"
#include <stdint.h>

namespace fl {

// Draws a uint8_t value to a CRGB array, blending it with the existing color.
struct XYDrawComposited {
    XYDrawComposited(const CRGB &color, const XYMap &xymap, CRGB *out);
    void draw(const point_xy<int> &pt, uint32_t index, uint8_t value);
    const CRGB mColor;
    const XYMap mXYMap;
    CRGB *mOut;
};

inline XYDrawComposited::XYDrawComposited(const CRGB &color, const XYMap &xymap,
                                          CRGB *out)
    : mColor(color), mXYMap(xymap), mOut(out) {}

inline void XYDrawComposited::draw(const point_xy<int> &pt, uint32_t index,
                                   uint8_t value) {
    FASTLED_UNUSED(pt);
    CRGB &c = mOut[index];
    CRGB blended = mColor;
    blended.fadeToBlackBy(255 - value);
    c = CRGB::blendAlphaMaxChannel(blended, c);
}

} // namespace fl
