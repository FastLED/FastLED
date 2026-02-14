#pragma once

// Unsigned 16.16 fixed-point arithmetic.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"
#include "fl/stl/type_traits.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Unsigned 16.16 fixed-point value type.
class u16x16 {
  public:
    static constexpr int INT_BITS = 16;
    static constexpr int FRAC_BITS = 16;

    // ---- Construction ------------------------------------------------------

    constexpr u16x16() = default;

    explicit constexpr u16x16(float f)
        : mValue(static_cast<u32>(f * (1 << FRAC_BITS))) {}

    // Auto-promotion from other fixed-point types
    // Enabled only when both INT_BITS and FRAC_BITS can be promoted (no demotion)
    template <typename OtherFP>
    constexpr u16x16(const OtherFP& other,
                     typename fl::enable_if<
                         (OtherFP::INT_BITS <= INT_BITS) &&
                         (OtherFP::FRAC_BITS <= FRAC_BITS) &&
                         (OtherFP::INT_BITS != INT_BITS || OtherFP::FRAC_BITS != FRAC_BITS),
                         int>::type = 0)
        : mValue(static_cast<u32>(
            static_cast<u64>(other.raw()) << (FRAC_BITS - OtherFP::FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE u16x16 from_raw(u32 raw) {
        u16x16 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    u32 raw() const { return mValue; }
    u32 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE u16x16 operator*(u16x16 b) const {
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE u16x16 operator/(u16x16 b) const {
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * (static_cast<u64>(1) << FRAC_BITS)) / b.mValue));
    }

    FASTLED_FORCE_INLINE u16x16 operator+(u16x16 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE u16x16 operator-(u16x16 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE u16x16 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    FASTLED_FORCE_INLINE u16x16 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE u16x16 operator*(u32 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE u16x16 operator*(u32 scalar, u16x16 fp) {
        return u16x16::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(u16x16 b) const { return mValue < b.mValue; }
    bool operator>(u16x16 b) const { return mValue > b.mValue; }
    bool operator<=(u16x16 b) const { return mValue <= b.mValue; }
    bool operator>=(u16x16 b) const { return mValue >= b.mValue; }
    bool operator==(u16x16 b) const { return mValue == b.mValue; }
    bool operator!=(u16x16 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE u16x16 mod(u16x16 a, u16x16 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE u16x16 floor(u16x16 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE u16x16 ceil(u16x16 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        u32 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE u16x16 fract(u16x16 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE u16x16 min(u16x16 a, u16x16 b) {
        return a < b ? a : b;
    }

    static FASTLED_FORCE_INLINE u16x16 max(u16x16 a, u16x16 b) {
        return a > b ? a : b;
    }

    static FASTLED_FORCE_INLINE u16x16 clamp(u16x16 x, u16x16 lo, u16x16 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE u16x16 lerp(u16x16 a, u16x16 b, u16x16 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE u16x16 step(u16x16 edge, u16x16 x) {
        constexpr u16x16 one(1.0f);
        return x < edge ? u16x16() : one;
    }

    static FASTLED_FORCE_INLINE u16x16 smoothstep(u16x16 edge0, u16x16 edge1, u16x16 x) {
        constexpr u16x16 zero(0.0f);
        constexpr u16x16 one(1.0f);
        constexpr u16x16 two(2.0f);
        constexpr u16x16 three(3.0f);
        u16x16 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    static FASTLED_FORCE_INLINE u16x16 sqrt(u16x16 x) {
        if (x.mValue == 0) return u16x16();
        return from_raw(static_cast<u32>(
            fl::isqrt64(static_cast<u64>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE u16x16 rsqrt(u16x16 x) {
        u16x16 s = sqrt(x);
        if (s.mValue == 0) return u16x16();
        return from_raw(static_cast<u32>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE u16x16 pow(u16x16 base, u16x16 exp) {
        constexpr u16x16 one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
        if (base.mValue == 0) return u16x16();
        return exp2_fp(exp * log2_fp(base));
    }

  private:
    u32 mValue = 0;

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
    // Uses 2-term polynomial for log2(1+t), t in [0,1) (simplified for unsigned).
    // Horner evaluation uses u64 intermediates (24 frac bits) to minimize
    // rounding error, then converts back to 16 frac bits.
    static FASTLED_FORCE_INLINE u16x16 log2_fp(u16x16 x) {
        u32 val = x.mValue;
        int msb = highest_bit(val);
        if (msb < 0) return u16x16();  // log2(0) = undefined, return 0

        u32 int_part = msb - FRAC_BITS;
        u32 t;
        if (msb >= FRAC_BITS) {
            t = (val >> (msb - FRAC_BITS)) - (1u << FRAC_BITS);
        } else {
            t = (val << (FRAC_BITS - msb)) - (1u << FRAC_BITS);
        }

        // 2-term polynomial coefficients for log2(1+t), t in [0,1).
        // Stored as u64 with 24 fractional bits.
        constexpr int IFRAC = 24;
        constexpr u64 c0 = 24189248ULL;  // 1.44179 * 2^24
        constexpr u64 c1 = 5049984ULL;   // 0.30093 * 2^24

        // Extend t from 16 to 24 frac bits.
        u64 t24 = static_cast<u64>(t) << (IFRAC - FRAC_BITS);

        // Simplified Horner: c0 + c1*t, then multiply by t
        u64 acc = c0 + ((c1 * t24) >> IFRAC);
        u64 frac_part = (acc * t24) >> IFRAC;

        // Convert from 24 frac bits back to 16.
        u32 frac16 = static_cast<u32>(frac_part >> (IFRAC - FRAC_BITS));
        return from_raw((int_part << FRAC_BITS) + frac16);
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses u64 intermediates (24 frac bits) to minimize
    // rounding error, then converts back to 16 frac bits.
    static FASTLED_FORCE_INLINE u16x16 exp2_fp(u16x16 x) {
        u16x16 fl_val = floor(x);
        u16x16 fr = x - fl_val;
        u32 n = fl_val.mValue >> FRAC_BITS;

        if (n >= INT_BITS) return from_raw(0xFFFFFFFF);  // overflow

        u32 int_pow = (1u << FRAC_BITS) << n;

        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as u64 with 24 fractional bits.
        constexpr int IFRAC = 24;
        constexpr u64 d0 = 11629376ULL;  // 0.69316 * 2^24
        constexpr u64 d1 = 4038400ULL;   // 0.24071 * 2^24
        constexpr u64 d2 = 895232ULL;    // 0.05336 * 2^24
        constexpr u64 d3 = 214016ULL;    // 0.01276 * 2^24

        // Extend fr from 16 to 24 frac bits.
        u64 fr24 = static_cast<u64>(fr.mValue) << (IFRAC - FRAC_BITS);

        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        u64 acc = d3;
        acc = d2 + ((acc * fr24) >> IFRAC);
        acc = d1 + ((acc * fr24) >> IFRAC);
        acc = d0 + ((acc * fr24) >> IFRAC);
        constexpr u64 one24 = 1ULL << IFRAC;
        u64 frac_pow24 = one24 + ((acc * fr24) >> IFRAC);

        // Convert from 24 frac bits to 16 frac bits, then scale by int_pow.
        u32 frac_pow16 = static_cast<u32>(frac_pow24 >> (IFRAC - FRAC_BITS));
        u64 result = (static_cast<u64>(int_pow) * frac_pow16) >> FRAC_BITS;
        return from_raw(static_cast<u32>(result));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
