#pragma once

// Template system for fixed-point types with sign support.
// Usage: fl::fixed_point<16, 16> (defaults to signed)
//        fl::fixed_point<16, 16, fl::Sign::UNSIGNED> (explicit unsigned)
//
// Free-function math API (std::cmath style):
//   fl::floor(x), fl::ceil(x), fl::fract(x), fl::abs(x)
//   fl::sqrt(x), fl::rsqrt(x), fl::pow(x, y)
//   fl::sin(x), fl::cos(x), fl::sincos(angle, &s, &c)
//   fl::atan(x), fl::atan2(y, x), fl::asin(x), fl::acos(x)
//   fl::lerp(a, b, t), fl::clamp(x, lo, hi), fl::mod(a, b)
//   fl::step(edge, x), fl::smoothstep(e0, e1, x)
//
// numeric_limits specializations in fl/stl/limits.h

#include "fl/stl/fixed_point/s0x32.h"
#include "fl/stl/fixed_point/s4x12.h"
#include "fl/stl/fixed_point/s8x8.h"
#include "fl/stl/fixed_point/s8x24.h"
#include "fl/stl/fixed_point/s12x4.h"
#include "fl/stl/fixed_point/s16x16.h"
#include "fl/stl/fixed_point/s24x8.h"
#include "fl/stl/fixed_point/u0x32.h"
#include "fl/stl/fixed_point/u4x12.h"
#include "fl/stl/fixed_point/u8x8.h"
#include "fl/stl/fixed_point/u8x24.h"
#include "fl/stl/fixed_point/u12x4.h"
#include "fl/stl/fixed_point/u16x16.h"
#include "fl/stl/fixed_point/u24x8.h"
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

    // Raw constructor for constexpr from_raw
    constexpr explicit fixed_point(RawType raw, typename Base::RawTag tag) : Base(raw, tag) {}

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

    static constexpr FASTLED_FORCE_INLINE fixed_point from_raw(RawType raw) {
        return fixed_point(raw, typename Base::RawTag());
    }

    // ---- Conversion methods ----

    FASTLED_FORCE_INLINE constexpr RawType raw() const { return Base::raw(); }
    constexpr FASTLED_FORCE_INLINE i32 to_int() const { return Base::to_int(); }
    constexpr FASTLED_FORCE_INLINE float to_float() const { return Base::to_float(); }

    // ---- Arithmetic operators ----

    constexpr FASTLED_FORCE_INLINE fixed_point operator+(fixed_point b) const {
        return from_raw((Base::operator+(b)).raw());
    }

    constexpr FASTLED_FORCE_INLINE fixed_point operator-(fixed_point b) const {
        return from_raw((Base::operator-(b)).raw());
    }

    constexpr FASTLED_FORCE_INLINE fixed_point operator*(fixed_point b) const {
        return from_raw((Base::operator*(b)).raw());
    }

    constexpr FASTLED_FORCE_INLINE fixed_point operator/(fixed_point b) const {
        return from_raw((Base::operator/(b)).raw());
    }

    constexpr FASTLED_FORCE_INLINE fixed_point operator-() const {
        return from_raw((Base::operator-()).raw());
    }

    constexpr FASTLED_FORCE_INLINE fixed_point operator>>(int shift) const {
        return from_raw((Base::operator>>(shift)).raw());
    }

    constexpr FASTLED_FORCE_INLINE fixed_point operator<<(int shift) const {
        return from_raw((Base::operator<<(shift)).raw());
    }

    // ---- Scalar multiplication ----

    constexpr FASTLED_FORCE_INLINE fixed_point operator*(i32 scalar) const {
        return from_raw((Base::operator*(scalar)).raw());
    }

    friend constexpr fixed_point operator*(i32 scalar, fixed_point fp) {
        return from_raw((scalar * static_cast<Base>(fp)).raw());
    }

    // ---- Comparison operators ----

    constexpr FASTLED_FORCE_INLINE bool operator<(fixed_point b) const {
        return Base::operator<(b);
    }

    constexpr FASTLED_FORCE_INLINE bool operator>(fixed_point b) const {
        return Base::operator>(b);
    }

    constexpr FASTLED_FORCE_INLINE bool operator<=(fixed_point b) const {
        return Base::operator<=(b);
    }

    constexpr FASTLED_FORCE_INLINE bool operator>=(fixed_point b) const {
        return Base::operator>=(b);
    }

    constexpr FASTLED_FORCE_INLINE bool operator==(fixed_point b) const {
        return Base::operator==(b);
    }

    constexpr FASTLED_FORCE_INLINE bool operator!=(fixed_point b) const {
        return Base::operator!=(b);
    }

    // ---- Static math functions ----

    static constexpr FASTLED_FORCE_INLINE fixed_point mod(fixed_point a, fixed_point b) {
        return from_raw(Base::mod(a, b).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point floor(fixed_point x) {
        return from_raw(Base::floor(x).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point ceil(fixed_point x) {
        return from_raw(Base::ceil(x).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point fract(fixed_point x) {
        return from_raw(Base::fract(x).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point abs(fixed_point x) {
        return from_raw(Base::abs(x).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point sign(fixed_point x) {
        return from_raw(Base::sign(x).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point lerp(fixed_point a, fixed_point b, fixed_point t) {
        return from_raw(Base::lerp(a, b, t).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point clamp(fixed_point x, fixed_point lo, fixed_point hi) {
        return from_raw(Base::clamp(x, lo, hi).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point step(fixed_point edge, fixed_point x) {
        return from_raw(Base::step(edge, x).raw());
    }

    static FASTLED_FORCE_INLINE fixed_point smoothstep(fixed_point edge0, fixed_point edge1, fixed_point x) {
        return from_raw(Base::smoothstep(edge0, edge1, x).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point min(fixed_point a, fixed_point b) {
        return from_raw(Base::min(a, b).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point max(fixed_point a, fixed_point b) {
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

    static constexpr FASTLED_FORCE_INLINE fixed_point sqrt(fixed_point x) {
        return from_raw(Base::sqrt(x).raw());
    }

    static constexpr FASTLED_FORCE_INLINE fixed_point rsqrt(fixed_point x) {
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

//-------------------------------------------------------------------------------
// is_fixed_point trait — specialization for fixed_point<I,F,S>
// (default false_type lives in fl/stl/type_traits.h)
//-------------------------------------------------------------------------------
template <int I, int F, Sign S>
struct is_fixed_point<fixed_point<I, F, S>> : true_type {};

// Handle cv-qualified and reference types
template <typename T> struct is_fixed_point<const T> : is_fixed_point<T> {};
template <typename T> struct is_fixed_point<volatile T> : is_fixed_point<T> {};
template <typename T> struct is_fixed_point<const volatile T> : is_fixed_point<T> {};
template <typename T> struct is_fixed_point<T&> : is_fixed_point<T> {};

//-------------------------------------------------------------------------------
// Free-function math API (std::cmath style)
// These enable ADL: fl::sin(x), fl::floor(x), etc.
//-------------------------------------------------------------------------------

// ---- Rounding / decomposition ----

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
floor(T x) { return T::floor(x); }

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
ceil(T x) { return T::ceil(x); }

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
fract(T x) { return T::fract(x); }

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
mod(T a, T b) { return T::mod(a, b); }

// ---- Absolute value / sign ----

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
abs(T x) { return T::abs(x); }

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, int>::type
sign(T x) { return T::sign(x); }

// ---- Interpolation / clamping ----

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
lerp(T a, T b, T t) { return T::lerp(a, b, t); }

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
clamp(T x, T lo, T hi) { return T::clamp(x, lo, hi); }

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
step(T edge, T x) { return T::step(edge, x); }

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
smoothstep(T edge0, T edge1, T x) { return T::smoothstep(edge0, edge1, x); }

// ---- Roots / powers ----

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
sqrt(T x) { return T::sqrt(x); }

template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
rsqrt(T x) { return T::rsqrt(x); }

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
pow(T base, T exp) { return T::pow(base, exp); }

// ---- Trigonometry ----

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
sin(T angle) { return T::sin(angle); }

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
cos(T angle) { return T::cos(angle); }

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, void>::type
sincos(T angle, T& out_sin, T& out_cos) { T::sincos(angle, out_sin, out_cos); }

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
atan(T x) { return T::atan(x); }

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
atan2(T y, T x) { return T::atan2(y, x); }

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
asin(T x) { return T::asin(x); }

template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
acos(T x) { return T::acos(x); }

//-------------------------------------------------------------------------------
// powfp<T>(base, exp) — free-function power for fixed_point types.
// Calls T::pow(base, exp) which uses exp2/log2 lookup tables internally.
//-------------------------------------------------------------------------------
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
powfp(T base, T exp) {
    return T::pow(base, exp);
}

//-------------------------------------------------------------------------------
// expfp<T>(x) — fixed-point exponential: computes e^x.
// Uses powfp(e, x) where e is a fixed-point constant for Euler's number.
//-------------------------------------------------------------------------------
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
expfp(T x) {
    static const T e_val(static_cast<float>(FL_E));
    return powfp(e_val, x);
}

// exp(x) for fixed-point types — dispatches to expfp
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
exp(T x) { return fl::expfp(x); }

// ---- Cross-type operator implementations (after all types are fully defined) ----

// s0x32 × s16x16 → s16x16
constexpr FASTLED_FORCE_INLINE s16x16 s0x32::operator*(s16x16 b) const {
    return s16x16::from_raw(static_cast<i32>(
        (static_cast<i64>(mValue) * b.raw()) >> 31));
}

// s16x16 × s0x32 → s16x16 (commutative)
constexpr FASTLED_FORCE_INLINE s16x16 operator*(s16x16 a, s0x32 b) {
    return b * a;
}

} // namespace fl
