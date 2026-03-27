#pragma once

// Unsigned 16.16 fixed-point arithmetic.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/math/fixed_point/isqrt.h"
#include "fl/stl/compiler_control.h"
#include "fl/math/fixed_point/traits.h"
#include "fl/stl/noexcept.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Unsigned 16.16 fixed-point value type.
class u16x16 {
  public:
    static constexpr int INT_BITS = 16;
    static constexpr int FRAC_BITS = 16;
    static constexpr i32 SCALE = static_cast<i32>(1) << FRAC_BITS;

    // ---- Construction ------------------------------------------------------

    constexpr u16x16() FL_NOEXCEPT = default;

    explicit constexpr u16x16(float f)
        : mValue(static_cast<u32>(f * (SCALE))) {}

    // Integer constructor — any integer width (portable: AVR 16-bit int, ARM/x86 32-bit).
    // Compile error if constexpr value exceeds INT_BITS range.
    template <typename IntT, detail::enable_if_integer_t<IntT> = 0>
    explicit constexpr u16x16(IntT n)
        : mValue(detail::int_to_fixed<INT_BITS, FRAC_BITS>::from_unsigned(n)) {}

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

    // Raw constructor for C++11 constexpr from_raw
    struct RawTag {};
    constexpr explicit u16x16(u32 raw, RawTag) : mValue(raw) {}

    static constexpr FASTLED_FORCE_INLINE u16x16 from_raw(u32 raw) {
        return u16x16(raw, RawTag());
    }

    // ---- Access ------------------------------------------------------------

    constexpr u32 raw() const { return mValue; }
    constexpr u32 to_int() const { return mValue >> FRAC_BITS; }
    constexpr float to_float() const { return static_cast<float>(mValue) / (SCALE); }

    // ---- Fixed-point arithmetic --------------------------------------------

#if defined(__riscv) && (__riscv_xlen == 32)
    // RV32: Use mul+mulhu inline asm to avoid expensive 64-bit widening multiply.
    // Extracts bits [47:16] of the 64-bit product for Q16.16 unsigned fixed-point.
    FASTLED_FORCE_INLINE u16x16 operator*(u16x16 b) const {
        u32 lo, hi;
        asm("mul   %0, %2, %3\n\t"
            "mulhu %1, %2, %3"
            : "=&r"(lo), "=&r"(hi)
            : "r"(mValue), "r"(b.mValue));
        return from_raw((lo >> 16) | (hi << 16));
    }
#else
    constexpr FASTLED_FORCE_INLINE u16x16 operator*(u16x16 b) const {
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * b.mValue) >> FRAC_BITS));
    }
#endif

    constexpr FASTLED_FORCE_INLINE u16x16 operator/(u16x16 b) const {
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * (static_cast<u64>(1) << FRAC_BITS)) / b.mValue));
    }

    constexpr FASTLED_FORCE_INLINE u16x16 operator+(u16x16 b) const {
        return from_raw(mValue + b.mValue);
    }

    constexpr FASTLED_FORCE_INLINE u16x16 operator-(u16x16 b) const {
        return from_raw(mValue - b.mValue);
    }

    constexpr FASTLED_FORCE_INLINE u16x16 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    constexpr FASTLED_FORCE_INLINE u16x16 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    constexpr FASTLED_FORCE_INLINE u16x16 operator*(u32 scalar) const {
        return from_raw(mValue * scalar);
    }

    friend constexpr u16x16 operator*(u32 scalar, u16x16 fp) {
        return u16x16::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    constexpr bool operator<(u16x16 b) const { return mValue < b.mValue; }
    constexpr bool operator>(u16x16 b) const { return mValue > b.mValue; }
    constexpr bool operator<=(u16x16 b) const { return mValue <= b.mValue; }
    constexpr bool operator>=(u16x16 b) const { return mValue >= b.mValue; }
    constexpr bool operator==(u16x16 b) const { return mValue == b.mValue; }
    constexpr bool operator!=(u16x16 b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static constexpr FASTLED_FORCE_INLINE u16x16 mod(u16x16 a, u16x16 b) {
        return from_raw(a.mValue % b.mValue);
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 floor(u16x16 x) {
        return from_raw(x.mValue & ~(u32((SCALE) - 1)));
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 ceil(u16x16 x) {
        return from_raw((x.mValue & ~(u32((SCALE) - 1))) +
                        ((x.mValue & u32((SCALE) - 1)) ? (SCALE) : 0));
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 fract(u16x16 x) {
        return from_raw(x.mValue & u32((SCALE) - 1));
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 min(u16x16 a, u16x16 b) {
        return a < b ? a : b;
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 max(u16x16 a, u16x16 b) {
        return a > b ? a : b;
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 clamp(u16x16 x, u16x16 lo, u16x16 hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

#if defined(__riscv) && (__riscv_xlen == 32)
    static FASTLED_FORCE_INLINE u16x16 lerp(u16x16 a, u16x16 b, u16x16 t) {
#else
    static constexpr FASTLED_FORCE_INLINE u16x16 lerp(u16x16 a, u16x16 b, u16x16 t) {
#endif
        return a + (b - a) * t;
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 step(u16x16 edge, u16x16 x) {
        return x < edge ? u16x16() : u16x16(1.0f);
    }

    static FASTLED_FORCE_INLINE u16x16 smoothstep(u16x16 edge0, u16x16 edge1, u16x16 x) {
        constexpr u16x16 zero(0.0f);
        constexpr u16x16 one(1.0f);
        constexpr u16x16 two(2.0f);
        constexpr u16x16 three(3.0f);
        u16x16 t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 sqrt(u16x16 x) {
        return x.mValue == 0 ? u16x16() : from_raw(static_cast<u32>(
            fl::isqrt64(static_cast<u64>(x.mValue) << FRAC_BITS)));
    }

    static constexpr FASTLED_FORCE_INLINE u16x16 rsqrt(u16x16 x) {
        return sqrt(x).mValue == 0
            ? u16x16()
            : from_raw(static_cast<u32>(1) << FRAC_BITS) / sqrt(x);
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
    static constexpr FASTLED_FORCE_INLINE int highest_bit(u32 v) {
        return v == 0 ? -1 : _highest_bit_step(v, 0);
    }

    static constexpr int _highest_bit_step(u32 v, int r) {
        return (v & 0xFFFF0000u) ? _highest_bit_step(v >> 16, r + 16)
             : (v & 0x0000FF00u) ? _highest_bit_step(v >> 8,  r + 8)
             : (v & 0x000000F0u) ? _highest_bit_step(v >> 4,  r + 4)
             : (v & 0x0000000Cu) ? _highest_bit_step(v >> 2,  r + 2)
             : (v & 0x00000002u) ? r + 1
             : r;
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
            t = (val >> (msb - FRAC_BITS)) - (SCALE);
        } else {
            t = (val << (FRAC_BITS - msb)) - (SCALE);
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

        u32 int_pow = (SCALE) << n;

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
