#pragma once

#include "fl/stl/int.h"
#include "fl/stl/span.h"

#include "fl/gfx/gradient.h"
#include "fl/stl/compiler_control.h"
#include "fl/math/xymap.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Draws a fl::u8 value to a CRGB array, blending it with the existing color.
struct XYDrawComposited {
    XYDrawComposited(const CRGB &color, const XYMap &xymap, fl::span<CRGB> out);

    // Copy constructor (assignment deleted due to const members)
    XYDrawComposited(const XYDrawComposited &other) FL_NOEXCEPT = default;
    XYDrawComposited &operator=(const XYDrawComposited &other) = delete;

    // Move constructor (assignment deleted due to const members)
    XYDrawComposited(XYDrawComposited &&other) FL_NOEXCEPT
        : mColor(fl::move(other.mColor)), mXYMap(fl::move(other.mXYMap)), mOut(other.mOut) {}
    XYDrawComposited &operator=(XYDrawComposited &&other) FL_NOEXCEPT = delete;

    void draw(const vec2<fl::u16> &pt, fl::u32 index, fl::u8 value);
    const CRGB mColor;
    const XYMap mXYMap;
    fl::span<CRGB> mOut;
};

struct XYDrawGradient {
    XYDrawGradient(const Gradient &gradient, const XYMap &xymap, fl::span<CRGB> out);

    // Copy constructor (assignment deleted due to const members)
    XYDrawGradient(const XYDrawGradient &other) FL_NOEXCEPT = default;
    XYDrawGradient &operator=(const XYDrawGradient &other) = delete;

    // Move constructor (assignment deleted due to const members)
    XYDrawGradient(XYDrawGradient &&other) FL_NOEXCEPT
        : mGradient(fl::move(other.mGradient)), mXYMap(fl::move(other.mXYMap)), mOut(other.mOut) {}
    XYDrawGradient &operator=(XYDrawGradient &&other) FL_NOEXCEPT = delete;

    void draw(const vec2<fl::u16> &pt, fl::u32 index, fl::u8 value);
    const Gradient mGradient;
    const XYMap mXYMap;
    fl::span<CRGB> mOut;
};

inline XYDrawComposited::XYDrawComposited(const CRGB &color, const XYMap &xymap,
                                          fl::span<CRGB> out)
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
                                      const XYMap &xymap, fl::span<CRGB> out)
    : mGradient(gradient), mXYMap(xymap), mOut(out) {}

inline void XYDrawGradient::draw(const vec2<fl::u16> &pt, fl::u32 index,
                                 fl::u8 value) {
    FASTLED_UNUSED(pt);
    CRGB c = mGradient.colorAt(value);
    mOut[index] = c;
}

} // namespace fl
