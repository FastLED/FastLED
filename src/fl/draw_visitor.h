#pragma once

#include "fl/stdint.h"
#include "fl/int.h"

#include "crgb.h"
#include "fl/geometry.h"
#include "fl/gradient.h"
#include "fl/namespace.h"
#include "fl/unused.h"
#include "fl/xymap.h"
#include "fl/move.h"

namespace fl {

// Draws a fl::u8 value to a CRGB array, blending it with the existing color.
struct XYDrawComposited {
    XYDrawComposited(const CRGB &color, const XYMap &xymap, CRGB *out);
    
    // Copy constructor (assignment deleted due to const members)
    XYDrawComposited(const XYDrawComposited &other) = default;
    XYDrawComposited &operator=(const XYDrawComposited &other) = delete;
    
    // Move constructor (assignment deleted due to const members)
    XYDrawComposited(XYDrawComposited &&other) noexcept 
        : mColor(fl::move(other.mColor)), mXYMap(fl::move(other.mXYMap)), mOut(other.mOut) {}
    XYDrawComposited &operator=(XYDrawComposited &&other) noexcept = delete;
    
    void draw(const vec2<fl::u16> &pt, fl::u32 index, fl::u8 value);
    const CRGB mColor;
    const XYMap mXYMap;
    CRGB *mOut;
};

struct XYDrawGradient {
    XYDrawGradient(const Gradient &gradient, const XYMap &xymap, CRGB *out);
    
    // Copy constructor (assignment deleted due to const members)
    XYDrawGradient(const XYDrawGradient &other) = default;
    XYDrawGradient &operator=(const XYDrawGradient &other) = delete;
    
    // Move constructor (assignment deleted due to const members)
    XYDrawGradient(XYDrawGradient &&other) noexcept 
        : mGradient(fl::move(other.mGradient)), mXYMap(fl::move(other.mXYMap)), mOut(other.mOut) {}
    XYDrawGradient &operator=(XYDrawGradient &&other) noexcept = delete;
    
    void draw(const vec2<fl::u16> &pt, fl::u32 index, fl::u8 value);
    const Gradient mGradient;
    const XYMap mXYMap;
    CRGB *mOut;
};

inline XYDrawComposited::XYDrawComposited(const CRGB &color, const XYMap &xymap,
                                          CRGB *out)
    : mColor(color), mXYMap(xymap), mOut(out) {}

inline void XYDrawComposited::draw(const vec2<fl::u16> &pt, fl::u32 index,
                                   fl::u8 value) {
    FASTLED_UNUSED(pt);
    CRGB &c = mOut[index];
    CRGB blended = mColor;
    blended.fadeToBlackBy(255 - value);
    c = CRGB::blendAlphaMaxChannel(blended, c);
}

inline XYDrawGradient::XYDrawGradient(const Gradient &gradient,
                                      const XYMap &xymap, CRGB *out)
    : mGradient(gradient), mXYMap(xymap), mOut(out) {}

inline void XYDrawGradient::draw(const vec2<fl::u16> &pt, fl::u32 index,
                                 fl::u8 value) {
    FASTLED_UNUSED(pt);
    CRGB c = mGradient.colorAt(value);
    mOut[index] = c;
}

} // namespace fl
