#pragma once

// Signed 4.12 fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Signed 4.12 fixed-point value type.
class s4x12 {
  public:
    static constexpr int INT_BITS = 4;
    static constexpr int FRAC_BITS = 12;

    // ---- Construction ------------------------------------------------------

    constexpr s4x12() = default;

    explicit constexpr s4x12(float f)
        : mValue(static_cast<i16>(f * (1 << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE s4x12 from_raw(i16 raw) {
        s4x12 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    i16 raw() const { return mValue; }
    i16 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE s4x12 operator*(s4x12 b) const {
        return from_raw(static_cast<i16>(
            (static_cast<i32>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE s4x12 operator/(s4x12 b) const {
        return from_raw(static_cast<i16>(
            (static_cast<i32>(mValue) << FRAC_BITS) / b.mValue));
    }

    FASTLED_FORCE_INLINE s4x12 operator+(s4x12 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE s4x12 operator-(s4x12 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE s4x12 operator-() const {
        return from_raw(-mValue);
    }

    FASTLED_FORCE_INLINE s4x12 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE s4x12 operator*(i16 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE s4x12 operator*(i16 scalar, s4x12 fp) {
        return s4x12::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(s4x12 b) const { return mValue < b.mValue; }
    bool operator>(s4x12 b) const { return mValue > b.mValue; }
    bool operator<=(s4x12 b) const { return mValue <= b.mValue; }
    bool operator>=(s4x12 b) const { return mValue >= b.mValue; }
    bool operator==(s4x12 b) const { return mValue == b.mValue; }
    bool operator!=(s4x12 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE s4x12 mod(s4x12 a, s4x12 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE s4x12 floor(s4x12 x) {
        constexpr i16 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE s4x12 ceil(s4x12 x) {
        constexpr i16 frac_mask = (1 << FRAC_BITS) - 1;
        i16 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE s4x12 fract(s4x12 x) {
        constexpr i16 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE s4x12 abs(s4x12 x) {
        return from_raw(x.mValue < 0 ? -x.mValue : x.mValue);
    }

    static FASTLED_FORCE_INLINE s4x12 sign(s4x12 x) {
        constexpr s4x12 pos_one(1.0f);
        constexpr s4x12 neg_one(-1.0f);
        if (x.mValue > 0) return pos_one;
        if (x.mValue < 0) return neg_one;
        return s4x12();
    }

    static FASTLED_FORCE_INLINE s4x12 lerp(s4x12 a, s4x12 b, s4x12 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE s4x12 clamp(s4x12 x, s4x12 lo, s4x12 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE s4x12 step(s4x12 edge, s4x12 x) {
        constexpr s4x12 one(1.0f);
        return x < edge ? s4x12() : one;
    }

    static FASTLED_FORCE_INLINE s4x12 smoothstep(s4x12 edge0, s4x12 edge1, s4x12 x) {
        constexpr s4x12 zero(0.0f);
        constexpr s4x12 one(1.0f);
        constexpr s4x12 two(2.0f);
        constexpr s4x12 three(3.0f);
        s4x12 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    // ---- Inverse Trigonometry (pure fixed-point) ----------------------------

    static FASTLED_FORCE_INLINE s4x12 atan(s4x12 x) {
        constexpr s4x12 one(1.0f);
        constexpr s4x12 pi_over_2(1.5707963f);
        bool neg = x.mValue < 0;
        s4x12 ax = abs(x);
        s4x12 result;
        if (ax <= one) {
            result = atan_unit(ax);
        } else {
            result = pi_over_2 - atan_unit(one / ax);
        }
        return neg ? -result : result;
    }

    static FASTLED_FORCE_INLINE s4x12 atan2(s4x12 y, s4x12 x) {
        constexpr s4x12 pi(3.1415926f);
        constexpr s4x12 pi_over_2(1.5707963f);
        if (x.mValue == 0 && y.mValue == 0) return s4x12();
        if (x.mValue == 0) return y.mValue > 0 ? pi_over_2 : -pi_over_2;
        if (y.mValue == 0) return x.mValue > 0 ? s4x12() : pi;
        s4x12 ax = abs(x);
        s4x12 ay = abs(y);
        s4x12 a;
        if (ax >= ay) {
            a = atan_unit(ay / ax);
        } else {
            a = pi_over_2 - atan_unit(ax / ay);
        }
        if (x.mValue < 0) a = pi - a;
        if (y.mValue < 0) a = -a;
        return a;
    }

    static FASTLED_FORCE_INLINE s4x12 asin(s4x12 x) {
        constexpr s4x12 one(1.0f);
        return atan2(x, sqrt(one - x * x));
    }

    static FASTLED_FORCE_INLINE s4x12 acos(s4x12 x) {
        constexpr s4x12 one(1.0f);
        return atan2(sqrt(one - x * x), x);
    }

    static FASTLED_FORCE_INLINE s4x12 sqrt(s4x12 x) {
        if (x.mValue <= 0) return s4x12();
        return from_raw(static_cast<i16>(
            fl::isqrt32(static_cast<u32>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE s4x12 rsqrt(s4x12 x) {
        s4x12 s = sqrt(x);
        if (s.mValue == 0) return s4x12();
        return from_raw(static_cast<i16>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE s4x12 pow(s4x12 base, s4x12 exp) {
        if (base.mValue <= 0) return s4x12();
        constexpr s4x12 one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
        return exp2_fp(exp * log2_fp(base));
    }

    // ---- Trigonometry ------------------------------------------------------

    static FASTLED_FORCE_INLINE s4x12 sin(s4x12 angle) {
        return from_raw(static_cast<i16>(fl::sin32(angle_to_a24(angle)) >> 19));
    }

    static FASTLED_FORCE_INLINE s4x12 cos(s4x12 angle) {
        return from_raw(static_cast<i16>(fl::cos32(angle_to_a24(angle)) >> 19));
    }

    // Combined sin+cos from s4x12 radians. Output in s4x12 [-1, 1].
    static FASTLED_FORCE_INLINE void sincos(s4x12 angle, s4x12 &out_sin,
                                            s4x12 &out_cos) {
        u32 a24 = angle_to_a24(angle);
        out_sin = from_raw(static_cast<i16>(fl::sin32(a24) >> 19));
        out_cos = from_raw(static_cast<i16>(fl::cos32(a24) >> 19));
    }

  private:
    i16 mValue = 0;

    // Returns 0-based position of highest set bit, or -1 if v==0.
    static FASTLED_FORCE_INLINE int highest_bit(u32 v) {
        if (v == 0) return -1;
        int r = 0;
        if (v & 0xFFFF0000u) { v >>= 16; r += 16; }
        if (v & 0x0000FF00u) { v >>= 8;  r += 8; }
        if (v & 0x000000F0u) { v >>= 4;  r += 4; }
        if (v & 0x0000000Cu) { v >>= 2;  r += 2; }
        if (v & 0x00000002u) { r += 1; }
        return r;
    }

    // Fixed-point log base 2 for positive values.
    static FASTLED_FORCE_INLINE s4x12 log2_fp(s4x12 x) {
        u32 val = static_cast<u32>(x.mValue);
        int msb = highest_bit(val);
        i32 int_part = msb - FRAC_BITS;
        i32 t;
        if (msb >= FRAC_BITS) {
            t = static_cast<i32>(
                (val >> (msb - FRAC_BITS)) - (1u << FRAC_BITS));
        } else {
            t = static_cast<i32>(
                (val << (FRAC_BITS - msb)) - (1u << FRAC_BITS));
        }
        s4x12 tf = from_raw(static_cast<i16>(t));
        constexpr s4x12 c0(1.4284f);
        constexpr s4x12 c1(-0.5765f);
        constexpr s4x12 c2(0.1481f);
        s4x12 frac_part = tf * (c0 + tf * (c1 + tf * c2));
        return from_raw(static_cast<i16>(
            (int_part << FRAC_BITS) + frac_part.mValue));
    }

    // Fixed-point 2^x.
    static FASTLED_FORCE_INLINE s4x12 exp2_fp(s4x12 x) {
        s4x12 fl_val = floor(x);
        s4x12 fr = x - fl_val;
        i32 n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS - 1) return from_raw(0x7FFF);
        if (n < -FRAC_BITS) return s4x12();
        i32 int_pow;
        if (n >= 0) {
            int_pow = static_cast<i32>(1u << FRAC_BITS) << n;
        } else {
            int_pow = static_cast<i32>(1u << FRAC_BITS) >> (-n);
        }
        constexpr s4x12 one(1.0f);
        constexpr s4x12 d0(0.69314718f);
        constexpr s4x12 d1(0.24022651f);
        constexpr s4x12 d2(0.05550411f);
        s4x12 frac_pow = one + fr * (d0 + fr * (d1 + fr * d2));
        i32 result =
            (int_pow * static_cast<i32>(frac_pow.mValue)) >> FRAC_BITS;
        return from_raw(static_cast<i16>(result));
    }

    // Converts s4x12 radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE u32 angle_to_a24(s4x12 angle) {
        // 256/(2*PI) in s4x12 — converts radians to sin32/cos32 format.
        static constexpr i32 RAD_TO_24 = 2670177;
        return static_cast<u32>(
            (static_cast<int64_t>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }

    // Polynomial atan for t in [0, 1]. Returns [0, π/4].
    static FASTLED_FORCE_INLINE s4x12 atan_unit(s4x12 t) {
        constexpr s4x12 pi_over_4(0.7853981f);
        constexpr s4x12 c1(0.2447f);
        constexpr s4x12 c2(0.0663f);
        constexpr s4x12 one(1.0f);
        return pi_over_4 * t - t * (t - one) * (c1 + c2 * t);
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
