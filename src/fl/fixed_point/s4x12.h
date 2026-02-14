#pragma once

// Signed 4.12 fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.

#include "fl/int.h"
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
        : mValue(static_cast<i16>(f * (static_cast<i16>(1) << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE s4x12 from_raw(i16 raw) {
        s4x12 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    i16 raw() const { return mValue; }
    i16 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (static_cast<i16>(1) << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE s4x12 operator*(s4x12 b) const {
        return from_raw(static_cast<i16>(
            (static_cast<i32>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE s4x12 operator/(s4x12 b) const {
        return from_raw(static_cast<i16>(
            (static_cast<i32>(mValue) * (static_cast<i32>(1) << FRAC_BITS)) / b.mValue));
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

    static FASTLED_FORCE_INLINE int sign(s4x12 x) {
        if (x.mValue > 0) return 1;
        if (x.mValue < 0) return -1;
        return 0;
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

    // ---- Member function versions (operate on *this) -----------------------

    FASTLED_FORCE_INLINE s4x12 floor() const {
        return floor(*this);
    }

    FASTLED_FORCE_INLINE s4x12 ceil() const {
        return ceil(*this);
    }

    FASTLED_FORCE_INLINE s4x12 fract() const {
        return fract(*this);
    }

    FASTLED_FORCE_INLINE s4x12 abs() const {
        return abs(*this);
    }

    FASTLED_FORCE_INLINE int sign() const {
        return sign(*this);
    }

    FASTLED_FORCE_INLINE s4x12 sin() const {
        return sin(*this);
    }

    FASTLED_FORCE_INLINE s4x12 cos() const {
        return cos(*this);
    }

    FASTLED_FORCE_INLINE s4x12 atan() const {
        return atan(*this);
    }

    FASTLED_FORCE_INLINE s4x12 asin() const {
        return asin(*this);
    }

    FASTLED_FORCE_INLINE s4x12 acos() const {
        return acos(*this);
    }

    FASTLED_FORCE_INLINE s4x12 sqrt() const {
        return sqrt(*this);
    }

    FASTLED_FORCE_INLINE s4x12 rsqrt() const {
        return rsqrt(*this);
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
    // Uses 4-term minimax polynomial for log2(1+t), t in [0,1).
    // Horner evaluation uses i32 intermediates (20 frac bits) to minimize
    // rounding error, then converts back to 12 frac bits.
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
        // 4-term minimax coefficients for log2(1+t), t in [0,1).
        // Stored as i32 with 20 fractional bits. Max product ~2^33, fits i64 intermediate.
        constexpr int IFRAC = 20;
        constexpr i32 c0 = 1512456;   // 1.44179 * 2^20
        constexpr i32 c1 = -733024;   // -0.69907 * 2^20
        constexpr i32 c2 = 381136;    // 0.36348 * 2^20
        constexpr i32 c3 = -111776;   // -0.10660 * 2^20
        // Extend t from 12 to 20 frac bits.
        i32 t20 = static_cast<i32>(t) << (IFRAC - FRAC_BITS);
        // Horner: t * (c0 + t * (c1 + t * (c2 + t * c3)))
        i32 acc = c3;
        acc = c2 + static_cast<i32>((static_cast<i64>(acc) * t20) >> IFRAC);
        acc = c1 + static_cast<i32>((static_cast<i64>(acc) * t20) >> IFRAC);
        acc = c0 + static_cast<i32>((static_cast<i64>(acc) * t20) >> IFRAC);
        i32 frac_part = static_cast<i32>((static_cast<i64>(acc) * t20) >> IFRAC);
        // Convert from 20 frac bits back to 12.
        i16 frac12 = static_cast<i16>(frac_part >> (IFRAC - FRAC_BITS));
        return from_raw(static_cast<i16>((int_part << FRAC_BITS) + frac12));
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses i32 intermediates (20 frac bits) to minimize
    // rounding error, then converts back to 12 frac bits.
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
        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as i32 with 20 fractional bits.
        constexpr int IFRAC = 20;
        constexpr i32 d0 = 726836;    // 0.69316 * 2^20
        constexpr i32 d1 = 252400;    // 0.24071 * 2^20
        constexpr i32 d2 = 55952;     // 0.05336 * 2^20
        constexpr i32 d3 = 13376;     // 0.01276 * 2^20
        // Extend fr from 12 to 20 frac bits.
        i32 fr20 = static_cast<i32>(fr.mValue) << (IFRAC - FRAC_BITS);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        i32 acc = d3;
        acc = d2 + static_cast<i32>((static_cast<i64>(acc) * fr20) >> IFRAC);
        acc = d1 + static_cast<i32>((static_cast<i64>(acc) * fr20) >> IFRAC);
        acc = d0 + static_cast<i32>((static_cast<i64>(acc) * fr20) >> IFRAC);
        constexpr i32 one20 = 1 << IFRAC;
        i32 frac_pow20 = one20 + static_cast<i32>((static_cast<i64>(acc) * fr20) >> IFRAC);
        // Convert from 20 frac bits to 12 frac bits, then scale by int_pow.
        i32 frac_pow12 = frac_pow20 >> (IFRAC - FRAC_BITS);
        i32 result =
            (int_pow * frac_pow12) >> FRAC_BITS;
        return from_raw(static_cast<i16>(result));
    }

    // Converts s4x12 radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE u32 angle_to_a24(s4x12 angle) {
        // 256/(2*PI) in s4x12 — converts radians to sin32/cos32 format.
        static constexpr i32 RAD_TO_24 = 2670177;
        return static_cast<u32>(
            (static_cast<i64>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }

    // Polynomial atan for t in [0, 1]. Returns [0, π/4].
    // 7th-order minimax: atan(t) ≈ t * (c0 + t² * (c1 + t² * (c2 + t² * c3)))
    // Coefficients optimized via coordinate descent on s16x16 quantization grid.
    static FASTLED_FORCE_INLINE s4x12 atan_unit(s4x12 t) {
        constexpr s4x12 c0(0.9998779297f);
        constexpr s4x12 c1(-0.3269348145f);
        constexpr s4x12 c2(0.1594085693f);
        constexpr s4x12 c3(-0.0472106934f);
        s4x12 t2 = t * t;
        return t * (c0 + t2 * (c1 + t2 * (c2 + t2 * c3)));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
