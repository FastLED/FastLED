#pragma once

// Unsigned 8.8 fixed-point arithmetic.
// All operations are integer-only in the hot path.

#include "fl/int.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"
#include "fl/stl/type_traits.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Unsigned 8.8 fixed-point value type.
// Range: [0, 256) with 8 fractional bits.
class u8x8 {
  public:
    static constexpr int INT_BITS = 8;
    static constexpr int FRAC_BITS = 8;
    static constexpr i32 SCALE = static_cast<i32>(1) << FRAC_BITS;

    // ---- Construction ------------------------------------------------------

    constexpr u8x8() = default;

    explicit constexpr u8x8(float f)
        : mValue(static_cast<u16>(f * (SCALE))) {}

    // Auto-promotion from other fixed-point types
    template <typename OtherFP>
    constexpr u8x8(const OtherFP& other,
                   typename fl::enable_if<
                       (OtherFP::INT_BITS <= INT_BITS) &&
                       (OtherFP::FRAC_BITS <= FRAC_BITS) &&
                       (OtherFP::INT_BITS != INT_BITS || OtherFP::FRAC_BITS != FRAC_BITS),
                       int>::type = 0)
        : mValue(static_cast<u16>(
            static_cast<u32>(other.raw()) << (FRAC_BITS - OtherFP::FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE u8x8 from_raw(u16 raw) {
        u8x8 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    u16 raw() const { return mValue; }
    u16 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (SCALE); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE u8x8 operator*(u8x8 b) const {
        return from_raw(static_cast<u16>(
            (static_cast<u32>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE u8x8 operator/(u8x8 b) const {
        if (b.mValue == 0) return u8x8();
        return from_raw(static_cast<u16>(
            (static_cast<u32>(mValue) * (static_cast<u32>(1) << FRAC_BITS)) / b.mValue));
    }

    FASTLED_FORCE_INLINE u8x8 operator+(u8x8 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE u8x8 operator-(u8x8 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE u8x8 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    FASTLED_FORCE_INLINE u8x8 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE u8x8 operator*(u16 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE u8x8 operator*(u16 scalar, u8x8 fp) {
        return u8x8::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(u8x8 b) const { return mValue < b.mValue; }
    bool operator>(u8x8 b) const { return mValue > b.mValue; }
    bool operator<=(u8x8 b) const { return mValue <= b.mValue; }
    bool operator>=(u8x8 b) const { return mValue >= b.mValue; }
    bool operator==(u8x8 b) const { return mValue == b.mValue; }
    bool operator!=(u8x8 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE u8x8 mod(u8x8 a, u8x8 b) {
        if (b.mValue == 0) return u8x8();
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE u8x8 floor(u8x8 x) {
        constexpr u16 frac_mask = (SCALE) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE u8x8 ceil(u8x8 x) {
        constexpr u16 frac_mask = (SCALE) - 1;
        u16 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (SCALE);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE u8x8 fract(u8x8 x) {
        constexpr u16 frac_mask = (SCALE) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE u8x8 abs(u8x8 x) {
        // Unsigned values are always non-negative
        return x;
    }

    static FASTLED_FORCE_INLINE u8x8 min(u8x8 a, u8x8 b) {
        return a < b ? a : b;
    }

    static FASTLED_FORCE_INLINE u8x8 max(u8x8 a, u8x8 b) {
        return a > b ? a : b;
    }

    static FASTLED_FORCE_INLINE u8x8 lerp(u8x8 a, u8x8 b, u8x8 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE u8x8 clamp(u8x8 x, u8x8 lo, u8x8 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE u8x8 step(u8x8 edge, u8x8 x) {
        constexpr u8x8 one(1.0f);
        return x < edge ? u8x8() : one;
    }

    static FASTLED_FORCE_INLINE u8x8 smoothstep(u8x8 edge0, u8x8 edge1, u8x8 x) {
        constexpr u8x8 zero(0.0f);
        constexpr u8x8 one(1.0f);
        constexpr u8x8 two(2.0f);
        constexpr u8x8 three(3.0f);
        u8x8 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    static FASTLED_FORCE_INLINE u8x8 sqrt(u8x8 x) {
        if (x.mValue == 0) return u8x8();
        return from_raw(static_cast<u16>(
            fl::isqrt32(static_cast<u32>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE u8x8 rsqrt(u8x8 x) {
        u8x8 s = sqrt(x);
        if (s.mValue == 0) return u8x8();
        return from_raw(static_cast<u16>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE u8x8 pow(u8x8 base, u8x8 exp) {
        if (base.mValue == 0) return u8x8();
        constexpr u8x8 one(1.0f);
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
    // Horner evaluation uses i32 intermediates (16 frac bits) to minimize
    // rounding error, then converts back to 8 frac bits.
    static FASTLED_FORCE_INLINE u8x8 log2_fp(u8x8 x) {
        u32 val = static_cast<u32>(x.mValue);
        int msb = highest_bit(val);
        if (msb < 0) return u8x8();
        i32 int_part = msb - FRAC_BITS;
        i32 t;
        if (msb >= FRAC_BITS) {
            t = static_cast<i32>(
                (val >> (msb - FRAC_BITS)) - (SCALE));
        } else {
            t = static_cast<i32>(
                (val << (FRAC_BITS - msb)) - (SCALE));
        }
        // 4-term minimax coefficients for log2(1+t), t in [0,1).
        // Stored as i32 with 16 fractional bits. Max product ~2^29, fits i32 after shift.
        constexpr int IFRAC = 16;
        constexpr i32 c0 = 94528;    // 1.44179 * 2^16
        constexpr i32 c1 = -45814;   // -0.69907 * 2^16
        constexpr i32 c2 = 23821;    // 0.36348 * 2^16
        constexpr i32 c3 = -6986;    // -0.10660 * 2^16
        // Extend t from 8 to 16 frac bits.
        i32 t16 = static_cast<i32>(t) << (IFRAC - FRAC_BITS);
        // Horner: t * (c0 + t * (c1 + t * (c2 + t * c3)))
        i32 acc = c3;
        acc = c2 + static_cast<i32>((static_cast<i64>(acc) * t16) >> IFRAC);
        acc = c1 + static_cast<i32>((static_cast<i64>(acc) * t16) >> IFRAC);
        acc = c0 + static_cast<i32>((static_cast<i64>(acc) * t16) >> IFRAC);
        i32 frac_part = static_cast<i32>((static_cast<i64>(acc) * t16) >> IFRAC);
        // Convert from 16 frac bits back to 8.
        i16 frac8 = static_cast<i16>(frac_part >> (IFRAC - FRAC_BITS));
        // Combine integer and fractional parts (handle negative integer part)
        i32 result_raw = (int_part << FRAC_BITS) + frac8;
        // Clamp to valid unsigned range
        if (result_raw < 0) return u8x8();
        return from_raw(static_cast<u16>(result_raw));
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses i32 intermediates (16 frac bits) to minimize
    // rounding error, then converts back to 8 frac bits.
    static FASTLED_FORCE_INLINE u8x8 exp2_fp(u8x8 x) {
        u8x8 fl_val = floor(x);
        u8x8 fr = x - fl_val;
        i32 n = fl_val.mValue >> FRAC_BITS;
        // Overflow check for unsigned type
        if (n >= INT_BITS) return from_raw(0xFFFF);
        if (n < 0) return u8x8();
        i32 int_pow = static_cast<i32>(SCALE) << n;
        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as i32 with 16 fractional bits.
        constexpr int IFRAC = 16;
        constexpr i32 d0 = 45427;    // 0.69316 * 2^16
        constexpr i32 d1 = 15775;    // 0.24071 * 2^16
        constexpr i32 d2 = 3497;     // 0.05336 * 2^16
        constexpr i32 d3 = 836;      // 0.01276 * 2^16
        // Extend fr from 8 to 16 frac bits.
        i32 fr16 = static_cast<i32>(fr.mValue) << (IFRAC - FRAC_BITS);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        i32 acc = d3;
        acc = d2 + static_cast<i32>((static_cast<i64>(acc) * fr16) >> IFRAC);
        acc = d1 + static_cast<i32>((static_cast<i64>(acc) * fr16) >> IFRAC);
        acc = d0 + static_cast<i32>((static_cast<i64>(acc) * fr16) >> IFRAC);
        constexpr i32 one16 = 1 << IFRAC;
        i32 frac_pow16 = one16 + static_cast<i32>((static_cast<i64>(acc) * fr16) >> IFRAC);
        // Convert from 16 frac bits to 8 frac bits, then scale by int_pow.
        i32 frac_pow8 = frac_pow16 >> (IFRAC - FRAC_BITS);
        i32 result =
            (int_pow * frac_pow8) >> FRAC_BITS;
        // Clamp to u16 range
        if (result > 0xFFFF) return from_raw(0xFFFF);
        return from_raw(static_cast<u16>(result));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
