/// @file crgb.cpp
/// Utility functions for the red, green, and blue (RGB) pixel struct

#define FASTLED_INTERNAL
#include "crgb.h"
#include "FastLED.h"
#include "fl/xymap.h"

#include "fl/upscale.h"
#include "fl/downscale.h"
#include "lib8tion/math8.h"

#include "fl/namespace.h"
#include "fl/int.h"

FASTLED_NAMESPACE_BEGIN

fl::string CRGB::toString() const {
    fl::string out;
    out.append("CRGB(");
    out.append(int16_t(r));
    out.append(",");
    out.append(int16_t(g));
    out.append(",");
    out.append(int16_t(b));
    out.append(")");
    return out;
}

CRGB CRGB::computeAdjustment(uint8_t scale, const CRGB &colorCorrection,
                             const CRGB &colorTemperature) {
#if defined(NO_CORRECTION) && (NO_CORRECTION == 1)
    return CRGB(scale, scale, scale);
#else
    CRGB adj(0, 0, 0);
    if (scale > 0) {
        for (uint8_t i = 0; i < 3; ++i) {
            uint8_t cc = colorCorrection.raw[i];
            uint8_t ct = colorTemperature.raw[i];
            if (cc > 0 && ct > 0) {
                // Optimized for AVR size. This function is only called very
                // infrequently so size matters more than speed.
                fl::u32 work = (((fl::u16)cc) + 1);
                work *= (((fl::u16)ct) + 1);
                work *= scale;
                work /= 0x10000L;
                adj.raw[i] = work & 0xFF;
            }
        }
    }
    return adj;
#endif
}

CRGB CRGB::blend(const CRGB &p1, const CRGB &p2, fract8 amountOfP2) {
    return CRGB(blend8(p1.r, p2.r, amountOfP2), blend8(p1.g, p2.g, amountOfP2),
                blend8(p1.b, p2.b, amountOfP2));
}

CRGB CRGB::blendAlphaMaxChannel(const CRGB &upper, const CRGB &lower) {
    // Use luma of upper pixel as alpha (0..255)
    uint8_t max_component = 0;
    for (int i = 0; i < 3; ++i) {
        if (upper.raw[i] > max_component) {
            max_component = upper.raw[i];
        }
    }
    // uint8_t alpha = upper.getLuma();
    // blend(lower, upper, alpha) → (lower * (255−alpha) + upper * alpha) / 256
    uint8_t amountOf2 = 255 - max_component;
    return CRGB::blend(upper, lower, amountOf2);
}

void CRGB::downscale(const CRGB *src, const fl::XYMap &srcXY, CRGB *dst,
                     const fl::XYMap &dstXY) {
    fl::downscale(src, srcXY, dst, dstXY);
}

void CRGB::upscale(const CRGB *src, const fl::XYMap &srcXY, CRGB *dst,
                   const fl::XYMap &dstXY) {
    FASTLED_WARN_IF(
        srcXY.getType() != fl::XYMap::kLineByLine,
        "Upscaling only works with a src matrix that is rectangular");
    fl::u16 w = srcXY.getWidth();
    fl::u16 h = srcXY.getHeight();
    fl::upscale(src, dst, w, h, dstXY);
}

CRGB &CRGB::nscale8(uint8_t scaledown) {
    nscale8x3(r, g, b, scaledown);
    return *this;
}

/// Add one CRGB to another, saturating at 0xFF for each channel
CRGB &CRGB::operator+=(const CRGB &rhs) {
    r = qadd8(r, rhs.r);
    g = qadd8(g, rhs.g);
    b = qadd8(b, rhs.b);
    return *this;
}

CRGB CRGB::lerp8(const CRGB &other, fract8 amountOf2) const {
    CRGB ret;

    ret.r = lerp8by8(r, other.r, amountOf2);
    ret.g = lerp8by8(g, other.g, amountOf2);
    ret.b = lerp8by8(b, other.b, amountOf2);

    return ret;
}

CRGB &CRGB::fadeToBlackBy(uint8_t fadefactor) {
    nscale8x3(r, g, b, 255 - fadefactor);
    return *this;
}

FASTLED_NAMESPACE_END
