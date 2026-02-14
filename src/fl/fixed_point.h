#pragma once

// Template system for fixed-point types with sign support.
// Usage: fl::fixed_point<16, 16> (defaults to signed)
//        fl::fixed_point<16, 16, fl::Sign::UNSIGNED> (explicit unsigned)

#include "fl/fixed_point/fixed_point_traits.h"
#include "fl/fixed_point/fixed_point_base.h"
#include "fl/fixed_point/s0x32.h"
#include "fl/fixed_point/s4x12.h"
#include "fl/fixed_point/s8x8.h"
#include "fl/fixed_point/s8x24.h"
#include "fl/fixed_point/s12x4.h"
#include "fl/fixed_point/s16x16.h"
#include "fl/fixed_point/s24x8.h"
#include "fl/fixed_point/u0x32.h"
#include "fl/fixed_point/u4x12.h"
#include "fl/fixed_point/u8x8.h"
#include "fl/fixed_point/u8x24.h"
#include "fl/fixed_point/u12x4.h"
#include "fl/fixed_point/u16x16.h"
#include "fl/fixed_point/u24x8.h"
#include "fl/stl/type_traits.h"

namespace fl {

// Sign enum for fixed-point types
enum class Sign {
    SIGNED,
    UNSIGNED
};

// Primary template — intentionally undefined.
// Only valid specializations (matching concrete types) may be used.
template <int IntBits, int FracBits, Sign S>
struct fixed_point_impl;

// Signed specializations
template <> struct fixed_point_impl<0, 32, Sign::SIGNED>   { using type = s0x32; };
template <> struct fixed_point_impl<4, 12, Sign::SIGNED>   { using type = s4x12; };
template <> struct fixed_point_impl<8, 8, Sign::SIGNED>    { using type = s8x8; };
template <> struct fixed_point_impl<8, 24, Sign::SIGNED>   { using type = s8x24; };
template <> struct fixed_point_impl<12, 4, Sign::SIGNED>   { using type = s12x4; };
template <> struct fixed_point_impl<16, 16, Sign::SIGNED>  { using type = s16x16; };
template <> struct fixed_point_impl<24, 8, Sign::SIGNED>   { using type = s24x8; };

// Unsigned specializations
template <> struct fixed_point_impl<0, 32, Sign::UNSIGNED>  { using type = u0x32; };
template <> struct fixed_point_impl<4, 12, Sign::UNSIGNED>  { using type = u4x12; };
template <> struct fixed_point_impl<8, 8, Sign::UNSIGNED>   { using type = u8x8; };
template <> struct fixed_point_impl<8, 24, Sign::UNSIGNED>  { using type = u8x24; };
template <> struct fixed_point_impl<12, 4, Sign::UNSIGNED>  { using type = u12x4; };
template <> struct fixed_point_impl<16, 16, Sign::UNSIGNED> { using type = u16x16; };
template <> struct fixed_point_impl<24, 8, Sign::UNSIGNED>  { using type = u24x8; };

// Forward declaration for is_fp_promotable trait
template <int IntBits, int FracBits, Sign S = Sign::SIGNED>
class fixed_point;

// Type trait: Check if fixed_point type From can be promoted to type To
// Promotion rules: both INT_BITS and FRAC_BITS must increase (or stay same),
// signs must match, and types must be different
template <typename From, typename To>
struct is_fp_promotable {
    static constexpr bool value = false;
};

template <int FromInt, int FromFrac, Sign FromSign, int ToInt, int ToFrac, Sign ToSign>
struct is_fp_promotable<fixed_point<FromInt, FromFrac, FromSign>, fixed_point<ToInt, ToFrac, ToSign>> {
    static constexpr bool value =
        (FromInt <= ToInt) &&
        (FromFrac <= ToFrac) &&
        (FromSign == ToSign) &&
        (FromInt != ToInt || FromFrac != ToFrac);
};

// Wrapper class that adds auto-promotion and forwards to concrete types
template <int IntBits, int FracBits, Sign S>
class fixed_point : protected fixed_point_impl<IntBits, FracBits, S>::type {
  private:
    using Base = typename fixed_point_impl<IntBits, FracBits, S>::type;
    using RawType = decltype(Base().raw());

    // Helper: Select intermediate type for promotion (i64 for signed, u64 for unsigned)
    using PromotionType = typename conditional<(S == Sign::SIGNED), i64, u64>::type;

    // Helper: Promote fixed-point value to higher precision with fractional bit shift
    template <int OtherFrac, typename OtherRawType>
    static constexpr RawType promote_fp(OtherRawType other_raw) {
        return static_cast<RawType>(
            static_cast<PromotionType>(other_raw) << (FracBits - OtherFrac));
    }

  public:
    // ---- Type constants (exposed via using) ----
    using Base::INT_BITS;
    using Base::FRAC_BITS;

    // ---- Constructors ----

    // Default constructor
    FASTLED_FORCE_INLINE constexpr fixed_point() : Base() {}

    // Float constructor
    FASTLED_FORCE_INLINE explicit constexpr fixed_point(float f) : Base(f) {}

    // Auto-promotion from other fixed-point types
    // Enabled when: is_fp_promotable<From, To>::value is true
    template <int OtherInt, int OtherFrac, Sign OtherSign>
    constexpr fixed_point(
        const fixed_point<OtherInt, OtherFrac, OtherSign>& other,
        typename enable_if<
            is_fp_promotable<
                fixed_point<OtherInt, OtherFrac, OtherSign>,
                fixed_point<IntBits, FracBits, S>
            >::value,
            int>::type = 0)
        : Base(Base::from_raw(promote_fp<OtherFrac>(other.raw()))) {}

    // ---- Factory methods ----

    static FASTLED_FORCE_INLINE fixed_point from_raw(RawType raw) {
        fixed_point result;
        result.Base::operator=(Base::from_raw(raw));
        return result;
    }

    // ---- Conversion methods ----

    FASTLED_FORCE_INLINE RawType raw() const { return Base::raw(); }
    FASTLED_FORCE_INLINE i32 to_int() const { return Base::to_int(); }
    FASTLED_FORCE_INLINE float to_float() const { return Base::to_float(); }

    // ---- Arithmetic operators ----

    FASTLED_FORCE_INLINE fixed_point operator+(fixed_point b) const {
        return from_raw((Base::operator+(b)).raw());
    }

    FASTLED_FORCE_INLINE fixed_point operator-(fixed_point b) const {
        return from_raw((Base::operator-(b)).raw());
    }

    FASTLED_FORCE_INLINE fixed_point operator*(fixed_point b) const {
        return from_raw((Base::operator*(b)).raw());
    }

    FASTLED_FORCE_INLINE fixed_point operator/(fixed_point b) const {
        return from_raw((Base::operator/(b)).raw());
    }

    FASTLED_FORCE_INLINE fixed_point operator-() const {
        return from_raw((Base::operator-()).raw());
    }

    FASTLED_FORCE_INLINE fixed_point operator>>(int shift) const {
        return from_raw((Base::operator>>(shift)).raw());
    }

    FASTLED_FORCE_INLINE fixed_point operator<<(int shift) const {
        return from_raw((Base::operator<<(shift)).raw());
    }

    // ---- Scalar multiplication ----

    FASTLED_FORCE_INLINE fixed_point operator*(i32 scalar) const {
        return from_raw((Base::operator*(scalar)).raw());
    }

    friend FASTLED_FORCE_INLINE fixed_point operator*(i32 scalar, fixed_point fp) {
        return from_raw((scalar * static_cast<Base>(fp)).raw());
    }

    // ---- Comparison operators ----

    FASTLED_FORCE_INLINE bool operator<(fixed_point b) const {
        return Base::operator<(b);
    }

    FASTLED_FORCE_INLINE bool operator>(fixed_point b) const {
        return Base::operator>(b);
    }

    FASTLED_FORCE_INLINE bool operator<=(fixed_point b) const {
        return Base::operator<=(b);
    }

    FASTLED_FORCE_INLINE bool operator>=(fixed_point b) const {
        return Base::operator>=(b);
    }

    FASTLED_FORCE_INLINE bool operator==(fixed_point b) const {
        return Base::operator==(b);
    }

    FASTLED_FORCE_INLINE bool operator!=(fixed_point b) const {
        return Base::operator!=(b);
    }

    // ---- Static math functions ----

    static FASTLED_FORCE_INLINE fixed_point mod(fixed_point a, fixed_point b) {
        return from_raw(Base::mod(a, b).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point floor(fixed_point x) {
        return from_raw(Base::floor(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point ceil(fixed_point x) {
        return from_raw(Base::ceil(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point fract(fixed_point x) {
        return from_raw(Base::fract(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point abs(fixed_point x) {
        return from_raw(Base::abs(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point sign(fixed_point x) {
        return from_raw(Base::sign(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point lerp(fixed_point a, fixed_point b, fixed_point t) {
        return from_raw(Base::lerp(a, b, t).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point clamp(fixed_point x, fixed_point lo, fixed_point hi) {
        return from_raw(Base::clamp(x, lo, hi).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point step(fixed_point edge, fixed_point x) {
        return from_raw(Base::step(edge, x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point smoothstep(fixed_point edge0, fixed_point edge1, fixed_point x) {
        return from_raw(Base::smoothstep(edge0, edge1, x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point min(fixed_point a, fixed_point b) {
        return from_raw(Base::min(a, b).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point max(fixed_point a, fixed_point b) {
        return from_raw(Base::max(a, b).raw());
    }

    // ---- Trigonometry ----

    static FASTLED_FORCE_INLINE fixed_point sin(fixed_point angle) {
        return from_raw(Base::sin(angle).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point cos(fixed_point angle) {
        return from_raw(Base::cos(angle).raw());
    }

    static FASTLED_FORCE_INLINE void sincos(fixed_point angle, fixed_point& out_sin, fixed_point& out_cos) {
        Base base_sin, base_cos;
        Base::sincos(angle, base_sin, base_cos);
        out_sin = from_raw(base_sin.raw());
        out_cos = from_raw(base_cos.raw());
    }

    static FASTLED_FORCE_INLINE fixed_point atan(fixed_point x) {
        return from_raw(Base::atan(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point atan2(fixed_point y, fixed_point x) {
        return from_raw(Base::atan2(y, x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point asin(fixed_point x) {
        return from_raw(Base::asin(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point acos(fixed_point x) {
        return from_raw(Base::acos(x).raw());
    }

    // ---- Other math functions ----

    static FASTLED_FORCE_INLINE fixed_point sqrt(fixed_point x) {
        return from_raw(Base::sqrt(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point rsqrt(fixed_point x) {
        return from_raw(Base::rsqrt(x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point pow(fixed_point base, fixed_point exp) {
        return from_raw(Base::pow(base, exp).raw());
    }
};

// Convenience aliases (backward compatibility)
template <int IntBits, int FracBits>
using sfixed_point = fixed_point<IntBits, FracBits, Sign::SIGNED>;

template <int IntBits, int FracBits>
using ufixed_point = fixed_point<IntBits, FracBits, Sign::UNSIGNED>;

} // namespace fl

// ---- Cross-type operator implementations (after all types are fully defined) ----
#include "fl/fixed_point/scalar_ops.h"  // allow-include-after-namespace
