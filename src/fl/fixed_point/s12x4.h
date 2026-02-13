#pragma once

// Signed 12.4 fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Signed 12.4 fixed-point value type.
class s12x4 {
  public:
    static constexpr int INT_BITS = 12;
    static constexpr int FRAC_BITS = 4;

    // ---- Construction ------------------------------------------------------

    constexpr s12x4() = default;

    explicit constexpr s12x4(float f)
        : mValue(static_cast<i16>(f * (1 << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE s12x4 from_raw(i16 raw) {
        s12x4 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    i16 raw() const { return mValue; }
    i16 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE s12x4 operator*(s12x4 b) const {
        return from_raw(static_cast<i16>(
            (static_cast<i32>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE s12x4 operator/(s12x4 b) const {
        return from_raw(static_cast<i16>(
            (static_cast<i32>(mValue) * (static_cast<i32>(1) << FRAC_BITS)) / b.mValue));
    }

    FASTLED_FORCE_INLINE s12x4 operator+(s12x4 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE s12x4 operator-(s12x4 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE s12x4 operator-() const {
        return from_raw(-mValue);
    }

    FASTLED_FORCE_INLINE s12x4 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE s12x4 operator*(i16 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE s12x4 operator*(i16 scalar, s12x4 fp) {
        return s12x4::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(s12x4 b) const { return mValue < b.mValue; }
    bool operator>(s12x4 b) const { return mValue > b.mValue; }
    bool operator<=(s12x4 b) const { return mValue <= b.mValue; }
    bool operator>=(s12x4 b) const { return mValue >= b.mValue; }
    bool operator==(s12x4 b) const { return mValue == b.mValue; }
    bool operator!=(s12x4 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE s12x4 mod(s12x4 a, s12x4 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE s12x4 floor(s12x4 x) {
        constexpr i16 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE s12x4 ceil(s12x4 x) {
        constexpr i16 frac_mask = (1 << FRAC_BITS) - 1;
        i16 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE s12x4 fract(s12x4 x) {
        constexpr i16 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE s12x4 abs(s12x4 x) {
        return from_raw(x.mValue < 0 ? -x.mValue : x.mValue);
    }

    static FASTLED_FORCE_INLINE s12x4 sign(s12x4 x) {
        constexpr s12x4 pos_one(1.0f);
        constexpr s12x4 neg_one(-1.0f);
        if (x.mValue > 0) return pos_one;
        if (x.mValue < 0) return neg_one;
        return s12x4();
    }

    static FASTLED_FORCE_INLINE s12x4 lerp(s12x4 a, s12x4 b, s12x4 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE s12x4 clamp(s12x4 x, s12x4 lo, s12x4 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE s12x4 step(s12x4 edge, s12x4 x) {
        constexpr s12x4 one(1.0f);
        return x < edge ? s12x4() : one;
    }

    static FASTLED_FORCE_INLINE s12x4 smoothstep(s12x4 edge0, s12x4 edge1, s12x4 x) {
        constexpr s12x4 zero(0.0f);
        constexpr s12x4 one(1.0f);
        constexpr s12x4 two(2.0f);
        constexpr s12x4 three(3.0f);
        s12x4 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    // ---- Inverse Trigonometry (pure fixed-point) ----------------------------

    static FASTLED_FORCE_INLINE s12x4 atan(s12x4 x) {
        constexpr s12x4 one(1.0f);
        constexpr s12x4 pi_over_2(1.5707963f);
        bool neg = x.mValue < 0;
        s12x4 ax = abs(x);
        s12x4 result;
        if (ax <= one) {
            result = atan_unit(ax);
        } else {
            result = pi_over_2 - atan_unit(one / ax);
        }
        return neg ? -result : result;
    }

    static FASTLED_FORCE_INLINE s12x4 atan2(s12x4 y, s12x4 x) {
        constexpr s12x4 pi(3.1415926f);
        constexpr s12x4 pi_over_2(1.5707963f);
        if (x.mValue == 0 && y.mValue == 0) return s12x4();
        if (x.mValue == 0) return y.mValue > 0 ? pi_over_2 : -pi_over_2;
        if (y.mValue == 0) return x.mValue > 0 ? s12x4() : pi;
        s12x4 ax = abs(x);
        s12x4 ay = abs(y);
        s12x4 a;
        if (ax >= ay) {
            a = atan_unit(ay / ax);
        } else {
            a = pi_over_2 - atan_unit(ax / ay);
        }
        if (x.mValue < 0) a = pi - a;
        if (y.mValue < 0) a = -a;
        return a;
    }

    static FASTLED_FORCE_INLINE s12x4 asin(s12x4 x) {
        constexpr s12x4 one(1.0f);
        return atan2(x, sqrt(one - x * x));
    }

    static FASTLED_FORCE_INLINE s12x4 acos(s12x4 x) {
        constexpr s12x4 one(1.0f);
        return atan2(sqrt(one - x * x), x);
    }

    static FASTLED_FORCE_INLINE s12x4 sqrt(s12x4 x) {
        if (x.mValue <= 0) return s12x4();
        return from_raw(static_cast<i16>(
            fl::isqrt32(static_cast<u32>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE s12x4 rsqrt(s12x4 x) {
        s12x4 s = sqrt(x);
        if (s.mValue == 0) return s12x4();
        return from_raw(static_cast<i16>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE s12x4 pow(s12x4 base, s12x4 exp) {
        if (base.mValue <= 0) return s12x4();
        constexpr s12x4 one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
        return exp2_fp(exp * log2_fp(base));
    }

    // ---- Trigonometry ------------------------------------------------------

    static FASTLED_FORCE_INLINE s12x4 sin(s12x4 angle) {
        return from_raw(static_cast<i16>(fl::sin32(angle_to_a24(angle)) >> 27));
    }

    static FASTLED_FORCE_INLINE s12x4 cos(s12x4 angle) {
        return from_raw(static_cast<i16>(fl::cos32(angle_to_a24(angle)) >> 27));
    }

    // Combined sin+cos from s12x4 radians. Output in s12x4 [-1, 1].
    static FASTLED_FORCE_INLINE void sincos(s12x4 angle, s12x4 &out_sin,
                                            s12x4 &out_cos) {
        u32 a24 = angle_to_a24(angle);
        out_sin = from_raw(static_cast<i16>(fl::sin32(a24) >> 27));
        out_cos = from_raw(static_cast<i16>(fl::cos32(a24) >> 27));
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
    // Uses 4-term minimax polynomial for log2(1+t), t in [0,1).
    // Horner evaluation uses i32 intermediates (12 frac bits) to minimize
    // rounding error, then converts back to 4 frac bits.
    static FASTLED_FORCE_INLINE s12x4 log2_fp(s12x4 x) {
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
        // 4-term minimax coefficients for log2(1+t), t in [0,1).
        // Stored as i32 with 12 fractional bits. Max product ~2^21, fits i32 comfortably.
        constexpr int IFRAC = 12;
        constexpr i32 c0 = 5907;    // 1.44179 * 2^12
        constexpr i32 c1 = -2864;   // -0.69907 * 2^12
        constexpr i32 c2 = 1489;    // 0.36348 * 2^12
        constexpr i32 c3 = -437;    // -0.10660 * 2^12
        // Extend t from 4 to 12 frac bits.
        i32 t12 = static_cast<i32>(t) << (IFRAC - FRAC_BITS);
        // Horner: t * (c0 + t * (c1 + t * (c2 + t * c3)))
        i32 acc = c3;
        acc = c2 + ((acc * t12) >> IFRAC);
        acc = c1 + ((acc * t12) >> IFRAC);
        acc = c0 + ((acc * t12) >> IFRAC);
        i32 frac_part = (acc * t12) >> IFRAC;
        // Convert from 12 frac bits back to 4.
        i16 frac4 = static_cast<i16>(frac_part >> (IFRAC - FRAC_BITS));
        return from_raw(static_cast<i16>((int_part << FRAC_BITS) + frac4));
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses i32 intermediates (12 frac bits) to minimize
    // rounding error, then converts back to 4 frac bits.
    static FASTLED_FORCE_INLINE s12x4 exp2_fp(s12x4 x) {
        s12x4 fl_val = floor(x);
        s12x4 fr = x - fl_val;
        i32 n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS - 1) return from_raw(0x7FFF);
        if (n < -FRAC_BITS) return s12x4();
        i32 int_pow;
        if (n >= 0) {
            int_pow = static_cast<i32>(1u << FRAC_BITS) << n;
        } else {
            int_pow = static_cast<i32>(1u << FRAC_BITS) >> (-n);
        }
        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as i32 with 12 fractional bits.
        constexpr int IFRAC = 12;
        constexpr i32 d0 = 2839;    // 0.69316 * 2^12
        constexpr i32 d1 = 986;     // 0.24071 * 2^12
        constexpr i32 d2 = 219;     // 0.05336 * 2^12
        constexpr i32 d3 = 52;      // 0.01276 * 2^12
        // Extend fr from 4 to 12 frac bits.
        i32 fr12 = static_cast<i32>(fr.mValue) << (IFRAC - FRAC_BITS);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        i32 acc = d3;
        acc = d2 + ((acc * fr12) >> IFRAC);
        acc = d1 + ((acc * fr12) >> IFRAC);
        acc = d0 + ((acc * fr12) >> IFRAC);
        constexpr i32 one12 = 1 << IFRAC;
        i32 frac_pow12 = one12 + ((acc * fr12) >> IFRAC);
        // Convert from 12 frac bits to 4 frac bits, then scale by int_pow.
        i32 frac_pow4 = frac_pow12 >> (IFRAC - FRAC_BITS);
        i32 result =
            (int_pow * frac_pow4) >> FRAC_BITS;
        return from_raw(static_cast<i16>(result));
    }

    // Converts s12x4 radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE u32 angle_to_a24(s12x4 angle) {
        // 256/(2*PI) in s16x16 — converts radians to sin32/cos32 format.
        static constexpr i32 RAD_TO_24 = 2670177;
        return static_cast<u32>(
            (static_cast<i64>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }

    // Polynomial atan for t in [0, 1]. Returns [0, π/4].
    // 7th-order minimax: atan(t) ≈ t * (c0 + t² * (c1 + t² * (c2 + t² * c3)))
    // Coefficients optimized via coordinate descent on s16x16 quantization grid.
    static FASTLED_FORCE_INLINE s12x4 atan_unit(s12x4 t) {
        constexpr s12x4 c0(0.9998779297f);
        constexpr s12x4 c1(-0.3269348145f);
        constexpr s12x4 c2(0.1594085693f);
        constexpr s12x4 c3(-0.0472106934f);
        s12x4 t2 = t * t;
        return t * (c0 + t2 * (c1 + t2 * (c2 + t2 * c3)));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
