#pragma once

// Unsigned 0.32 fixed-point arithmetic.
// Represents normalized values in range [0.0, 1.0).

#include "fl/int.h"
#include "fl/compiler_control.h"
#include "fl/stl/type_traits.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// Forward declaration for cross-type operations
class s16x16;

// Unsigned 0.32 fixed-point value type (UQ32 format).
// Represents values in range [0.0, 1.0) using all 32 bits for fractional precision.
//
// Bit layout (u32 storage):
//   Bits 31-0: Fractional magnitude (32 bits of precision)
//   Value interpretation: raw_u32 / 2^32
//
// Primary use cases:
//   - Normalized alpha/opacity values
//   - Color blending factors
//   - Normalized coordinate systems (unsigned)
//   - Probability values
class u0x32 {
  public:
    static constexpr int INT_BITS = 0;   // No integer bits (always in [0, 1))
    static constexpr int FRAC_BITS = 32; // 32 fractional bits (all bits)

    // ---- Construction ------------------------------------------------------

    constexpr u0x32() = default;

    // Construct from float (clamps to [0.0, 1.0) range)
    // UQ32 format: max value is 0xFFFFFFFF (just under 1.0), min is 0 (exactly 0.0)
    explicit constexpr u0x32(float f)
        : mValue(f <= 0.0f ? 0U :                       // Exactly 0.0
                 f >= 1.0f ? 0xFFFFFFFFU :              // Max positive (0.9999999997...)
                 static_cast<u32>(f * 4294967296.0f)) {}

    // Auto-promotion from other fixed-point types
    // u0x32 has INT_BITS=0, so it can only accept types with INT_BITS=0
    // and FRAC_BITS ≤ 32 (but since u0x32 has the most fractional precision, no other type qualifies)
    // This constructor allows future expansion if more normalized types are added
    template <typename OtherFP>
    constexpr u0x32(const OtherFP& other,
                    typename fl::enable_if<
                        (OtherFP::INT_BITS <= INT_BITS) &&
                        (OtherFP::FRAC_BITS <= FRAC_BITS) &&
                        (OtherFP::INT_BITS != INT_BITS || OtherFP::FRAC_BITS != FRAC_BITS),
                        int>::type = 0)
        : mValue(static_cast<u32>(
            static_cast<u64>(other.raw()) << (FRAC_BITS - OtherFP::FRAC_BITS))) {}

    // Construct from raw u32 value (UQ32 format)
    static FASTLED_FORCE_INLINE u0x32 from_raw(u32 raw) {
        u0x32 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    u32 raw() const { return mValue; }

    // Convert to integer (always 0 since range is [0.0, 1.0))
    u32 to_int() const { return 0; }

    float to_float() const {
        return static_cast<float>(mValue) / 4294967296.0f;
    }

    // ---- Same-type arithmetic (u0x32 OP u0x32 → u0x32) --------------------

    FASTLED_FORCE_INLINE u0x32 operator+(u0x32 b) const {
        // Saturating add to prevent overflow
        u32 result = mValue + b.mValue;
        if (result < mValue) return from_raw(0xFFFFFFFFU); // Overflow, clamp to max
        return from_raw(result);
    }

    FASTLED_FORCE_INLINE u0x32 operator-(u0x32 b) const {
        // Saturating subtract to prevent underflow
        if (b.mValue > mValue) return from_raw(0); // Underflow, clamp to 0
        return from_raw(mValue - b.mValue);
    }

    // Multiply two normalized values: u0x32 × u0x32 → u0x32
    // Both inputs < 1.0, so product < 1.0
    FASTLED_FORCE_INLINE u0x32 operator*(u0x32 b) const {
        // UQ32 × UQ32 = UQ64 → shift right 32 → UQ32
        return from_raw(static_cast<u32>(
            (static_cast<u64>(mValue) * b.mValue) >> 32));
    }

    // Divide normalized values: u0x32 / u0x32 → u0x32
    FASTLED_FORCE_INLINE u0x32 operator/(u0x32 b) const {
        // UQ32 / UQ32: shift dividend left 32 bits then divide
        // (a / 2^32) / (b / 2^32) = a / b → need (a << 32) / b
        if (b.mValue == 0) return from_raw(0xFFFFFFFFU); // Division by zero, return max
        u64 result = (static_cast<u64>(mValue) << 32) / b.mValue;
        if (result > 0xFFFFFFFFULL) return from_raw(0xFFFFFFFFU); // Overflow, saturate to max
        return from_raw(static_cast<u32>(result));
    }

    FASTLED_FORCE_INLINE u0x32 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    FASTLED_FORCE_INLINE u0x32 operator<<(int shift) const {
        return from_raw(mValue << shift);
    }

    // ---- Scalar arithmetic (u0x32 × raw integer → u0x32) ------------------

    FASTLED_FORCE_INLINE u0x32 operator*(u32 scalar) const {
        // UQ32 * scalar with saturation to prevent overflow
        u64 result = static_cast<u64>(mValue) * scalar;
        if (result > 0xFFFFFFFFULL) return from_raw(0xFFFFFFFFU);
        return from_raw(static_cast<u32>(result));
    }

    friend FASTLED_FORCE_INLINE u0x32 operator*(u32 scalar, u0x32 a) {
        return a * scalar;  // Commutative
    }

    FASTLED_FORCE_INLINE u0x32 operator/(u32 scalar) const {
        if (scalar == 0) return from_raw(0xFFFFFFFFU); // Division by zero, return max
        return from_raw(mValue / scalar);
    }

    // ---- Math functions ----------------------------------------------------

    static FASTLED_FORCE_INLINE u0x32 min(u0x32 a, u0x32 b) {
        return from_raw(a.mValue < b.mValue ? a.mValue : b.mValue);
    }

    static FASTLED_FORCE_INLINE u0x32 max(u0x32 a, u0x32 b) {
        return from_raw(a.mValue > b.mValue ? a.mValue : b.mValue);
    }

    static FASTLED_FORCE_INLINE u0x32 clamp(u0x32 val, u0x32 low, u0x32 high) {
        return max(low, min(val, high));
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(u0x32 b) const { return mValue < b.mValue; }
    bool operator>(u0x32 b) const { return mValue > b.mValue; }
    bool operator<=(u0x32 b) const { return mValue <= b.mValue; }
    bool operator>=(u0x32 b) const { return mValue >= b.mValue; }
    bool operator==(u0x32 b) const { return mValue == b.mValue; }
    bool operator!=(u0x32 b) const { return mValue != b.mValue; }

  private:
    u32 mValue = 0;
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
