#pragma once

// CRTP base template for signed fixed-point arithmetic and trigonometry.
// All operations are integer-only in the hot path.
//
// Derived classes inherit via protected inheritance and re-expose methods
// via using declarations for explicit API control.

#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/isqrt.h"
#include "fl/fixed_point/fixed_point_traits.h"
#include "fl/compiler_control.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// CRTP base class for fixed-point types.
// Template parameters:
//   Derived  - The concrete type (e.g., s16x16) for CRTP pattern
//   IntBits  - Number of integer bits
//   FracBits - Number of fractional bits
template<typename Derived, int IntBits, int FracBits>
class fixed_point_base {
  public:
    using traits = fixed_point_traits<IntBits, FracBits>;
    using raw_type = typename traits::raw_type;
    using unsigned_raw_type = typename traits::unsigned_raw_type;
    using intermediate_type = typename traits::intermediate_type;
    using unsigned_intermediate_type = typename traits::unsigned_intermediate_type;
    using poly_intermediate_type = typename traits::poly_intermediate_type;

    static constexpr int INT_BITS = IntBits;
    static constexpr int FRAC_BITS = FracBits;

  protected:
    raw_type mValue = 0;

  public:

    // ---- Construction ------------------------------------------------------

    constexpr fixed_point_base() = default;

    explicit constexpr fixed_point_base(float f)
        : mValue(static_cast<raw_type>(f * (static_cast<raw_type>(1) << FRAC_BITS))) {}

    static FASTLED_FORCE_INLINE Derived from_raw(raw_type raw) {
        Derived r;
        r.mValue = raw;
        return r;
    }

    // ---- Access ------------------------------------------------------------

    raw_type raw() const { return mValue; }
    raw_type to_int() const { return mValue >> FRAC_BITS; }
    float to_float() const { return static_cast<float>(mValue) / (static_cast<raw_type>(1) << FRAC_BITS); }

    // ---- Fixed-point arithmetic --------------------------------------------

    FASTLED_FORCE_INLINE Derived operator*(Derived b) const {
        return Derived::from_raw(static_cast<raw_type>(
            (static_cast<intermediate_type>(mValue) * b.mValue) >> FRAC_BITS));
    }

    FASTLED_FORCE_INLINE Derived operator/(Derived b) const {
        return Derived::from_raw(static_cast<raw_type>(
            (static_cast<intermediate_type>(mValue) * (static_cast<intermediate_type>(1) << FRAC_BITS)) / b.mValue));
    }

    FASTLED_FORCE_INLINE Derived operator+(Derived b) const {
        return Derived::from_raw(mValue + b.mValue);
    }

    FASTLED_FORCE_INLINE Derived operator-(Derived b) const {
        return Derived::from_raw(mValue - b.mValue);
    }

    FASTLED_FORCE_INLINE Derived operator-() const {
        return Derived::from_raw(-mValue);
    }

    FASTLED_FORCE_INLINE Derived operator>>(int shift) const {
        return Derived::from_raw(mValue >> shift);
    }

    // ---- Scalar multiply (no fixed-point shift) ----------------------------

    FASTLED_FORCE_INLINE Derived operator*(raw_type scalar) const {
        return Derived::from_raw(mValue * scalar);
    }

    friend FASTLED_FORCE_INLINE Derived operator*(raw_type scalar, Derived fp) {
        return Derived::from_raw(scalar * fp.mValue);
    }

    // ---- Comparisons -------------------------------------------------------

    bool operator<(Derived b) const { return mValue < b.mValue; }
    bool operator>(Derived b) const { return mValue > b.mValue; }
    bool operator<=(Derived b) const { return mValue <= b.mValue; }
    bool operator>=(Derived b) const { return mValue >= b.mValue; }
    bool operator==(Derived b) const { return mValue == b.mValue; }
    bool operator!=(Derived b) const { return mValue != b.mValue; }

    // ---- Math ---------------------------------------------------------------

    static FASTLED_FORCE_INLINE Derived mod(Derived a, Derived b) {
        return Derived::from_raw(a.mValue % b.mValue);
    }

    static FASTLED_FORCE_INLINE Derived floor(Derived x) {
        constexpr raw_type frac_mask = (1 << FRAC_BITS) - 1;
        return Derived::from_raw(x.mValue & ~frac_mask);
    }

    static FASTLED_FORCE_INLINE Derived ceil(Derived x) {
        constexpr raw_type frac_mask = (1 << FRAC_BITS) - 1;
        raw_type floored = x.mValue & ~frac_mask;
        if (x.mValue & frac_mask) floored += (1 << FRAC_BITS);
        return Derived::from_raw(floored);
    }

    static FASTLED_FORCE_INLINE Derived fract(Derived x) {
        constexpr raw_type frac_mask = (1 << FRAC_BITS) - 1;
        return Derived::from_raw(x.mValue & frac_mask);
    }

    static FASTLED_FORCE_INLINE Derived abs(Derived x) {
        return Derived::from_raw(x.mValue < 0 ? -x.mValue : x.mValue);
    }

    static FASTLED_FORCE_INLINE int sign(Derived x) {
        if (x.mValue > 0) return 1;
        if (x.mValue < 0) return -1;
        return 0;
    }

    static FASTLED_FORCE_INLINE Derived lerp(Derived a, Derived b, Derived t) {
        return a + (b - a) * t;
    }

    static FASTLED_FORCE_INLINE Derived clamp(Derived x, Derived lo, Derived hi) {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static FASTLED_FORCE_INLINE Derived step(Derived edge, Derived x) {
        constexpr Derived one(1.0f);
        return x < edge ? Derived() : one;
    }

    static FASTLED_FORCE_INLINE Derived smoothstep(Derived edge0, Derived edge1, Derived x) {
        constexpr Derived zero(0.0f);
        constexpr Derived one(1.0f);
        constexpr Derived two(2.0f);
        constexpr Derived three(3.0f);
        Derived t = clamp((x - edge0) / (edge1 - edge0), zero, one);
        return t * t * (three - two * t);
    }

    // ---- Inverse Trigonometry (pure fixed-point) ----------------------------

    static FASTLED_FORCE_INLINE Derived atan(Derived x) {
        constexpr Derived one(1.0f);
        constexpr Derived pi_over_2(1.5707963f);
        bool neg = x.mValue < 0;
        Derived ax = abs(x);
        Derived result;
        if (ax <= one) {
            result = atan_unit(ax);
        } else {
            result = pi_over_2 - atan_unit(one / ax);
        }
        return neg ? -result : result;
    }

    static FASTLED_FORCE_INLINE Derived atan2(Derived y, Derived x) {
        constexpr Derived pi(3.1415926f);
        constexpr Derived pi_over_2(1.5707963f);
        if (x.mValue == 0 && y.mValue == 0) return Derived();
        if (x.mValue == 0) return y.mValue > 0 ? pi_over_2 : -pi_over_2;
        if (y.mValue == 0) return x.mValue > 0 ? Derived() : pi;
        Derived ax = abs(x);
        Derived ay = abs(y);
        Derived a;
        if (ax >= ay) {
            a = atan_unit(ay / ax);
        } else {
            a = pi_over_2 - atan_unit(ax / ay);
        }
        if (x.mValue < 0) a = pi - a;
        if (y.mValue < 0) a = -a;
        return a;
    }

    static FASTLED_FORCE_INLINE Derived asin(Derived x) {
        constexpr Derived one(1.0f);
        return atan2(x, sqrt(one - x * x));
    }

    static FASTLED_FORCE_INLINE Derived acos(Derived x) {
        constexpr Derived one(1.0f);
        return atan2(sqrt(one - x * x), x);
    }

    static FASTLED_FORCE_INLINE Derived sqrt(Derived x) {
        if (x.mValue <= 0) return Derived();
        return sqrt_impl(x, fl::bool_constant<traits::USE_ISQRT32>());
    }

  private:
    // sqrt implementation for i16 types (use isqrt32)
    static FASTLED_FORCE_INLINE Derived sqrt_impl(Derived x, fl::true_type) {
        return Derived::from_raw(static_cast<raw_type>(
            fl::isqrt32(static_cast<u32>(x.mValue) << FRAC_BITS)));
    }

    // sqrt implementation for i32 types (use isqrt64)
    static FASTLED_FORCE_INLINE Derived sqrt_impl(Derived x, fl::false_type) {
        return Derived::from_raw(static_cast<raw_type>(
            fl::isqrt64(static_cast<u64>(x.mValue) << FRAC_BITS)));
    }

  public:

    static FASTLED_FORCE_INLINE Derived rsqrt(Derived x) {
        Derived s = sqrt(x);
        if (s.mValue == 0) return Derived();
        return Derived::from_raw(static_cast<raw_type>(1) << FRAC_BITS) / s;
    }

    static FASTLED_FORCE_INLINE Derived pow(Derived base, Derived exp) {
        if (base.mValue <= 0) return Derived();
        constexpr Derived one(1.0f);
        if (exp.mValue == 0) return one;
        if (base == one) return one;
        return exp2_fp(exp * log2_fp(base));
    }

    // ---- Member function versions (operate on *this) -----------------------

    FASTLED_FORCE_INLINE Derived floor() const {
        return floor(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived ceil() const {
        return ceil(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived fract() const {
        return fract(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived abs() const {
        return abs(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE int sign() const {
        return sign(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived sin() const {
        return sin(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived cos() const {
        return cos(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived atan() const {
        return atan(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived asin() const {
        return asin(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived acos() const {
        return acos(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived sqrt() const {
        return sqrt(*static_cast<const Derived*>(this));
    }

    FASTLED_FORCE_INLINE Derived rsqrt() const {
        return rsqrt(*static_cast<const Derived*>(this));
    }

    // ---- Trigonometry ------------------------------------------------------

    static FASTLED_FORCE_INLINE Derived sin(Derived angle) {
        return Derived::from_raw(static_cast<raw_type>(
            fl::sin32(angle_to_a24(angle)) >> traits::SIN_COS_SHIFT));
    }

    static FASTLED_FORCE_INLINE Derived cos(Derived angle) {
        return Derived::from_raw(static_cast<raw_type>(
            fl::cos32(angle_to_a24(angle)) >> traits::SIN_COS_SHIFT));
    }

    // Combined sin+cos from radians. Output in [-1, 1].
    // Uses sincos32 which shares angle decomposition for ~30% fewer ops.
    static FASTLED_FORCE_INLINE void sincos(Derived angle, Derived &out_sin,
                                            Derived &out_cos) {
        u32 a24 = angle_to_a24(angle);
        SinCos32 sc = fl::sincos32(a24);
        out_sin = Derived::from_raw(static_cast<raw_type>(sc.sin_val >> traits::SIN_COS_SHIFT));
        out_cos = Derived::from_raw(static_cast<raw_type>(sc.cos_val >> traits::SIN_COS_SHIFT));
    }

  private:
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
    // Horner evaluation uses intermediate precision (IFRAC) to minimize
    // rounding error, then converts back to FRAC_BITS.
    static FASTLED_FORCE_INLINE Derived log2_fp(Derived x) {
        constexpr int IFRAC = traits::IFRAC;

        unsigned_raw_type val = static_cast<unsigned_raw_type>(x.mValue);
        int msb = highest_bit(static_cast<u32>(val));
        raw_type int_part = msb - FRAC_BITS;
        raw_type t;
        if (msb >= FRAC_BITS) {
            t = static_cast<raw_type>(
                (val >> (msb - FRAC_BITS)) - (static_cast<unsigned_raw_type>(1) << FRAC_BITS));
        } else {
            t = static_cast<raw_type>(
                (val << (FRAC_BITS - msb)) - (static_cast<unsigned_raw_type>(1) << FRAC_BITS));
        }

        // 4-term minimax coefficients for log2(1+t), t in [0,1).
        // Coefficients scaled by 2^IFRAC.
        // Use poly_intermediate_type (i32 or i64 based on IFRAC).
        using poly_type = poly_intermediate_type;
        constexpr poly_type c0 = static_cast<poly_type>(1.44179 * (1LL << IFRAC));
        constexpr poly_type c1 = static_cast<poly_type>(-0.69907 * (1LL << IFRAC));
        constexpr poly_type c2 = static_cast<poly_type>(0.36348 * (1LL << IFRAC));
        constexpr poly_type c3 = static_cast<poly_type>(-0.10660 * (1LL << IFRAC));

        // Extend t to IFRAC fractional bits
        poly_type t_ifrac = static_cast<poly_type>(t) << (IFRAC - FRAC_BITS);

        // Horner: t * (c0 + t * (c1 + t * (c2 + t * c3)))
        return log2_horner(int_part, t_ifrac, c0, c1, c2, c3, fl::bool_constant<(IFRAC <= 16)>());
    }

    // log2 Horner evaluation for i32 intermediates (IFRAC <= 16)
    template<typename PolyType>
    static FASTLED_FORCE_INLINE Derived log2_horner(raw_type int_part, PolyType t_ifrac,
                                                     PolyType c0, PolyType c1, PolyType c2, PolyType c3,
                                                     fl::true_type) {
        constexpr int IFRAC = traits::IFRAC;
        PolyType acc = c3;
        acc = c2 + static_cast<PolyType>((static_cast<i64>(acc) * t_ifrac) >> IFRAC);
        acc = c1 + static_cast<PolyType>((static_cast<i64>(acc) * t_ifrac) >> IFRAC);
        acc = c0 + static_cast<PolyType>((static_cast<i64>(acc) * t_ifrac) >> IFRAC);
        PolyType frac_part = static_cast<PolyType>((static_cast<i64>(acc) * t_ifrac) >> IFRAC);
        raw_type frac_result = static_cast<raw_type>(frac_part >> (IFRAC - FRAC_BITS));
        return Derived::from_raw((int_part << FRAC_BITS) + frac_result);
    }

    // log2 Horner evaluation for i64 intermediates (IFRAC > 16)
    template<typename PolyType>
    static FASTLED_FORCE_INLINE Derived log2_horner(raw_type int_part, PolyType t_ifrac,
                                                     PolyType c0, PolyType c1, PolyType c2, PolyType c3,
                                                     fl::false_type) {
        constexpr int IFRAC = traits::IFRAC;
        PolyType acc = c3;
        acc = c2 + ((acc * t_ifrac) >> IFRAC);
        acc = c1 + ((acc * t_ifrac) >> IFRAC);
        acc = c0 + ((acc * t_ifrac) >> IFRAC);
        PolyType frac_part = (acc * t_ifrac) >> IFRAC;
        raw_type frac_result = static_cast<raw_type>(frac_part >> (IFRAC - FRAC_BITS));
        return Derived::from_raw((int_part << FRAC_BITS) + frac_result);
    }

    // Fixed-point 2^x. Uses 4-term minimax polynomial for 2^t, t in [0,1).
    // Horner evaluation uses intermediate precision (IFRAC) to minimize
    // rounding error, then converts back to FRAC_BITS.
    static FASTLED_FORCE_INLINE Derived exp2_fp(Derived x) {
        constexpr int IFRAC = traits::IFRAC;

        Derived fl_val = floor(x);
        Derived fr = x - fl_val;
        raw_type n = fl_val.mValue >> FRAC_BITS;
        if (n >= INT_BITS - 1) return Derived::from_raw(traits::MAX_OVERFLOW);
        if (n < -FRAC_BITS) return Derived();

        raw_type int_pow;
        if (n >= 0) {
            int_pow = static_cast<raw_type>(static_cast<unsigned_raw_type>(1) << FRAC_BITS) << n;
        } else {
            int_pow = static_cast<raw_type>(static_cast<unsigned_raw_type>(1) << FRAC_BITS) >> (-n);
        }

        // 4-term minimax coefficients for 2^t - 1, t in [0,1).
        // Coefficients scaled by 2^IFRAC.
        using poly_type = poly_intermediate_type;
        constexpr poly_type d0 = static_cast<poly_type>(0.69316 * (1LL << IFRAC));
        constexpr poly_type d1 = static_cast<poly_type>(0.24071 * (1LL << IFRAC));
        constexpr poly_type d2 = static_cast<poly_type>(0.05336 * (1LL << IFRAC));
        constexpr poly_type d3 = static_cast<poly_type>(0.01276 * (1LL << IFRAC));

        // Extend fr to IFRAC fractional bits
        poly_type fr_ifrac = static_cast<poly_type>(fr.mValue) << (IFRAC - FRAC_BITS);

        // Horner: 1 + fr * (d0 + fr * (d1 + fr * (d2 + fr * d3)))
        return exp2_horner(int_pow, fr_ifrac, d0, d1, d2, d3, fl::bool_constant<(IFRAC <= 16)>());
    }

    // exp2 Horner evaluation for i32 intermediates (IFRAC <= 16)
    template<typename PolyType>
    static FASTLED_FORCE_INLINE Derived exp2_horner(raw_type int_pow, PolyType fr_ifrac,
                                                     PolyType d0, PolyType d1, PolyType d2, PolyType d3,
                                                     fl::true_type) {
        constexpr int IFRAC = traits::IFRAC;
        constexpr PolyType one_ifrac = static_cast<PolyType>(1) << IFRAC;
        PolyType acc = d3;
        acc = d2 + static_cast<PolyType>((static_cast<i64>(acc) * fr_ifrac) >> IFRAC);
        acc = d1 + static_cast<PolyType>((static_cast<i64>(acc) * fr_ifrac) >> IFRAC);
        acc = d0 + static_cast<PolyType>((static_cast<i64>(acc) * fr_ifrac) >> IFRAC);
        PolyType frac_pow_ifrac = one_ifrac + static_cast<PolyType>((static_cast<i64>(acc) * fr_ifrac) >> IFRAC);
        raw_type frac_pow = static_cast<raw_type>(frac_pow_ifrac >> (IFRAC - FRAC_BITS));
        intermediate_type result = (static_cast<intermediate_type>(int_pow) * frac_pow) >> FRAC_BITS;
        return Derived::from_raw(static_cast<raw_type>(result));
    }

    // exp2 Horner evaluation for i64 intermediates (IFRAC > 16)
    template<typename PolyType>
    static FASTLED_FORCE_INLINE Derived exp2_horner(raw_type int_pow, PolyType fr_ifrac,
                                                     PolyType d0, PolyType d1, PolyType d2, PolyType d3,
                                                     fl::false_type) {
        constexpr int IFRAC = traits::IFRAC;
        constexpr PolyType one_ifrac = static_cast<PolyType>(1) << IFRAC;
        PolyType acc = d3;
        acc = d2 + ((acc * fr_ifrac) >> IFRAC);
        acc = d1 + ((acc * fr_ifrac) >> IFRAC);
        acc = d0 + ((acc * fr_ifrac) >> IFRAC);
        PolyType frac_pow_ifrac = one_ifrac + ((acc * fr_ifrac) >> IFRAC);
        raw_type frac_pow = static_cast<raw_type>(frac_pow_ifrac >> (IFRAC - FRAC_BITS));
        intermediate_type result = (static_cast<intermediate_type>(int_pow) * frac_pow) >> FRAC_BITS;
        return Derived::from_raw(static_cast<raw_type>(result));
    }

    // Converts radians to sin32/cos32 input format.
    static FASTLED_FORCE_INLINE u32 angle_to_a24(Derived angle) {
        // 256/(2*PI) — converts radians to sin32/cos32 format.
        static constexpr i32 RAD_TO_24 = 2670177;
        // Always use i64 for multiplication to avoid overflow
        return static_cast<u32>(
            (static_cast<i64>(angle.mValue) * RAD_TO_24) >> FRAC_BITS);
    }

    // Polynomial atan for t in [0, 1]. Returns [0, π/4].
    // 7th-order minimax: atan(t) ≈ t * (c0 + t² * (c1 + t² * (c2 + t² * c3)))
    // Coefficients optimized via coordinate descent on s16x16 quantization grid.
    static FASTLED_FORCE_INLINE Derived atan_unit(Derived t) {
        constexpr Derived c0(0.9998779297f);
        constexpr Derived c1(-0.3269348145f);
        constexpr Derived c2(0.1594085693f);
        constexpr Derived c3(-0.0472106934f);
        Derived t2 = t * t;
        return t * (c0 + t2 * (c1 + t2 * (c2 + t2 * c3)));
    }
};

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
