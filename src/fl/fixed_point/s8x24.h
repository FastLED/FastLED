#pragma once

// Signed 8.24 fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.

#include "fl/int.h"
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
    static constexpr i32 SCALE = static_cast<i32>(1) << FRAC_BITS;

    // ---- Construction ------------------------------------------------------

    constexpr s8x24() = default;

    explicit constexpr s8x24(float f)
        : mValue(static_cast<i32>(f * SCALE)) {}

    static FASTLED_FORCE_INLINE s8x24 from_raw(i32 raw) {
        s8x24 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    i32 raw() const { return mValue; }
    i32 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / SCALE; }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE s8x24 operator*(s8x24 b) const {
        return from_raw(static_cast<i32>(
            (static_cast<i64>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE s8x24 operator/(s8x24 b) const {
        return from_raw(static_cast<i32>(
            (static_cast<i64>(mValue) * SCALE) / b.mValue));
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

    FASTLED_FORCE_INLINE s8x24 operator*(i32 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE s8x24 operator*(i32 scalar, s8x24 fp) {
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
        constexpr i32 frac_mask = SCALE - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE s8x24 ceil(s8x24 x) {
        constexpr i32 frac_mask = SCALE - 1;
        i32 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += SCALE;
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE s8x24 fract(s8x24 x) {
        constexpr i32 frac_mask = SCALE - 1;
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
        return from_raw(static_cast<i32>(
            fl::isqrt64(static_cast<u64>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE s8x24 rsqrt(s8x24 x) {
        s8x24 s = sqrt(x);
        if (s.mValue == 0) return s8x24();
        return from_raw(SCALE) / s;
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
        u32 a24 = angle_to_a24(angle);
        out_sin = from_raw(fl::sin32(a24) >> 7);
        out_cos = from_raw(fl::cos32(a24) >> 7);
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
    // rounding error, then converts back to 24 frac bits.
    static FASTLED_FORCE_INLINE s8x24 log2_fp(s8x24 x) {
        u32 val = static_cast<u32>(x.mValue);
        int msb = highest_bit(val);
        i32 int_part = msb - FRAC_BITS;
        i32 t;
        if (msb >= FRAC_BITS) {
            t = static_cast<i32>(
                (val >> (msb - FRAC_BITS)) - SCALE);
        } else {
            t = static_cast<i32>(
                (val << (FRAC_BITS - msb)) - SCALE);
        }
        // 4-term minimax coefficients for log2(1+t), t in [0,1).
        // Stored as i64 with 24 fractional bits (same as FRAC_BITS).
        constexpr int IFRAC = 24;
        constexpr i64 c0 = 24189248LL;  // 1.44179 * 2^24
        constexpr i64 c1 = -11728384LL; // -0.69907 * 2^24
        constexpr i64 c2 = 6098176LL;   // 0.36348 * 2^24
        constexpr i64 c3 = -1788416LL;  // -0.10660 * 2^24
        // t is already at 24 frac bits (same as FRAC_BITS).
        i64 t24 = static_cast<i64>(t);
        // Horner: t * (c0 + t * (c1 + t * (c2 + t * c3)))
        i64 acc = c3;
        acc = c2 + ((acc * t24) >> IFRAC);
        acc = c1 + ((acc * t24) >> IFRAC);
        acc = c0 + ((acc * t24) >> IFRAC);
        i64 frac_part = (acc * t24) >> IFRAC;
        i32 frac24 = static_cast<i32>(frac_part);
        return from_raw((int_part << FRAC_BITS) + frac24);
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses i64 intermediates (24 frac bits) to minimize
    // rounding error, then converts back to 24 frac bits.
    static FASTLED_FORCE_INLINE s8x24 exp2_fp(s8x24 x) {
        s8x24 fl_val = floor(x);
        s8x24 fr = x - fl_val;
        i32 n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS - 1) return from_raw(0x7FFFFFFF);
        if (n < -FRAC_BITS) return s8x24();
        i32 int_pow;
        if (n >= 0) {
            int_pow = SCALE << n;
        } else {
            int_pow = SCALE >> (-n);
        }
        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as i64 with 24 fractional bits (same as FRAC_BITS).
        constexpr int IFRAC = 24;
        constexpr i64 d0 = 11629376LL;  // 0.69316 * 2^24
        constexpr i64 d1 = 4038400LL;   // 0.24071 * 2^24
        constexpr i64 d2 = 895232LL;    // 0.05336 * 2^24
        constexpr i64 d3 = 214016LL;    // 0.01276 * 2^24
        // fr is already at 24 frac bits (same as FRAC_BITS).
        i64 fr24 = static_cast<i64>(fr.mValue);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        i64 acc = d3;
        acc = d2 + ((acc * fr24) >> IFRAC);
        acc = d1 + ((acc * fr24) >> IFRAC);
        acc = d0 + ((acc * fr24) >> IFRAC);
        constexpr i64 one24 = 1LL << IFRAC;
        i64 frac_pow24 = one24 + ((acc * fr24) >> IFRAC);
        // Scale by int_pow (result stays at 24 frac bits).
        i64 result =
            (static_cast<i64>(int_pow) * frac_pow24) >> FRAC_BITS;
        return from_raw(static_cast<i32>(result));
    }

    // Converts s8x24 radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE u32 angle_to_a24(s8x24 angle) {
        // 256/(2*PI) in s8x24 — converts radians to sin32/cos32 format.
        static constexpr i32 RAD_TO_24 = 2670177;
        return static_cast<u32>(
            (static_cast<i64>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }

    // Polynomial atan for t in [0, 1]. Returns [0, π/4].
    // 7th-order minimax: atan(t) ≈ t * (c0 + t² * (c1 + t² * (c2 + t² * c3)))
    // Coefficients optimized via coordinate descent on s16x16 quantization grid.
    static FASTLED_FORCE_INLINE s8x24 atan_unit(s8x24 t) {
        constexpr s8x24 c0(0.9998779297f);
        constexpr s8x24 c1(-0.3269348145f);
        constexpr s8x24 c2(0.1594085693f);
        constexpr s8x24 c3(-0.0472106934f);
        s8x24 t2 = t * t;
        return t * (c0 + t2 * (c1 + t2 * (c2 + t2 * c3)));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
