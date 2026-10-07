/// @file crgb.cpp
/// Utility functions for the red, green, and blue (RGB) pixel struct

#define FASTLED_INTERNAL
#include "crgb.h"
#include "fl/system/fastled.h"
#include "fl/math/math8.h"

#include "fl/stl/int.h"



fl::string CRGB::toString() const FL_NO_EXCEPT {
    fl::string out;
    out.append("CRGB(");
    out.append(fl::i16(r));
    out.append(",");
    out.append(fl::i16(g));
    out.append(",");
    out.append(fl::i16(b));
    out.append(")");
    return out;
}

CRGB CRGB::computeAdjustment(fl::u8 scale, const CRGB &colorCorrection,
                             const CRGB &colorTemperature) FL_NO_EXCEPT {
#if defined(NO_CORRECTION) && (NO_CORRECTION == 1)
    return CRGB(scale, scale, scale);
#else
    CRGB adj(0, 0, 0);
    if (scale > 0) {
        for (fl::u8 i = 0; i < 3; ++i) {
            fl::u8 cc = colorCorrection.raw[i];
            fl::u8 ct = colorTemperature.raw[i];
            if (cc > 0 && ct > 0) {
                // Optimized for AVR size. This function is only called very
                // infrequently so size matters more than speed.
                // (cc + 1) * scale is at most 65280, so it fits in
                // 16 bits. Reordering the exact product avoids a wide multiply.
                const fl::u16 scaledCorrection = (fl::u16(cc) + 1U) * scale;
                fl::u32 work = fl::u32(scaledCorrection) * (fl::u16(ct) + 1U);
                work /= 0x10000L;
                adj.raw[i] = work & 0xFF;
            }
        }
    }
    return adj;
#endif
}

CRGB CRGB::blend(const CRGB &p1, const CRGB &p2, fract8 amountOfP2) FL_NO_EXCEPT {
    return CRGB(fl::blend8(p1.r, p2.r, amountOfP2), fl::blend8(p1.g, p2.g, amountOfP2),
                fl::blend8(p1.b, p2.b, amountOfP2));
}

CRGB CRGB::blendAlphaMaxChannel(const CRGB &upper, const CRGB &lower) FL_NO_EXCEPT {
    // Use luma of upper pixel as alpha (0..255)
    fl::u8 max_component = 0;
    for (int i = 0; i < 3; ++i) {
        if (upper.raw[i] > max_component) {
            max_component = upper.raw[i];
        }
    }
    // uint8_t alpha = upper.getLuma();
    // blend(lower, upper, alpha) → (lower * (255−alpha) + upper * alpha) / 256
    fl::u8 amountOf2 = 255 - max_component;
    return CRGB::blend(upper, lower, amountOf2);
}

CRGB &CRGB::nscale8(fl::u8 scaledown) FL_NO_EXCEPT {
    nscale8x3(r, g, b, scaledown);
    return *this;
}

/// Add one CRGB to another, saturating at 0xFF for each channel
CRGB &CRGB::operator+=(const CRGB &rhs) FL_NO_EXCEPT {
    r = qadd8(r, rhs.r);
    g = qadd8(g, rhs.g);
    b = qadd8(b, rhs.b);
    return *this;
}

CRGB CRGB::lerp8(const CRGB &other, fract8 amountOf2) const FL_NO_EXCEPT {
    CRGB ret;

    ret.r = lerp8by8(r, other.r, amountOf2);
    ret.g = lerp8by8(g, other.g, amountOf2);
    ret.b = lerp8by8(b, other.b, amountOf2);

    return ret;
}

CRGB &CRGB::fadeToBlackBy(fl::u8 fadefactor) FL_NO_EXCEPT {
    nscale8x3(r, g, b, 255 - fadefactor);
    return *this;
}
