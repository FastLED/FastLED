#pragma once

// Unsigned 4.12 fixed-point arithmetic.
// All operations are integer-only in the hot path.

#include "fl/int.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"
#include "fl/stl/type_traits.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Unsigned 4.12 fixed-point value type.
class u4x12 {
  public:
    static constexpr int INT_BITS = 4;
    static constexpr int FRAC_BITS = 12;

    // ---- Construction ------------------------------------------------------

    constexpr u4x12() = default;

    explicit constexpr u4x12(float f)
        : mValue(static_cast<u16>(f * (1 << FRAC_BITS))) {}

    // Auto-promotion from other fixed-point types
    template <typename OtherFP>
    constexpr u4x12(const OtherFP& other,
                    typename fl::enable_if<
                        (OtherFP::INT_BITS <= INT_BITS) &&
                        (OtherFP::FRAC_BITS <= FRAC_BITS) &&
                        (OtherFP::INT_BITS != INT_BITS || OtherFP::FRAC_BITS != FRAC_BITS),
                        int>::type = 0)
        : mValue(static_cast<u16>(
            static_cast<u32>(other.raw()) << (FRAC_BITS - OtherFP::FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE u4x12 from_raw(u16 raw) {
        u4x12 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    u16 raw() const { return mValue; }
    u16 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE u4x12 operator*(u4x12 b) const {
        return from_raw(static_cast<u16>(
            (static_cast<u32>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE u4x12 operator/(u4x12 b) const {
        return from_raw(static_cast<u16>(
            (static_cast<u32>(mValue) * (static_cast<u32>(1) << FRAC_BITS)) / b.mValue));
    }

    FASTLED_FORCE_INLINE u4x12 operator+(u4x12 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE u4x12 operator-(u4x12 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE u4x12 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    FASTLED_FORCE_INLINE u4x12 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE u4x12 operator*(u16 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE u4x12 operator*(u16 scalar, u4x12 fp) {
        return u4x12::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(u4x12 b) const { return mValue < b.mValue; }
    bool operator>(u4x12 b) const { return mValue > b.mValue; }
    bool operator<=(u4x12 b) const { return mValue <= b.mValue; }
    bool operator>=(u4x12 b) const { return mValue >= b.mValue; }
    bool operator==(u4x12 b) const { return mValue == b.mValue; }
    bool operator!=(u4x12 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE u4x12 mod(u4x12 a, u4x12 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE u4x12 floor(u4x12 x) {
        constexpr u16 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE u4x12 ceil(u4x12 x) {
        constexpr u16 frac_mask = (1 << FRAC_BITS) - 1;
        u16 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE u4x12 fract(u4x12 x) {
        constexpr u16 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE u4x12 abs(u4x12 x) {
        // For unsigned type, abs is identity
        return x;
    }

    static FASTLED_FORCE_INLINE u4x12 min(u4x12 a, u4x12 b) {
        return a < b ? a : b;
    }

    static FASTLED_FORCE_INLINE u4x12 max(u4x12 a, u4x12 b) {
        return a > b ? a : b;
    }

    static FASTLED_FORCE_INLINE u4x12 lerp(u4x12 a, u4x12 b, u4x12 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE u4x12 clamp(u4x12 x, u4x12 lo, u4x12 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE u4x12 step(u4x12 edge, u4x12 x) {
        constexpr u4x12 one(1.0f);
        return x < edge ? u4x12() : one;
    }

    static FASTLED_FORCE_INLINE u4x12 smoothstep(u4x12 edge0, u4x12 edge1, u4x12 x) {
        constexpr u4x12 zero(0.0f);
        constexpr u4x12 one(1.0f);
        constexpr u4x12 two(2.0f);
        constexpr u4x12 three(3.0f);
        u4x12 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    static FASTLED_FORCE_INLINE u4x12 sqrt(u4x12 x) {
        if (x.mValue == 0) return u4x12();
        return from_raw(static_cast<u16>(
            fl::isqrt32(static_cast<u32>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE u4x12 rsqrt(u4x12 x) {
        u4x12 s = sqrt(x);
        if (s.mValue == 0) return u4x12();
        return from_raw(static_cast<u16>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE u4x12 pow(u4x12 base, u4x12 exp) {
        if (base.mValue == 0) return u4x12();
        constexpr u4x12 one(1.0f);
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
    // Horner evaluation uses i32 intermediates (20 frac bits) to minimize
    // rounding error, then converts back to 12 frac bits.
    static FASTLED_FORCE_INLINE u4x12 log2_fp(u4x12 x) {
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
        u16 frac12 = static_cast<u16>(frac_part >> (IFRAC - FRAC_BITS));
        return from_raw(static_cast<u16>((int_part << FRAC_BITS) + frac12));
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses i32 intermediates (20 frac bits) to minimize
    // rounding error, then converts back to 12 frac bits.
    static FASTLED_FORCE_INLINE u4x12 exp2_fp(u4x12 x) {
        u4x12 fl_val = floor(x);
        u4x12 fr = x - fl_val;
        i32 n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS) return from_raw(0xFFFF);
        if (n < 0) return u4x12();
        u32 int_pow = static_cast<u32>(1u << FRAC_BITS) << n;
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
        u32 frac_pow12 = static_cast<u32>(frac_pow20) >> (IFRAC - FRAC_BITS);
        u32 result =
            (int_pow * frac_pow12) >> FRAC_BITS;
        return from_raw(static_cast<u16>(result));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
