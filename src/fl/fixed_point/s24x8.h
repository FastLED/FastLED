#pragma once

// Signed 24.8 fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.

#include "fl/int.h"
#include "fl/sin32.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Signed 24.8 fixed-point value type.
class s24x8 {
  public:
    static constexpr int INT_BITS = 24;
    static constexpr int FRAC_BITS = 8;

    // ---- Construction ------------------------------------------------------

    constexpr s24x8() = default;

    explicit constexpr s24x8(float f)
        : mValue(static_cast<i32>(f * (1 << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE s24x8 from_raw(i32 raw) {
        s24x8 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    i32 raw() const { return mValue; }
    i32 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE s24x8 operator*(s24x8 b) const {
        return from_raw(static_cast<i32>(
            (static_cast<i64>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE s24x8 operator/(s24x8 b) const {
        return from_raw(static_cast<i32>(
            (static_cast<i64>(mValue) << FRAC_BITS) / b.mValue));
    }

    FASTLED_FORCE_INLINE s24x8 operator+(s24x8 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE s24x8 operator-(s24x8 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE s24x8 operator-() const {
        return from_raw(-mValue);
    }

    FASTLED_FORCE_INLINE s24x8 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE s24x8 operator*(i32 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE s24x8 operator*(i32 scalar, s24x8 fp) {
        return s24x8::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(s24x8 b) const { return mValue < b.mValue; }
    bool operator>(s24x8 b) const { return mValue > b.mValue; }
    bool operator<=(s24x8 b) const { return mValue <= b.mValue; }
    bool operator>=(s24x8 b) const { return mValue >= b.mValue; }
    bool operator==(s24x8 b) const { return mValue == b.mValue; }
    bool operator!=(s24x8 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE s24x8 mod(s24x8 a, s24x8 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE s24x8 floor(s24x8 x) {
        constexpr i32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE s24x8 ceil(s24x8 x) {
        constexpr i32 frac_mask = (1 << FRAC_BITS) - 1;
        i32 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE s24x8 fract(s24x8 x) {
        constexpr i32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE s24x8 abs(s24x8 x) {
        return from_raw(x.mValue < 0 ? -x.mValue : x.mValue);
    }

    static FASTLED_FORCE_INLINE s24x8 sign(s24x8 x) {
        constexpr s24x8 pos_one(1.0f);
        constexpr s24x8 neg_one(-1.0f);
        if (x.mValue > 0) return pos_one;
        if (x.mValue < 0) return neg_one;
        return s24x8();
    }

    static FASTLED_FORCE_INLINE s24x8 lerp(s24x8 a, s24x8 b, s24x8 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE s24x8 clamp(s24x8 x, s24x8 lo, s24x8 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE s24x8 step(s24x8 edge, s24x8 x) {
        constexpr s24x8 one(1.0f);
        return x < edge ? s24x8() : one;
    }

    static FASTLED_FORCE_INLINE s24x8 smoothstep(s24x8 edge0, s24x8 edge1, s24x8 x) {
        constexpr s24x8 zero(0.0f);
        constexpr s24x8 one(1.0f);
        constexpr s24x8 two(2.0f);
        constexpr s24x8 three(3.0f);
        s24x8 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    // ---- Inverse Trigonometry (pure fixed-point) ----------------------------

    static FASTLED_FORCE_INLINE s24x8 atan(s24x8 x) {
        constexpr s24x8 one(1.0f);
        constexpr s24x8 pi_over_2(1.5707963f);
        bool neg = x.mValue < 0;
        s24x8 ax = abs(x);
        s24x8 result;
        if (ax <= one) {
            result = atan_unit(ax);
        } else {
            result = pi_over_2 - atan_unit(one / ax);
        }
        return neg ? -result : result;
    }

    static FASTLED_FORCE_INLINE s24x8 atan2(s24x8 y, s24x8 x) {
        constexpr s24x8 pi(3.1415926f);
        constexpr s24x8 pi_over_2(1.5707963f);
        if (x.mValue == 0 && y.mValue == 0) return s24x8();
        if (x.mValue == 0) return y.mValue > 0 ? pi_over_2 : -pi_over_2;
        if (y.mValue == 0) return x.mValue > 0 ? s24x8() : pi;
        s24x8 ax = abs(x);
        s24x8 ay = abs(y);
        s24x8 a;
        if (ax >= ay) {
            a = atan_unit(ay / ax);
        } else {
            a = pi_over_2 - atan_unit(ax / ay);
        }
        if (x.mValue < 0) a = pi - a;
        if (y.mValue < 0) a = -a;
        return a;
    }

    static FASTLED_FORCE_INLINE s24x8 asin(s24x8 x) {
        constexpr s24x8 one(1.0f);
        return atan2(x, sqrt(one - x * x));
    }

    static FASTLED_FORCE_INLINE s24x8 acos(s24x8 x) {
        constexpr s24x8 one(1.0f);
        return atan2(sqrt(one - x * x), x);
    }

    static FASTLED_FORCE_INLINE s24x8 sqrt(s24x8 x) {
        if (x.mValue <= 0) return s24x8();
        return from_raw(static_cast<i32>(
            fl::isqrt64(static_cast<u64>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE s24x8 rsqrt(s24x8 x) {
        s24x8 s = sqrt(x);
        if (s.mValue == 0) return s24x8();
        return from_raw(static_cast<i32>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE s24x8 pow(s24x8 base, s24x8 exp) {
        if (base.mValue <= 0) return s24x8();
        constexpr s24x8 one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
        return exp2_fp(exp * log2_fp(base));
    }

    // ---- Trigonometry ------------------------------------------------------

    static FASTLED_FORCE_INLINE s24x8 sin(s24x8 angle) {
        return from_raw(fl::sin32(angle_to_a24(angle)) >> (31 - FRAC_BITS));
    }

    static FASTLED_FORCE_INLINE s24x8 cos(s24x8 angle) {
        return from_raw(fl::cos32(angle_to_a24(angle)) >> (31 - FRAC_BITS));
    }

    // Combined sin+cos from s24x8 radians. Output in s24x8 [-1, 1].
    static FASTLED_FORCE_INLINE void sincos(s24x8 angle, s24x8 &out_sin,
                                            s24x8 &out_cos) {
        u32 a24 = angle_to_a24(angle);
        out_sin = from_raw(fl::sin32(a24) >> (31 - FRAC_BITS));
        out_cos = from_raw(fl::cos32(a24) >> (31 - FRAC_BITS));
    }

  private:
    i32 mValue = 0;

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
    // Horner evaluation uses i64 intermediates (16 frac bits) to minimize
    // rounding error, then converts back to 8 frac bits.
    static FASTLED_FORCE_INLINE s24x8 log2_fp(s24x8 x) {
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
        // Stored as i64 with 16 fractional bits.
        constexpr int IFRAC = 16;
        constexpr i64 c0 = 94528LL;    // 1.44179 * 2^16
        constexpr i64 c1 = -45814LL;   // -0.69907 * 2^16
        constexpr i64 c2 = 23821LL;    // 0.36348 * 2^16
        constexpr i64 c3 = -6986LL;    // -0.10660 * 2^16
        // Extend t from 8 to 16 frac bits.
        i64 t16 = static_cast<i64>(t) << (IFRAC - FRAC_BITS);
        // Horner: t * (c0 + t * (c1 + t * (c2 + t * c3)))
        i64 acc = c3;
        acc = c2 + ((acc * t16) >> IFRAC);
        acc = c1 + ((acc * t16) >> IFRAC);
        acc = c0 + ((acc * t16) >> IFRAC);
        i64 frac_part = (acc * t16) >> IFRAC;
        // Convert from 16 frac bits back to 8.
        i32 frac8 = static_cast<i32>(frac_part >> (IFRAC - FRAC_BITS));
        return from_raw((int_part << FRAC_BITS) + frac8);
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses i64 intermediates (16 frac bits) to minimize
    // rounding error, then converts back to 8 frac bits.
    static FASTLED_FORCE_INLINE s24x8 exp2_fp(s24x8 x) {
        s24x8 fl_val = floor(x);
        s24x8 fr = x - fl_val;
        i32 n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS - 1) return from_raw(0x7FFFFFFF);
        if (n < -FRAC_BITS) return s24x8();
        i32 int_pow;
        if (n >= 0) {
            int_pow = static_cast<i32>(1u << FRAC_BITS) << n;
        } else {
            int_pow = static_cast<i32>(1u << FRAC_BITS) >> (-n);
        }
        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as i64 with 16 fractional bits.
        constexpr int IFRAC = 16;
        constexpr i64 d0 = 45427LL;    // 0.69316 * 2^16
        constexpr i64 d1 = 15775LL;    // 0.24071 * 2^16
        constexpr i64 d2 = 3497LL;     // 0.05336 * 2^16
        constexpr i64 d3 = 836LL;      // 0.01276 * 2^16
        // Extend fr from 8 to 16 frac bits.
        i64 fr16 = static_cast<i64>(fr.mValue) << (IFRAC - FRAC_BITS);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        i64 acc = d3;
        acc = d2 + ((acc * fr16) >> IFRAC);
        acc = d1 + ((acc * fr16) >> IFRAC);
        acc = d0 + ((acc * fr16) >> IFRAC);
        constexpr i64 one16 = 1LL << IFRAC;
        i64 frac_pow16 = one16 + ((acc * fr16) >> IFRAC);
        // Convert from 16 frac bits to 8 frac bits, then scale by int_pow.
        i32 frac_pow8 = static_cast<i32>(frac_pow16 >> (IFRAC - FRAC_BITS));
        i64 result =
            (static_cast<i64>(int_pow) * frac_pow8) >> FRAC_BITS;
        return from_raw(static_cast<i32>(result));
    }

    // Converts s24x8 radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE u32 angle_to_a24(s24x8 angle) {
        // 2^24/(2*PI) — converts radians to sin32/cos32 format.
        static constexpr i32 RAD_TO_24 = 2670177;
        return static_cast<u32>(
            (static_cast<i64>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }

    // Polynomial atan for t in [0, 1]. Returns [0, π/4].
    // 7th-order minimax: atan(t) ≈ t * (c0 + t² * (c1 + t² * (c2 + t² * c3)))
    // Coefficients optimized via coordinate descent on s16x16 quantization grid.
    static FASTLED_FORCE_INLINE s24x8 atan_unit(s24x8 t) {
        constexpr s24x8 c0(0.9998779297f);
        constexpr s24x8 c1(-0.3269348145f);
        constexpr s24x8 c2(0.1594085693f);
        constexpr s24x8 c3(-0.0472106934f);
        s24x8 t2 = t * t;
        return t * (c0 + t2 * (c1 + t2 * (c2 + t2 * c3)));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
