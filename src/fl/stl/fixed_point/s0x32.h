#pragma once

// Signed 0.32 fixed-point arithmetic.
// Represents normalized values in range [-1.0, 1.0].

#include "fl/int.h"
#include "fl/compiler_control.h"
#include "fl/stl/type_traits.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Forward declaration for cross-type operations
class s16x16;

// Signed 0.32 fixed-point value type (Q31 format in DSP terminology).
// Represents values in range [-1.0, 1.0] using all 32 bits for fractional precision.
//
// Bit layout (i32 storage, two's complement):
//   Bit 31: Sign bit (implicit in two's complement)
//   Bits 30-0: Fractional magnitude (31 bits of precision)
//   Value interpretation: raw_i32 / 2^31
//
// Primary use cases:
//   - sin32/cos32 output (normalized trigonometric values)
//   - Color scaling factors
//   - Normalized coordinate systems
class s0x32 {
  public:
    static constexpr int INT_BITS = 0;   // No integer bits (always in [-1, 1])
    static constexpr int FRAC_BITS = 32; // 32 fractional bits (all bits)

    // ---- Construction ------------------------------------------------------

    constexpr s0x32() = default;

    // Construct from float (clamps to [-1.0, 1.0] range)
    // Q31 format: max value is 0x7FFFFFFF (just under 1.0), min is 0x80000000 (-1.0)
    explicit constexpr s0x32(float f)
        : mValue(f <= -1.0f ? (i32)0x80000000 :     // Exactly -1.0
                 f >= 1.0f ? (i32)0x7FFFFFFF :      // Max positive (0.9999999995...)
                 static_cast<i32>(f * 2147483648.0f)) {}

    // Auto-promotion from other fixed-point types
    // s0x32 has INT_BITS=0, so it can only accept types with INT_BITS=0
    // and FRAC_BITS ≤ 32 (but since s0x32 has the most fractional precision, no other type qualifies)
    // This constructor allows future expansion if more normalized types are added
    template <typename OtherFP>
    constexpr s0x32(const OtherFP& other,
                    typename fl::enable_if<
                        (OtherFP::INT_BITS <= INT_BITS) &&
                        (OtherFP::FRAC_BITS <= FRAC_BITS) &&
                        (OtherFP::INT_BITS != INT_BITS || OtherFP::FRAC_BITS != FRAC_BITS),
                        int>::type = 0)
        : mValue(static_cast<i32>(
            static_cast<i64>(other.raw()) << (FRAC_BITS - OtherFP::FRAC_BITS))) {}

    // Construct from raw i32 value (Q31 format)
    // Raw constructor for C++11 constexpr from_raw
    struct RawTag {};
    constexpr explicit s0x32(i32 raw, RawTag) : mValue(raw) {}

    static constexpr FASTLED_FORCE_INLINE s0x32 from_raw(i32 raw) {
        return s0x32(raw, RawTag());
    }

    // ---- Access ------------------------------------------------------------

    constexpr i32 raw() const { return mValue; }

    // Convert to integer (always 0 or -1 since range is [-1.0, 1.0])
    constexpr i32 to_int() const { return mValue >> 31; }

    constexpr float to_float() const {
        return static_cast<float>(mValue) / (1LL << 31);
    }

    // ---- Same-type arithmetic (s0x32 OP s0x32 → s0x32) --------------------

    constexpr FASTLED_FORCE_INLINE s0x32 operator+(s0x32 b) const {
        return from_raw(static_cast<i32>(
            static_cast<u32>(mValue) + static_cast<u32>(b.mValue)));
    }

    constexpr FASTLED_FORCE_INLINE s0x32 operator-(s0x32 b) const {
        return from_raw(static_cast<i32>(
            static_cast<u32>(mValue) - static_cast<u32>(b.mValue)));
    }

    constexpr FASTLED_FORCE_INLINE s0x32 operator-() const {
        return from_raw(static_cast<i32>(0u - static_cast<u32>(mValue)));
    }

    // Multiply two normalized values: s0x32 × s0x32 → s0x32
    // Both inputs ≤ 1.0, so product ≤ 1.0
    constexpr FASTLED_FORCE_INLINE s0x32 operator*(s0x32 b) const {
        // Q31 × Q31 = Q62 → shift right 31 → Q31
        return from_raw(static_cast<i32>(
            (static_cast<i64>(mValue) * b.mValue) >> 31));
    }

    // Divide normalized values: s0x32 / s0x32 → s0x32
    constexpr FASTLED_FORCE_INLINE s0x32 operator/(s0x32 b) const {
        // Q31 / Q31: shift dividend left 31 bits then divide
        // (a / 2^31) / (b / 2^31) = a / b → need (a << 31) / b
        return from_raw(static_cast<i32>(
            (static_cast<i64>(mValue) << 31) / b.mValue));
    }

    constexpr FASTLED_FORCE_INLINE s0x32 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    constexpr FASTLED_FORCE_INLINE s0x32 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Scalar arithmetic (s0x32 × raw integer → s0x32) ------------------

    constexpr FASTLED_FORCE_INLINE s0x32 operator*(i32 scalar) const {
        // Q31 * scalar with clamping to prevent overflow
        return (static_cast<i64>(mValue) * scalar > 0x7FFFFFFFLL)
            ? from_raw(0x7FFFFFFF)
            : (static_cast<i64>(mValue) * scalar < -0x80000000LL)
                ? from_raw(static_cast<i32>(0x80000000u))
                : from_raw(static_cast<i32>(static_cast<i64>(mValue) * scalar));
    }

    friend constexpr s0x32 operator*(i32 scalar, s0x32 a) {
        return a * scalar;  // Commutative
    }

    constexpr FASTLED_FORCE_INLINE s0x32 operator/(i32 scalar) const {
        return from_raw(mValue / scalar);
    }

    // ---- Cross-type operations (s0x32 × s16x16 → s16x16) ------------------

    // Multiply normalized value (s0x32) by general fixed-point (s16x16)
    // Common pattern: sin/cos × distance
    // Math: Q31 × Q16 = Q47 → shift right 31 → Q16
    // Implemented in fixed_point.h after s16x16 is fully defined
    constexpr FASTLED_FORCE_INLINE s16x16 operator*(s16x16 b) const;

    friend constexpr s16x16 operator*(s16x16 a, s0x32 b);

    // ---- Math functions ----------------------------------------------------

    static constexpr FASTLED_FORCE_INLINE s0x32 abs(s0x32 x) {
        return from_raw(x.mValue < 0 ? -x.mValue : x.mValue);
    }

    static constexpr FASTLED_FORCE_INLINE s0x32 min(s0x32 a, s0x32 b) {
        return from_raw(a.mValue < b.mValue ? a.mValue : b.mValue);
    }

    static constexpr FASTLED_FORCE_INLINE s0x32 max(s0x32 a, s0x32 b) {
        return from_raw(a.mValue > b.mValue ? a.mValue : b.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    constexpr bool operator<(s0x32 b) const { return mValue < b.mValue; }
    constexpr bool operator>(s0x32 b) const { return mValue > b.mValue; }
    constexpr bool operator<=(s0x32 b) const { return mValue <= b.mValue; }
    constexpr bool operator>=(s0x32 b) const { return mValue >= b.mValue; }
    constexpr bool operator==(s0x32 b) const { return mValue == b.mValue; }
    constexpr bool operator!=(s0x32 b) const { return mValue != b.mValue; }

  private:
    i32 mValue = 0;
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
