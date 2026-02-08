#pragma once

// Signed 8.24 fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Signed 8.24 fixed-point value type.
class s8x24 {
  public:
    static constexpr int INT_BITS = 8;
    static constexpr int FRAC_BITS = 24;

    // ---- Construction ------------------------------------------------------

    constexpr s8x24() = default;

    explicit constexpr s8x24(float f)
        : mValue(static_cast<int32_t>(f * (1 << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE s8x24 from_raw(int32_t raw) {
        s8x24 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    int32_t raw() const { return mValue; }
    int32_t to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE s8x24 operator*(s8x24 b) const {
        return from_raw(static_cast<int32_t>(
            (static_cast<int64_t>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE s8x24 operator/(s8x24 b) const {
        return from_raw(static_cast<int32_t>(
            (static_cast<int64_t>(mValue) << FRAC_BITS) / b.mValue));
    }

    FASTLED_FORCE_INLINE s8x24 operator+(s8x24 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE s8x24 operator-(s8x24 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE s8x24 operator-() const {
        return from_raw(-mValue);
    }

    FASTLED_FORCE_INLINE s8x24 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE s8x24 operator*(int32_t scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE s8x24 operator*(int32_t scalar, s8x24 fp) {
        return s8x24::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(s8x24 b) const { return mValue < b.mValue; }
    bool operator>(s8x24 b) const { return mValue > b.mValue; }
    bool operator<=(s8x24 b) const { return mValue <= b.mValue; }
    bool operator>=(s8x24 b) const { return mValue >= b.mValue; }
    bool operator==(s8x24 b) const { return mValue == b.mValue; }
    bool operator!=(s8x24 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE s8x24 mod(s8x24 a, s8x24 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE s8x24 floor(s8x24 x) {
        constexpr int32_t frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE s8x24 ceil(s8x24 x) {
        constexpr int32_t frac_mask = (1 << FRAC_BITS) - 1;
        int32_t floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE s8x24 fract(s8x24 x) {
        constexpr int32_t frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE s8x24 abs(s8x24 x) {
        return from_raw(x.mValue < 0 ? -x.mValue : x.mValue);
    }

    static FASTLED_FORCE_INLINE s8x24 sign(s8x24 x) {
        constexpr s8x24 pos_one(1.0f);
        constexpr s8x24 neg_one(-1.0f);
        if (x.mValue > 0) return pos_one;
        if (x.mValue < 0) return neg_one;
        return s8x24();
    }

    static FASTLED_FORCE_INLINE s8x24 lerp(s8x24 a, s8x24 b, s8x24 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE s8x24 clamp(s8x24 x, s8x24 lo, s8x24 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE s8x24 step(s8x24 edge, s8x24 x) {
        constexpr s8x24 one(1.0f);
        return x < edge ? s8x24() : one;
    }

    static FASTLED_FORCE_INLINE s8x24 smoothstep(s8x24 edge0, s8x24 edge1, s8x24 x) {
        constexpr s8x24 zero(0.0f);
        constexpr s8x24 one(1.0f);
        constexpr s8x24 two(2.0f);
        constexpr s8x24 three(3.0f);
        s8x24 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    // ---- Inverse Trigonometry (pure fixed-point) ----------------------------

    static FASTLED_FORCE_INLINE s8x24 atan(s8x24 x) {
        constexpr s8x24 one(1.0f);
        constexpr s8x24 pi_over_2(1.5707963f);
        bool neg = x.mValue < 0;
        s8x24 ax = abs(x);
        s8x24 result;
        if (ax <= one) {
            result = atan_unit(ax);
        } else {
            result = pi_over_2 - atan_unit(one / ax);
        }
        return neg ? -result : result;
    }

    static FASTLED_FORCE_INLINE s8x24 atan2(s8x24 y, s8x24 x) {
        constexpr s8x24 pi(3.1415926f);
        constexpr s8x24 pi_over_2(1.5707963f);
        if (x.mValue == 0 && y.mValue == 0) return s8x24();
        if (x.mValue == 0) return y.mValue > 0 ? pi_over_2 : -pi_over_2;
        if (y.mValue == 0) return x.mValue > 0 ? s8x24() : pi;
        s8x24 ax = abs(x);
        s8x24 ay = abs(y);
        s8x24 a;
        if (ax >= ay) {
            a = atan_unit(ay / ax);
        } else {
            a = pi_over_2 - atan_unit(ax / ay);
        }
        if (x.mValue < 0) a = pi - a;
        if (y.mValue < 0) a = -a;
        return a;
    }

    static FASTLED_FORCE_INLINE s8x24 asin(s8x24 x) {
        constexpr s8x24 one(1.0f);
        return atan2(x, sqrt(one - x * x));
    }

    static FASTLED_FORCE_INLINE s8x24 acos(s8x24 x) {
        constexpr s8x24 one(1.0f);
        return atan2(sqrt(one - x * x), x);
    }

    static FASTLED_FORCE_INLINE s8x24 sqrt(s8x24 x) {
        if (x.mValue <= 0) return s8x24();
        return from_raw(static_cast<int32_t>(
            fl::isqrt64(static_cast<uint64_t>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE s8x24 rsqrt(s8x24 x) {
        s8x24 s = sqrt(x);
        if (s.mValue == 0) return s8x24();
        return from_raw(static_cast<int32_t>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE s8x24 pow(s8x24 base, s8x24 exp) {
        if (base.mValue <= 0) return s8x24();
        constexpr s8x24 one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
        return exp2_fp(exp * log2_fp(base));
    }

    // ---- Trigonometry ------------------------------------------------------

    static FASTLED_FORCE_INLINE s8x24 sin(s8x24 angle) {
        return from_raw(fl::sin32(angle_to_a24(angle)) >> 7);
    }

    static FASTLED_FORCE_INLINE s8x24 cos(s8x24 angle) {
        return from_raw(fl::cos32(angle_to_a24(angle)) >> 7);
    }

    // Combined sin+cos from s8x24 radians. Output in s8x24 [-1, 1].
    static FASTLED_FORCE_INLINE void sincos(s8x24 angle, s8x24 &out_sin,
                                            s8x24 &out_cos) {
        uint32_t a24 = angle_to_a24(angle);
        out_sin = from_raw(fl::sin32(a24) >> 7);
        out_cos = from_raw(fl::cos32(a24) >> 7);
    }

  private:
    int32_t mValue = 0;

    // Returns 0-based position of highest set bit, or -1 if v==0.
    static FASTLED_FORCE_INLINE int highest_bit(uint32_t v) {
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
    static FASTLED_FORCE_INLINE s8x24 log2_fp(s8x24 x) {
        uint32_t val = static_cast<uint32_t>(x.mValue);
        int msb = highest_bit(val);
        int32_t int_part = msb - FRAC_BITS;
        int32_t t;
        if (msb >= FRAC_BITS) {
            t = static_cast<int32_t>(
                (val >> (msb - FRAC_BITS)) - (1u << FRAC_BITS));
        } else {
            t = static_cast<int32_t>(
                (val << (FRAC_BITS - msb)) - (1u << FRAC_BITS));
        }
        s8x24 tf = from_raw(t);
        constexpr s8x24 c0(1.4284f);
        constexpr s8x24 c1(-0.5765f);
        constexpr s8x24 c2(0.1481f);
        s8x24 frac_part = tf * (c0 + tf * (c1 + tf * c2));
        return from_raw((int_part << FRAC_BITS) + frac_part.mValue);
    }

    // Fixed-point 2^x.
    static FASTLED_FORCE_INLINE s8x24 exp2_fp(s8x24 x) {
        s8x24 fl_val = floor(x);
        s8x24 fr = x - fl_val;
        int32_t n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS - 1) return from_raw(0x7FFFFFFF);
        if (n < -FRAC_BITS) return s8x24();
        int32_t int_pow;
        if (n >= 0) {
            int_pow = static_cast<int32_t>(1u << FRAC_BITS) << n;
        } else {
            int_pow = static_cast<int32_t>(1u << FRAC_BITS) >> (-n);
        }
        constexpr s8x24 one(1.0f);
        constexpr s8x24 d0(0.69314718f);
        constexpr s8x24 d1(0.24022651f);
        constexpr s8x24 d2(0.05550411f);
        s8x24 frac_pow = one + fr * (d0 + fr * (d1 + fr * d2));
        int64_t result =
            (static_cast<int64_t>(int_pow) * frac_pow.mValue) >> FRAC_BITS;
        return from_raw(static_cast<int32_t>(result));
    }

    // Converts s8x24 radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE uint32_t angle_to_a24(s8x24 angle) {
        // 256/(2*PI) in s8x24 — converts radians to sin32/cos32 format.
        static constexpr int32_t RAD_TO_24 = 2670177;
        return static_cast<uint32_t>(
            (static_cast<int64_t>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }

    // Polynomial atan for t in [0, 1]. Returns [0, π/4].
    static FASTLED_FORCE_INLINE s8x24 atan_unit(s8x24 t) {
        constexpr s8x24 pi_over_4(0.7853981f);
        constexpr s8x24 c1(0.2447f);
        constexpr s8x24 c2(0.0663f);
        constexpr s8x24 one(1.0f);
        return pi_over_4 * t - t * (t - one) * (c1 + c2 * t);
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
