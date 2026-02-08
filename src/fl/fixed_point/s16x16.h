#pragma once

// Signed 16.16 fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Signed 16.16 fixed-point value type.
class s16x16 {
  public:
    static constexpr int INT_BITS = 16;
    static constexpr int FRAC_BITS = 16;

    // ---- Construction ------------------------------------------------------

    constexpr s16x16() = default;

    explicit constexpr s16x16(float f)
        : mValue(static_cast<i32>(f * (1 << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE s16x16 from_raw(i32 raw) {
        s16x16 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    i32 raw() const { return mValue; }
    i32 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE s16x16 operator*(s16x16 b) const {
        return from_raw(static_cast<i32>(
            (static_cast<i64>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE s16x16 operator/(s16x16 b) const {
        return from_raw(static_cast<i32>(
            (static_cast<i64>(mValue) << FRAC_BITS) / b.mValue));
    }

    FASTLED_FORCE_INLINE s16x16 operator+(s16x16 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE s16x16 operator-(s16x16 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE s16x16 operator-() const {
        return from_raw(-mValue);
    }

    FASTLED_FORCE_INLINE s16x16 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE s16x16 operator*(i32 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE s16x16 operator*(i32 scalar, s16x16 fp) {
        return s16x16::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(s16x16 b) const { return mValue < b.mValue; }
    bool operator>(s16x16 b) const { return mValue > b.mValue; }
    bool operator<=(s16x16 b) const { return mValue <= b.mValue; }
    bool operator>=(s16x16 b) const { return mValue >= b.mValue; }
    bool operator==(s16x16 b) const { return mValue == b.mValue; }
    bool operator!=(s16x16 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE s16x16 mod(s16x16 a, s16x16 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE s16x16 floor(s16x16 x) {
        constexpr i32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE s16x16 ceil(s16x16 x) {
        constexpr i32 frac_mask = (1 << FRAC_BITS) - 1;
        i32 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE s16x16 fract(s16x16 x) {
        constexpr i32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE s16x16 abs(s16x16 x) {
        return from_raw(x.mValue < 0 ? -x.mValue : x.mValue);
    }

    static FASTLED_FORCE_INLINE s16x16 sign(s16x16 x) {
        constexpr s16x16 pos_one(1.0f);
        constexpr s16x16 neg_one(-1.0f);
        if (x.mValue > 0) return pos_one;
        if (x.mValue < 0) return neg_one;
        return s16x16();
    }

    static FASTLED_FORCE_INLINE s16x16 lerp(s16x16 a, s16x16 b, s16x16 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE s16x16 clamp(s16x16 x, s16x16 lo, s16x16 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE s16x16 step(s16x16 edge, s16x16 x) {
        constexpr s16x16 one(1.0f);
        return x < edge ? s16x16() : one;
    }

    static FASTLED_FORCE_INLINE s16x16 smoothstep(s16x16 edge0, s16x16 edge1, s16x16 x) {
        constexpr s16x16 zero(0.0f);
        constexpr s16x16 one(1.0f);
        constexpr s16x16 two(2.0f);
        constexpr s16x16 three(3.0f);
        s16x16 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    // ---- Inverse Trigonometry (pure fixed-point) ----------------------------

    static FASTLED_FORCE_INLINE s16x16 atan(s16x16 x) {
        constexpr s16x16 one(1.0f);
        constexpr s16x16 pi_over_2(1.5707963f);
        bool neg = x.mValue < 0;
        s16x16 ax = abs(x);
        s16x16 result;
        if (ax <= one) {
            result = atan_unit(ax);
        } else {
            result = pi_over_2 - atan_unit(one / ax);
        }
        return neg ? -result : result;
    }

    static FASTLED_FORCE_INLINE s16x16 atan2(s16x16 y, s16x16 x) {
        constexpr s16x16 pi(3.1415926f);
        constexpr s16x16 pi_over_2(1.5707963f);
        if (x.mValue == 0 && y.mValue == 0) return s16x16();
        if (x.mValue == 0) return y.mValue > 0 ? pi_over_2 : -pi_over_2;
        if (y.mValue == 0) return x.mValue > 0 ? s16x16() : pi;
        s16x16 ax = abs(x);
        s16x16 ay = abs(y);
        s16x16 a;
        if (ax >= ay) {
            a = atan_unit(ay / ax);
        } else {
            a = pi_over_2 - atan_unit(ax / ay);
        }
        if (x.mValue < 0) a = pi - a;
        if (y.mValue < 0) a = -a;
        return a;
    }

    static FASTLED_FORCE_INLINE s16x16 asin(s16x16 x) {
        constexpr s16x16 one(1.0f);
        return atan2(x, sqrt(one - x * x));
    }

    static FASTLED_FORCE_INLINE s16x16 acos(s16x16 x) {
        constexpr s16x16 one(1.0f);
        return atan2(sqrt(one - x * x), x);
    }

    static FASTLED_FORCE_INLINE s16x16 sqrt(s16x16 x) {
        if (x.mValue <= 0) return s16x16();
        return from_raw(static_cast<i32>(
            fl::isqrt64(static_cast<u64>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE s16x16 rsqrt(s16x16 x) {
        s16x16 s = sqrt(x);
        if (s.mValue == 0) return s16x16();
        return from_raw(static_cast<i32>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE s16x16 pow(s16x16 base, s16x16 exp) {
        if (base.mValue <= 0) return s16x16();
        constexpr s16x16 one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
        return exp2_fp(exp * log2_fp(base));
    }

    // ---- Trigonometry ------------------------------------------------------

    static FASTLED_FORCE_INLINE s16x16 sin(s16x16 angle) {
        return from_raw(fl::sin32(angle_to_a24(angle)) >> 15);
    }

    static FASTLED_FORCE_INLINE s16x16 cos(s16x16 angle) {
        return from_raw(fl::cos32(angle_to_a24(angle)) >> 15);
    }

    // Combined sin+cos from s16x16 radians. Output in s16x16 [-1, 1].
    // Uses sincos32 which shares angle decomposition for ~30% fewer ops.
    static FASTLED_FORCE_INLINE void sincos(s16x16 angle, s16x16 &out_sin,
                                            s16x16 &out_cos) {
        u32 a24 = angle_to_a24(angle);
        SinCos32 sc = fl::sincos32(a24);
        out_sin = from_raw(sc.sin_val >> 15);
        out_cos = from_raw(sc.cos_val >> 15);
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
    // Horner evaluation uses i64 intermediates (24 frac bits) to minimize
    // rounding error, then converts back to 16 frac bits.
    static FASTLED_FORCE_INLINE s16x16 log2_fp(s16x16 x) {
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
        // Stored as i64 with 24 fractional bits. Max product ~2^49, fits i64.
        constexpr int IFRAC = 24;
        constexpr int64_t c0 = 24189248LL;  // 1.44179 * 2^24
        constexpr int64_t c1 = -11728384LL; // -0.69907 * 2^24
        constexpr int64_t c2 = 6098176LL;   // 0.36348 * 2^24
        constexpr int64_t c3 = -1788416LL;  // -0.10660 * 2^24
        // Extend t from 16 to 24 frac bits.
        int64_t t24 = static_cast<int64_t>(t) << (IFRAC - FRAC_BITS);
        // Horner: t * (c0 + t * (c1 + t * (c2 + t * c3)))
        int64_t acc = c3;
        acc = c2 + ((acc * t24) >> IFRAC);
        acc = c1 + ((acc * t24) >> IFRAC);
        acc = c0 + ((acc * t24) >> IFRAC);
        int64_t frac_part = (acc * t24) >> IFRAC;
        // Convert from 24 frac bits back to 16.
        i32 frac16 = static_cast<i32>(frac_part >> (IFRAC - FRAC_BITS));
        return from_raw((int_part << FRAC_BITS) + frac16);
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses i64 intermediates (24 frac bits) to minimize
    // rounding error, then converts back to 16 frac bits.
    static FASTLED_FORCE_INLINE s16x16 exp2_fp(s16x16 x) {
        s16x16 fl_val = floor(x);
        s16x16 fr = x - fl_val;
        i32 n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS - 1) return from_raw(0x7FFFFFFF);
        if (n < -FRAC_BITS) return s16x16();
        i32 int_pow;
        if (n >= 0) {
            int_pow = static_cast<i32>(1u << FRAC_BITS) << n;
        } else {
            int_pow = static_cast<i32>(1u << FRAC_BITS) >> (-n);
        }
        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as i64 with 24 fractional bits. Max product ~2^48, fits i64.
        constexpr int IFRAC = 24;
        constexpr int64_t d0 = 11629376LL;  // 0.69316 * 2^24
        constexpr int64_t d1 = 4038400LL;   // 0.24071 * 2^24
        constexpr int64_t d2 = 895232LL;    // 0.05336 * 2^24
        constexpr int64_t d3 = 214016LL;    // 0.01276 * 2^24
        // Extend fr from 16 to 24 frac bits.
        int64_t fr24 = static_cast<int64_t>(fr.mValue) << (IFRAC - FRAC_BITS);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        int64_t acc = d3;
        acc = d2 + ((acc * fr24) >> IFRAC);
        acc = d1 + ((acc * fr24) >> IFRAC);
        acc = d0 + ((acc * fr24) >> IFRAC);
        constexpr int64_t one24 = 1LL << IFRAC;
        int64_t frac_pow24 = one24 + ((acc * fr24) >> IFRAC);
        // Convert from 24 frac bits to 16 frac bits, then scale by int_pow.
        i32 frac_pow16 = static_cast<i32>(frac_pow24 >> (IFRAC - FRAC_BITS));
        int64_t result =
            (static_cast<int64_t>(int_pow) * frac_pow16) >> FRAC_BITS;
        return from_raw(static_cast<i32>(result));
    }

    // Converts s16x16 radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE u32 angle_to_a24(s16x16 angle) {
        // 256/(2*PI) in s16x16 — converts radians to sin32/cos32 format.
        static constexpr i32 RAD_TO_24 = 2670177;
        return static_cast<u32>(
            (static_cast<i64>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }

    // Polynomial atan for t in [0, 1]. Returns [0, π/4].
    // 7th-order minimax: atan(t) ≈ t * (c0 + t² * (c1 + t² * (c2 + t² * c3)))
    // Coefficients optimized via coordinate descent on s16x16 quantization grid.
    static FASTLED_FORCE_INLINE s16x16 atan_unit(s16x16 t) {
        constexpr s16x16 c0(0.9998779297f);
        constexpr s16x16 c1(-0.3269348145f);
        constexpr s16x16 c2(0.1594085693f);
        constexpr s16x16 c3(-0.0472106934f);
        s16x16 t2 = t * t;
        return t * (c0 + t2 * (c1 + t2 * (c2 + t2 * c3)));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
