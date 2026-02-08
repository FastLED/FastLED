#pragma once

// Signed 16.16 fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.

#include "fl/stl/stdint.h"
#include "fl/sin32.h"

namespace fl {

// Signed 16.16 fixed-point value type.
class s16x16 {
  public:
    static constexpr int INT_BITS = 16;
    static constexpr int FRAC_BITS = 16;

    // ---- Construction ------------------------------------------------------

    constexpr s16x16() = default;

    explicit constexpr s16x16(float f)
        : mValue(static_cast<int32_t>(f * (1 << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE s16x16 from_raw(int32_t raw) {
        s16x16 r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    int32_t raw() const { return mValue; }
    int32_t to_int() const { return mValue >> FRAC_BITS; }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE s16x16 operator*(s16x16 b) const {
        return from_raw(static_cast<int32_t>(
            (static_cast<int64_t>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE s16x16 operator/(s16x16 b) const {
        return from_raw(static_cast<int32_t>(
            (static_cast<int64_t>(mValue) << FRAC_BITS) / b.mValue));
    }

    FASTLED_FORCE_INLINE s16x16 operator+(s16x16 b) const {
        return from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE s16x16 operator-(s16x16 b) const {
        return from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE s16x16 operator-() const {
        return from_raw(-mValue);
    }

    FASTLED_FORCE_INLINE s16x16 operator>>(int shift) const {
        return from_raw(mValue >> shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE s16x16 operator*(int32_t scalar) const {
        return from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE s16x16 operator*(int32_t scalar, s16x16 fp) {
        return s16x16::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(s16x16 b) const { return mValue < b.mValue; }
    bool operator>(s16x16 b) const { return mValue > b.mValue; }
    bool operator<=(s16x16 b) const { return mValue <= b.mValue; }
    bool operator>=(s16x16 b) const { return mValue >= b.mValue; }
    bool operator==(s16x16 b) const { return mValue == b.mValue; }
    bool operator!=(s16x16 b) const { return mValue != b.mValue; }

    // ---- Trigonometry ------------------------------------------------------

    static FASTLED_FORCE_INLINE s16x16 sin(s16x16 angle) {
        return from_raw(fl::sin32(angle_to_a24(angle)) >> 15);
    }

    static FASTLED_FORCE_INLINE s16x16 cos(s16x16 angle) {
        return from_raw(fl::cos32(angle_to_a24(angle)) >> 15);
    }

    // Combined sin+cos from s16x16 radians. Output in s16x16 [-1, 1].
    static FASTLED_FORCE_INLINE void sincos(s16x16 angle, s16x16 &out_sin,
                                            s16x16 &out_cos) {
        uint32_t a24 = angle_to_a24(angle);
        out_sin = from_raw(fl::sin32(a24) >> 15);
        out_cos = from_raw(fl::cos32(a24) >> 15);
    }

  private:
    int32_t mValue = 0;

    // Converts s16x16 radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE uint32_t angle_to_a24(s16x16 angle) {
        // 256/(2*PI) in s16x16 — converts radians to sin32/cos32 format.
        static constexpr int32_t RAD_TO_24 = 2670177;
        return static_cast<uint32_t>(
            (static_cast<int64_t>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }
};

} // namespace fl
