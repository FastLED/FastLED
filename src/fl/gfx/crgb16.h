#pragma once

#include "fl/stl/int.h"
#include "fl/math/fixed_point/u8x8.h"
#include "fl/stl/noexcept.h"

namespace fl {

struct CRGB16 {
    typedef u8x8 fp;
    u8x8 r, g, b;
    CRGB16() FL_NOEXCEPT = default;
    CRGB16(u8x8 rv, u8x8 gv, u8x8 bv) : r(rv), g(gv), b(bv) {}
    CRGB16& nscale8(u8 scale) {
        r = u8x8::from_raw(static_cast<u16>((static_cast<u32>(r.raw()) * scale) >> 8));
        g = u8x8::from_raw(static_cast<u16>((static_cast<u32>(g.raw()) * scale) >> 8));
        b = u8x8::from_raw(static_cast<u16>((static_cast<u32>(b.raw()) * scale) >> 8));
        return *this;
    }
    CRGB16& nscale(u8x8 scale) {
        r = r * scale;
        g = g * scale;
        b = b * scale;
        return *this;
    }
    CRGB16& operator+=(const CRGB16& rhs) {
        u32 nr = static_cast<u32>(r.raw()) + rhs.r.raw();
        u32 ng = static_cast<u32>(g.raw()) + rhs.g.raw();
        u32 nb = static_cast<u32>(b.raw()) + rhs.b.raw();
        r = u8x8::from_raw(nr > 0xFFFF ? u16(0xFFFF) : static_cast<u16>(nr));
        g = u8x8::from_raw(ng > 0xFFFF ? u16(0xFFFF) : static_cast<u16>(ng));
        b = u8x8::from_raw(nb > 0xFFFF ? u16(0xFFFF) : static_cast<u16>(nb));
        return *this;
    }
};

} // namespace fl
