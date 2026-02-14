#pragma once

// Unsigned 12.4 fixed-point arithmetic.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Unsigned 12.4 fixed-point value type.
class u12x4 {
  public:
    static constexpr int INT_BITS = 12;
    static constexpr int FRAC_BITS = 4;

    // ---- Construction ------------------------------------------------------

    constexpr u12x4() = default;

    explicit constexpr u12x4(float f)
        : mValue(static_cast<u16>(f * (1 << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE u12x4 from_raw(u16 raw) {
        u12x4 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    u16 raw() const { return mValue; }
    u16 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE u12x4 operator*(u12x4 b) const {
        return from_raw(static_cast<u16>(
            (static_cast<u32>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE u12x4 operator/(u12x4 b) const {
        return from_raw(static_cast<u16>(
            (static_cast<u32>(mValue) * (static_cast<u32>(1) << FRAC_BITS)) / b.mValue));
    }

    FASTLED_FORCE_INLINE u12x4 operator+(u12x4 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE u12x4 operator-(u12x4 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE u12x4 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    FASTLED_FORCE_INLINE u12x4 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Compound assignment operators -------------------------------------

    FASTLED_FORCE_INLINE u12x4& operator+=(u12x4 b) {
        mValue += b.mValue;
        return *this;
    }

    FASTLED_FORCE_INLINE u12x4& operator-=(u12x4 b) {
        mValue -= b.mValue;
        return *this;
    }

    FASTLED_FORCE_INLINE u12x4& operator*=(u12x4 b) {
        mValue = static_cast<u16>(
            (static_cast<u32>(mValue) * b.mValue) >> FRAC_BITS);
        return *this;
    }

    FASTLED_FORCE_INLINE u12x4& operator/=(u12x4 b) {
        mValue = static_cast<u16>(
            (static_cast<u32>(mValue) * (static_cast<u32>(1) << FRAC_BITS)) / b.mValue);
        return *this;
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE u12x4 operator*(u16 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE u12x4 operator*(u16 scalar, u12x4 fp) {
        return u12x4::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(u12x4 b) const { return mValue < b.mValue; }
    bool operator>(u12x4 b) const { return mValue > b.mValue; }
    bool operator<=(u12x4 b) const { return mValue <= b.mValue; }
    bool operator>=(u12x4 b) const { return mValue >= b.mValue; }
    bool operator==(u12x4 b) const { return mValue == b.mValue; }
    bool operator!=(u12x4 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE u12x4 mod(u12x4 a, u12x4 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE u12x4 floor(u12x4 x) {
        constexpr u16 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE u12x4 ceil(u12x4 x) {
        constexpr u16 frac_mask = (1 << FRAC_BITS) - 1;
        u16 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE u12x4 fract(u12x4 x) {
        constexpr u16 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE u12x4 abs(u12x4 x) {
        // Unsigned values are always non-negative
        return x;
    }

    static FASTLED_FORCE_INLINE u12x4 min(u12x4 a, u12x4 b) {
        return a < b ? a : b;
    }

    static FASTLED_FORCE_INLINE u12x4 max(u12x4 a, u12x4 b) {
        return a > b ? a : b;
    }

    static FASTLED_FORCE_INLINE u12x4 lerp(u12x4 a, u12x4 b, u12x4 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE u12x4 clamp(u12x4 x, u12x4 lo, u12x4 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE u12x4 step(u12x4 edge, u12x4 x) {
        constexpr u12x4 one(1.0f);
        return x < edge ? u12x4() : one;
    }

    static FASTLED_FORCE_INLINE u12x4 smoothstep(u12x4 edge0, u12x4 edge1, u12x4 x) {
        constexpr u12x4 zero(0.0f);
        constexpr u12x4 one(1.0f);
        constexpr u12x4 two(2.0f);
        constexpr u12x4 three(3.0f);
        u12x4 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    static FASTLED_FORCE_INLINE u12x4 sqrt(u12x4 x) {
        if (x.mValue == 0) return u12x4();
        return from_raw(static_cast<u16>(
            fl::isqrt32(static_cast<u32>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE u12x4 rsqrt(u12x4 x) {
        u12x4 s = sqrt(x);
        if (s.mValue == 0) return u12x4();
        return from_raw(static_cast<u16>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE u12x4 pow(u12x4 base, u12x4 exp) {
        if (base.mValue == 0) return u12x4();
        constexpr u12x4 one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
        return exp2_fp(exp * log2_fp(base));
    }

  private:
    u16 mValue = 0;

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
    // Horner evaluation uses u32 intermediates (12 frac bits) to minimize
    // rounding error, then converts back to 4 frac bits.
    static FASTLED_FORCE_INLINE u12x4 log2_fp(u12x4 x) {
        u32 val = static_cast<u32>(x.mValue);
        int msb = highest_bit(val);
        u32 int_part = msb - FRAC_BITS;
        u32 t;
        if (msb >= FRAC_BITS) {
            t = static_cast<u32>(
                (val >> (msb - FRAC_BITS)) - (1u << FRAC_BITS));
        } else {
            t = static_cast<u32>(
                (val << (FRAC_BITS - msb)) - (1u << FRAC_BITS));
        }
        // 4-term minimax coefficients for log2(1+t), t in [0,1).
        // Stored as u32 with 12 fractional bits. Max product ~2^21, fits u32 comfortably.
        constexpr int IFRAC = 12;
        constexpr u32 c0 = 5907;    // 1.44179 * 2^12
        constexpr u32 c1 = 2864;    // 0.69907 * 2^12 (no negation for unsigned)
        constexpr u32 c2 = 1489;    // 0.36348 * 2^12
        constexpr u32 c3 = 437;     // 0.10660 * 2^12 (no negation for unsigned)
        // Extend t from 4 to 12 frac bits.
        u32 t12 = static_cast<u32>(t) << (IFRAC - FRAC_BITS);
        // Simplified Horner for unsigned: t * (c0 + t * c1 + t^2 * c2 + t^3 * c3)
        u32 acc = c3;
        acc = c2 + ((acc * t12) >> IFRAC);
        acc = c1 + ((acc * t12) >> IFRAC);
        acc = c0 + ((acc * t12) >> IFRAC);
        u32 frac_part = (acc * t12) >> IFRAC;
        // Convert from 12 frac bits back to 4.
        u16 frac4 = static_cast<u16>(frac_part >> (IFRAC - FRAC_BITS));
        return from_raw(static_cast<u16>((int_part << FRAC_BITS) + frac4));
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses u32 intermediates (12 frac bits) to minimize
    // rounding error, then converts back to 4 frac bits.
    static FASTLED_FORCE_INLINE u12x4 exp2_fp(u12x4 x) {
        u12x4 fl_val = floor(x);
        u12x4 fr = x - fl_val;
        u32 n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS - 1) return from_raw(0xFFFF);
        u32 int_pow;
        if (n >= 0) {
            int_pow = static_cast<u32>(1u << FRAC_BITS) << n;
        } else {
            int_pow = static_cast<u32>(1u << FRAC_BITS) >> (-static_cast<int>(n));
        }
        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as u32 with 12 fractional bits.
        constexpr int IFRAC = 12;
        constexpr u32 d0 = 2839;    // 0.69316 * 2^12
        constexpr u32 d1 = 986;     // 0.24071 * 2^12
        constexpr u32 d2 = 219;     // 0.05336 * 2^12
        constexpr u32 d3 = 52;      // 0.01276 * 2^12
        // Extend fr from 4 to 12 frac bits.
        u32 fr12 = static_cast<u32>(fr.mValue) << (IFRAC - FRAC_BITS);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        u32 acc = d3;
        acc = d2 + ((acc * fr12) >> IFRAC);
        acc = d1 + ((acc * fr12) >> IFRAC);
        acc = d0 + ((acc * fr12) >> IFRAC);
        constexpr u32 one12 = 1 << IFRAC;
        u32 frac_pow12 = one12 + ((acc * fr12) >> IFRAC);
        // Convert from 12 frac bits to 4 frac bits, then scale by int_pow.
        u32 frac_pow4 = frac_pow12 >> (IFRAC - FRAC_BITS);
        u32 result =
            (int_pow * frac_pow4) >> FRAC_BITS;
        return from_raw(static_cast<u16>(result));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
