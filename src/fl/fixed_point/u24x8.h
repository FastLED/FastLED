#pragma once

// Unsigned 24.8 fixed-point arithmetic.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"
#include "fl/stl/type_traits.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Unsigned 24.8 fixed-point value type.
class u24x8 {
  public:
    static constexpr int INT_BITS = 24;
    static constexpr int FRAC_BITS = 8;

    // ---- Construction ------------------------------------------------------

    constexpr u24x8() = default;

    explicit constexpr u24x8(float f)
        : mValue(static_cast<u32>(f * (1 << FRAC_BITS))) {}

    // Auto-promotion from other fixed-point types
    template <typename OtherFP>
    constexpr u24x8(const OtherFP& other,
                    typename fl::enable_if<
                        (OtherFP::INT_BITS <= INT_BITS) &&
                        (OtherFP::FRAC_BITS <= FRAC_BITS) &&
                        (OtherFP::INT_BITS != INT_BITS || OtherFP::FRAC_BITS != FRAC_BITS),
                        int>::type = 0)
        : mValue(static_cast<u32>(
            static_cast<u64>(other.raw()) << (FRAC_BITS - OtherFP::FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE u24x8 from_raw(u32 raw) {
        u24x8 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    u32 raw() const { return mValue; }
    u32 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE u24x8 operator*(u24x8 b) const {
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE u24x8 operator/(u24x8 b) const {
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * (static_cast<u64>(1) << FRAC_BITS)) / b.mValue));
    }

    FASTLED_FORCE_INLINE u24x8 operator+(u24x8 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE u24x8 operator-(u24x8 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE u24x8 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    FASTLED_FORCE_INLINE u24x8 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE u24x8 operator*(u32 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE u24x8 operator*(u32 scalar, u24x8 fp) {
        return u24x8::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(u24x8 b) const { return mValue < b.mValue; }
    bool operator>(u24x8 b) const { return mValue > b.mValue; }
    bool operator<=(u24x8 b) const { return mValue <= b.mValue; }
    bool operator>=(u24x8 b) const { return mValue >= b.mValue; }
    bool operator==(u24x8 b) const { return mValue == b.mValue; }
    bool operator!=(u24x8 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE u24x8 mod(u24x8 a, u24x8 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE u24x8 floor(u24x8 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE u24x8 ceil(u24x8 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        u32 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE u24x8 fract(u24x8 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE u24x8 abs(u24x8 x) {
        // For unsigned, abs is identity
        return x;
    }

    static FASTLED_FORCE_INLINE u24x8 min(u24x8 a, u24x8 b) {
        return a.mValue < b.mValue ? a : b;
    }

    static FASTLED_FORCE_INLINE u24x8 max(u24x8 a, u24x8 b) {
        return a.mValue > b.mValue ? a : b;
    }

    static FASTLED_FORCE_INLINE u24x8 lerp(u24x8 a, u24x8 b, u24x8 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE u24x8 clamp(u24x8 x, u24x8 lo, u24x8 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE u24x8 step(u24x8 edge, u24x8 x) {
        constexpr u24x8 one(1.0f);
        return x < edge ? u24x8() : one;
    }

    static FASTLED_FORCE_INLINE u24x8 smoothstep(u24x8 edge0, u24x8 edge1, u24x8 x) {
        constexpr u24x8 zero(0.0f);
        constexpr u24x8 one(1.0f);
        constexpr u24x8 two(2.0f);
        constexpr u24x8 three(3.0f);
        u24x8 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    static FASTLED_FORCE_INLINE u24x8 sqrt(u24x8 x) {
        if (x.mValue == 0) return u24x8();
        return from_raw(static_cast<u32>(
            fl::isqrt64(static_cast<u64>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE u24x8 rsqrt(u24x8 x) {
        u24x8 s = sqrt(x);
        if (s.mValue == 0) return u24x8();
        return from_raw(static_cast<u32>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE u24x8 pow(u24x8 base, u24x8 exp) {
        if (base.mValue == 0) return u24x8();
        constexpr u24x8 one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
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
    // Uses 4-term minimax polynomial for log2(1+t), t in [0,1).
    // Horner evaluation uses u64 intermediates (16 frac bits) to minimize
    // rounding error, then converts back to 8 frac bits.
    static FASTLED_FORCE_INLINE u24x8 log2_fp(u24x8 x) {
        u32 val = x.mValue;
        int msb = highest_bit(val);
        if (msb < 0) return u24x8();  // log2(0) = undefined, return 0

        u32 int_part = msb - FRAC_BITS;
        u32 t;
        if (msb >= FRAC_BITS) {
            t = (val >> (msb - FRAC_BITS)) - (1u << FRAC_BITS);
        } else {
            t = (val << (FRAC_BITS - msb)) - (1u << FRAC_BITS);
        }

        // Simplified 2-term minimax coefficients for log2(1+t), t in [0,1).
        // Stored as u64 with 16 fractional bits.
        constexpr int IFRAC = 16;
        constexpr u64 c0 = 94528ULL;   // 1.44179 * 2^16
        constexpr u64 c1 = 19727ULL;   // 0.30093 * 2^16 (unsigned approximation)

        // Extend t from 8 to 16 frac bits.
        u64 t16 = static_cast<u64>(t) << (IFRAC - FRAC_BITS);

        // Horner for unsigned: simplified polynomial
        u64 acc = c0 + ((c1 * t16) >> IFRAC);
        u64 frac_part = (acc * t16) >> IFRAC;

        // Convert from 16 frac bits back to 8.
        u32 frac8 = static_cast<u32>(frac_part >> (IFRAC - FRAC_BITS));
        return from_raw((int_part << FRAC_BITS) + frac8);
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses u64 intermediates (16 frac bits) to minimize
    // rounding error, then converts back to 8 frac bits.
    static FASTLED_FORCE_INLINE u24x8 exp2_fp(u24x8 x) {
        u24x8 fl_val = floor(x);
        u24x8 fr = x - fl_val;
        u32 n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS) return from_raw(0xFFFFFFFF);
        u32 int_pow;
        int_pow = static_cast<u32>(1u << FRAC_BITS) << n;
        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Stored as u64 with 16 fractional bits.
        constexpr int IFRAC = 16;
        constexpr u64 d0 = 45427ULL;    // 0.69316 * 2^16
        constexpr u64 d1 = 15775ULL;    // 0.24071 * 2^16
        constexpr u64 d2 = 3497ULL;     // 0.05336 * 2^16
        constexpr u64 d3 = 836ULL;      // 0.01276 * 2^16
        // Extend fr from 8 to 16 frac bits.
        u64 fr16 = static_cast<u64>(fr.mValue) << (IFRAC - FRAC_BITS);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        u64 acc = d3;
        acc = d2 + ((acc * fr16) >> IFRAC);
        acc = d1 + ((acc * fr16) >> IFRAC);
        acc = d0 + ((acc * fr16) >> IFRAC);
        constexpr u64 one16 = 1ULL << IFRAC;
        u64 frac_pow16 = one16 + ((acc * fr16) >> IFRAC);
        // Convert from 16 frac bits to 8 frac bits, then scale by int_pow.
        u32 frac_pow8 = static_cast<u32>(frac_pow16 >> (IFRAC - FRAC_BITS));
        u64 result =
            (static_cast<u64>(int_pow) * frac_pow8) >> FRAC_BITS;
        return from_raw(static_cast<u32>(result));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
