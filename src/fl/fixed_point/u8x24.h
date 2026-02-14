#pragma once

// Unsigned 8.24 fixed-point arithmetic.
// All operations are integer-only in the hot path.
// Range: [0, 256) with 24 fractional bits.

#include "fl/int.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/compiler_control.h"
#include "fl/stl/type_traits.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Unsigned 8.24 fixed-point value type.
class u8x24 {
  public:
    static constexpr int INT_BITS = 8;
    static constexpr int FRAC_BITS = 24;

    // ---- Construction ------------------------------------------------------

    constexpr u8x24() = default;

    explicit constexpr u8x24(float f)
        : mValue(static_cast<u32>(f * (1 << FRAC_BITS))) {}

    // Auto-promotion from other fixed-point types
    template <typename OtherFP>
    constexpr u8x24(const OtherFP& other,
                    typename fl::enable_if<
                        (OtherFP::INT_BITS <= INT_BITS) &&
                        (OtherFP::FRAC_BITS <= FRAC_BITS) &&
                        (OtherFP::INT_BITS != INT_BITS || OtherFP::FRAC_BITS != FRAC_BITS),
                        int>::type = 0)
        : mValue(static_cast<u32>(
            static_cast<u64>(other.raw()) << (FRAC_BITS - OtherFP::FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE u8x24 from_raw(u32 raw) {
        u8x24 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    u32 raw() const { return mValue; }
    u32 to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (1 << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE u8x24 operator*(u8x24 b) const {
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE u8x24 operator/(u8x24 b) const {
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * (static_cast<u64>(1) << FRAC_BITS)) / b.mValue));
    }

    FASTLED_FORCE_INLINE u8x24 operator+(u8x24 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE u8x24 operator-(u8x24 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE u8x24 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    FASTLED_FORCE_INLINE u8x24 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE u8x24 operator*(u32 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE u8x24 operator*(u32 scalar, u8x24 fp) {
        return u8x24::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(u8x24 b) const { return mValue < b.mValue; }
    bool operator>(u8x24 b) const { return mValue > b.mValue; }
    bool operator<=(u8x24 b) const { return mValue <= b.mValue; }
    bool operator>=(u8x24 b) const { return mValue >= b.mValue; }
    bool operator==(u8x24 b) const { return mValue == b.mValue; }
    bool operator!=(u8x24 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE u8x24 mod(u8x24 a, u8x24 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE u8x24 floor(u8x24 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE u8x24 ceil(u8x24 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        u32 floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return from_raw(floored);
    }

    static FASTLED_FORCE_INLINE u8x24 fract(u8x24 x) {
        constexpr u32 frac_mask = (1 << FRAC_BITS) - 1;
        return from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE u8x24 min(u8x24 a, u8x24 b) {
        return a.mValue < b.mValue ? a : b;
    }

    static FASTLED_FORCE_INLINE u8x24 max(u8x24 a, u8x24 b) {
        return a.mValue > b.mValue ? a : b;
    }

    static FASTLED_FORCE_INLINE u8x24 lerp(u8x24 a, u8x24 b, u8x24 t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE u8x24 clamp(u8x24 x, u8x24 lo, u8x24 hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE u8x24 step(u8x24 edge, u8x24 x) {
        constexpr u8x24 one(1.0f);
        return x < edge ? u8x24() : one;
    }

    static FASTLED_FORCE_INLINE u8x24 smoothstep(u8x24 edge0, u8x24 edge1, u8x24 x) {
        constexpr u8x24 zero(0.0f);
        constexpr u8x24 one(1.0f);
        constexpr u8x24 two(2.0f);
        constexpr u8x24 three(3.0f);
        u8x24 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    static FASTLED_FORCE_INLINE u8x24 sqrt(u8x24 x) {
        if (x.mValue == 0) return u8x24();
        return from_raw(static_cast<u32>(
            fl::isqrt64(static_cast<u64>(x.mValue) << FRAC_BITS)));
    }

    static FASTLED_FORCE_INLINE u8x24 rsqrt(u8x24 x) {
        u8x24 s = sqrt(x);
        if (s.mValue == 0) return u8x24();
        return from_raw(static_cast<u32>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE u8x24 pow(u8x24 base, u8x24 exp) {
        if (base.mValue == 0) return u8x24();
        constexpr u8x24 one(1.0f);
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
    static FASTLED_FORCE_INLINE u8x24 log2_fp(u8x24 x) {
        u32 val = x.mValue;
        int msb = highest_bit(val);
        if (msb < 0) return u8x24(); // log2(0) = -inf, return 0

        u32 int_part = msb - FRAC_BITS;
        u32 t;
        if (msb >= FRAC_BITS) {
            t = (val >> (msb - FRAC_BITS)) - (1u << FRAC_BITS);
        } else {
            t = (val << (FRAC_BITS - msb)) - (1u << FRAC_BITS);
        }

        // 4-term minimax coefficients for log2(1+t), t in [0,1).
        constexpr int IFRAC = 24;
        constexpr u64 c0 = 24189248ULL;  // 1.44179 * 2^24
        constexpr i64 c1 = -11728384LL;  // -0.69907 * 2^24
        constexpr i64 c2 = 6098176LL;    // 0.36348 * 2^24
        constexpr i64 c3 = -1788416LL;   // -0.10660 * 2^24

        i64 t24 = static_cast<i64>(t);
        // Horner: t * (c0 + t * (c1 + t * (c2 + t * c3)))
        i64 acc = c3;
        acc = c2 + ((acc * t24) >> IFRAC);
        acc = c1 + ((acc * t24) >> IFRAC);
        acc = static_cast<i64>(c0) + ((acc * t24) >> IFRAC);
        i64 frac_part = (acc * t24) >> IFRAC;

        // Combine integer and fractional parts
        i64 result = (static_cast<i64>(int_part) << FRAC_BITS) + frac_part;
        return from_raw(result >= 0 ? static_cast<u32>(result) : 0);
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    static FASTLED_FORCE_INLINE u8x24 exp2_fp(u8x24 x) {
        u8x24 fl_val = floor(x);
        u8x24 fr = x - fl_val;
        u32 n = fl_val.mValue >> FRAC_BITS;

        // Prevent overflow
        if (n >= INT_BITS) return from_raw(0xFFFFFFFFu);

        u32 int_pow = static_cast<u32>(1u << FRAC_BITS) << n;

        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        constexpr int IFRAC = 24;
        constexpr u64 d0 = 11629376ULL;  // 0.69316 * 2^24
        constexpr u64 d1 = 4038400ULL;   // 0.24071 * 2^24
        constexpr u64 d2 = 895232ULL;    // 0.05336 * 2^24
        constexpr u64 d3 = 214016ULL;    // 0.01276 * 2^24

        u64 fr24 = static_cast<u64>(fr.mValue);
        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        u64 acc = d3;
        acc = d2 + ((acc * fr24) >> IFRAC);
        acc = d1 + ((acc * fr24) >> IFRAC);
        acc = d0 + ((acc * fr24) >> IFRAC);
        constexpr u64 one24 = 1ULL << IFRAC;
        u64 frac_pow24 = one24 + ((acc * fr24) >> IFRAC);

        // Scale by int_pow (result stays at 24 frac bits).
        u64 result = (static_cast<u64>(int_pow) * frac_pow24) >> FRAC_BITS;
        return from_raw(static_cast<u32>(result));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
