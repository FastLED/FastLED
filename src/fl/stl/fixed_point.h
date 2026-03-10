#pragma once

// Template system for fixed-point types with sign support.
//
// Fixed-point numbers represent fractional values using integer arithmetic,
// splitting a 32-bit integer into an integer part and a fractional part.
// For example, s16x16 uses 16 bits for the integer part (range ~-32768 to 32767)
// and 16 bits for the fractional part (precision ~0.000015).
//
// Quick start:
//   using FP = fl::fixed_point<16, 16>;           // signed 16.16 (default)
//   FP x(3.14f);                                  // construct from float
//   FP y = FP::from_raw(0x00028000);              // construct from raw: 2.5
//   FP z = x + y;                                 // arithmetic: 5.64
//   float f = z.to_float();                       // convert back: 5.64
//   int i = z.to_int();                           // truncate to int: 5
//
// Available types (IntBits.FracBits):
//   Signed:   s0x32, s4x12, s8x8, s8x24, s12x4, s16x16, s24x8
//   Unsigned: u0x32, u4x12, u8x8, u8x24, u12x4, u16x16, u24x8
//
// Template aliases:
//   fl::sfixed_integer<16, 16>  → signed 16.16
//   fl::ufixed_integer<8, 8>   → unsigned 8.8
//
// Free-function math API (std::cmath style):
//   fl::floor(x), fl::ceil(x), fl::fract(x), fl::abs(x)
//   fl::sqrt(x), fl::rsqrt(x), fl::pow(x, y), fl::exp(x)
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

// Sign selector for fixed-point types.
//   fl::fixed_point<16, 16>                  // defaults to Sign::SIGNED
//   fl::fixed_point<16, 16, Sign::UNSIGNED>  // explicit unsigned
enum class Sign {
    SIGNED,
    UNSIGNED
};

// Maps <IntBits, FracBits, Sign> to the concrete type (e.g. s16x16, u8x8).
// Only explicitly specialized combinations are valid; others produce a
// compile error via the undefined primary template.
//
// Example: fixed_point_impl<16, 16, Sign::SIGNED>::type → s16x16
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

// Type trait: true when fixed_point type From can be implicitly promoted to To.
// Promotion requires: both INT_BITS and FRAC_BITS of From <= To, same sign,
// and at least one dimension differs (no identity promotion).
//
// Example:
//   is_fp_promotable<fixed_point<8,8>, fixed_point<16,16>>::value  // true
//   is_fp_promotable<fixed_point<16,16>, fixed_point<8,8>>::value  // false (demotion)
//   is_fp_promotable<fixed_point<8,8>, fixed_point<8,8>>::value    // false (identity)
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

// Generic fixed-point wrapper that adds auto-promotion between types and
// forwards all operations to the underlying concrete type (e.g. s16x16).
//
// Example:
//   using FP = fl::fixed_point<16, 16>;    // signed 16.16
//   FP a(2.5f);
//   FP b(1.25f);
//   FP c = a * b;                          // 3.125
//   float f = c.to_float();                // 3.125f
//
// Auto-promotion:
//   fl::fixed_point<8, 8> small(1.5f);
//   fl::fixed_point<16, 16> big = small;   // implicit widening, no data loss
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
    // ---- Type constants ----
    // INT_BITS:  number of integer bits (e.g. 16 for s16x16)
    // FRAC_BITS: number of fractional bits (e.g. 16 for s16x16)
    //   static_assert(FP::INT_BITS == 16, "");
    using Base::INT_BITS;
    using Base::FRAC_BITS;

    // ---- Constructors ----

    // Default constructor — value is zero.
    //   FP x;  // x == 0
    FASTLED_FORCE_INLINE constexpr fixed_point() : Base() {}

    // Construct from a floating-point literal. Conversion is done at compile
    // time when used in a constexpr context.
    //   FP x(3.14f);   // x ≈ 3.14
    //   FP y(-0.5f);   // y = -0.5
    FASTLED_FORCE_INLINE explicit constexpr fixed_point(float f) : Base(f) {}

    // Construct from a raw integer value (used internally by from_raw).
    //   FP x(0x00028000, FP::Base::RawTag());  // prefer from_raw() instead
    constexpr explicit fixed_point(RawType raw, typename Base::RawTag tag) : Base(raw, tag) {}

    // Implicit widening promotion from a narrower fixed-point type.
    // Only enabled when both IntBits and FracBits of the source are <=
    // this type's, with the same sign. No data is lost.
    //   fixed_point<8, 8> narrow(1.5f);
    //   fixed_point<16, 16> wide = narrow;  // OK: implicit promotion
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

    // Construct from the underlying raw integer representation.
    // For s16x16 the value 0x00018000 represents 1.5 (0x0001 integer, 0x8000 = 0.5).
    //   auto x = FP::from_raw(0x00028000);  // 2.5 in 16.16 format
    static constexpr FASTLED_FORCE_INLINE fixed_point from_raw(RawType raw) {
        return fixed_point(raw, typename Base::RawTag());
    }

    // ---- Conversion methods ----

    // Returns the underlying raw integer representation.
    //   FP x(2.5f);
    //   i32 r = x.raw();  // 0x00028000 for s16x16
    FASTLED_FORCE_INLINE constexpr RawType raw() const { return Base::raw(); }

    // Truncates toward zero, returning only the integer part.
    //   FP(3.75f).to_int()   // 3
    //   FP(-2.25f).to_int()  // -2
    constexpr FASTLED_FORCE_INLINE i32 to_int() const { return Base::to_int(); }

    // Converts to a float. Useful for debugging or interop with float APIs.
    //   FP(3.14f).to_float()  // ≈ 3.14f
    constexpr FASTLED_FORCE_INLINE float to_float() const { return Base::to_float(); }

    // ---- Arithmetic operators ----
    // All arithmetic is integer-only in the hot path.
    //   FP a(3.0f), b(1.5f);
    //   FP sum  = a + b;   // 4.5
    //   FP diff = a - b;   // 1.5
    //   FP prod = a * b;   // 4.5
    //   FP quot = a / b;   // 2.0
    //   FP neg  = -a;      // -3.0

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

    // Unary negation.
    //   FP x(2.0f);
    //   FP y = -x;  // -2.0
    constexpr FASTLED_FORCE_INLINE fixed_point operator-() const {
        return from_raw((Base::operator-()).raw());
    }

    // Bit-shift operators — shift the raw representation.
    //   FP x(4.0f);
    //   FP half    = x >> 1;  // 2.0  (divide by 2)
    //   FP doubled = x << 1;  // 8.0  (multiply by 2)
    constexpr FASTLED_FORCE_INLINE fixed_point operator>>(int shift) const {
        return from_raw((Base::operator>>(shift)).raw());
    }

    constexpr FASTLED_FORCE_INLINE fixed_point operator<<(int shift) const {
        return from_raw((Base::operator<<(shift)).raw());
    }

    // ---- Scalar multiplication ----
    // Multiply a fixed-point value by a plain integer (both orderings).
    //   FP x(1.5f);
    //   FP a = x * 3;   // 4.5
    //   FP b = 3 * x;   // 4.5

    constexpr FASTLED_FORCE_INLINE fixed_point operator*(i32 scalar) const {
        return from_raw((Base::operator*(scalar)).raw());
    }

    friend constexpr fixed_point operator*(i32 scalar, fixed_point fp) {
        return from_raw((scalar * static_cast<Base>(fp)).raw());
    }

    // ---- Comparison operators ----
    // All six relational operators. Comparisons are on the raw integer.
    //   FP a(1.0f), b(2.0f);
    //   a < b;   // true
    //   a == a;  // true
    //   a != b;  // true

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
    // These are also available as free functions: fl::floor(x), fl::mod(a,b), etc.

    // Remainder after division: mod(5.5, 2.0) → 1.5
    static constexpr FASTLED_FORCE_INLINE fixed_point mod(fixed_point a, fixed_point b) {
        return from_raw(Base::mod(a, b).raw());
    }

    // Round down to the nearest integer: floor(3.7) → 3.0, floor(-1.2) → -2.0
    static constexpr FASTLED_FORCE_INLINE fixed_point floor(fixed_point x) {
        return from_raw(Base::floor(x).raw());
    }

    // Round up to the nearest integer: ceil(3.2) → 4.0, ceil(-1.8) → -1.0
    static constexpr FASTLED_FORCE_INLINE fixed_point ceil(fixed_point x) {
        return from_raw(Base::ceil(x).raw());
    }

    // Fractional part: fract(3.75) → 0.75, fract(-1.25) → 0.75
    static constexpr FASTLED_FORCE_INLINE fixed_point fract(fixed_point x) {
        return from_raw(Base::fract(x).raw());
    }

    // Absolute value: abs(-2.5) → 2.5
    static constexpr FASTLED_FORCE_INLINE fixed_point abs(fixed_point x) {
        return from_raw(Base::abs(x).raw());
    }

    // Sign function: returns -1, 0, or +1.
    //   sign(-3.5) → -1.0, sign(0) → 0.0, sign(2.0) → 1.0
    static constexpr FASTLED_FORCE_INLINE fixed_point sign(fixed_point x) {
        return from_raw(Base::sign(x).raw());
    }

    // Linear interpolation: lerp(a, b, t) = a + t*(b - a), where t is in [0, 1].
    //   lerp(0, 10, 0.5) → 5.0
    //   lerp(0, 10, 0.25) → 2.5
    static constexpr FASTLED_FORCE_INLINE fixed_point lerp(fixed_point a, fixed_point b, fixed_point t) {
        return from_raw(Base::lerp(a, b, t).raw());
    }

    // Clamp x to the range [lo, hi]: clamp(5.0, 0.0, 3.0) → 3.0
    static constexpr FASTLED_FORCE_INLINE fixed_point clamp(fixed_point x, fixed_point lo, fixed_point hi) {
        return from_raw(Base::clamp(x, lo, hi).raw());
    }

    // Step function: returns 0.0 if x < edge, 1.0 otherwise.
    //   step(0.5, 0.3) → 0.0
    //   step(0.5, 0.7) → 1.0
    static constexpr FASTLED_FORCE_INLINE fixed_point step(fixed_point edge, fixed_point x) {
        return from_raw(Base::step(edge, x).raw());
    }

    // Smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1.
    //   smoothstep(0.0, 1.0, 0.5) ≈ 0.5
    //   smoothstep(0.0, 1.0, 0.0) → 0.0
    //   smoothstep(0.0, 1.0, 1.0) → 1.0
    static FASTLED_FORCE_INLINE fixed_point smoothstep(fixed_point edge0, fixed_point edge1, fixed_point x) {
        return from_raw(Base::smoothstep(edge0, edge1, x).raw());
    }

    // Component-wise minimum: min(3.0, 1.5) → 1.5
    static constexpr FASTLED_FORCE_INLINE fixed_point min(fixed_point a, fixed_point b) {
        return from_raw(Base::min(a, b).raw());
    }

    // Component-wise maximum: max(3.0, 1.5) → 3.0
    static constexpr FASTLED_FORCE_INLINE fixed_point max(fixed_point a, fixed_point b) {
        return from_raw(Base::max(a, b).raw());
    }

    // ---- Trigonometry ----
    // Angles are in radians (not degrees, not 0-65535 "brev" units).
    // All trig functions use integer-only LUT-based implementations.

    // Sine: sin(π/2) → 1.0
    //   FP angle(1.5708f);  // π/2
    //   FP s = FP::sin(angle);  // ≈ 1.0
    static FASTLED_FORCE_INLINE fixed_point sin(fixed_point angle) {
        return from_raw(Base::sin(angle).raw());
    }

    // Cosine: cos(0) → 1.0
    //   FP c = FP::cos(FP(0.0f));  // 1.0
    static FASTLED_FORCE_INLINE fixed_point cos(fixed_point angle) {
        return from_raw(Base::cos(angle).raw());
    }

    // Compute sin and cos simultaneously (more efficient than calling each).
    //   FP s, c;
    //   FP::sincos(FP(1.0f), s, c);  // s ≈ 0.841, c ≈ 0.540
    static FASTLED_FORCE_INLINE void sincos(fixed_point angle, fixed_point& out_sin, fixed_point& out_cos) {
        Base base_sin, base_cos;
        Base::sincos(angle, base_sin, base_cos);
        out_sin = from_raw(base_sin.raw());
        out_cos = from_raw(base_cos.raw());
    }

    // Arctangent: atan(1.0) ≈ π/4 ≈ 0.785
    static FASTLED_FORCE_INLINE fixed_point atan(fixed_point x) {
        return from_raw(Base::atan(x).raw());
    }

    // Two-argument arctangent: atan2(y, x) returns angle in [-π, π].
    //   FP::atan2(FP(1.0f), FP(1.0f))  // ≈ π/4 ≈ 0.785
    static FASTLED_FORCE_INLINE fixed_point atan2(fixed_point y, fixed_point x) {
        return from_raw(Base::atan2(y, x).raw());
    }

    // Arc sine: asin(1.0) ≈ π/2. Input must be in [-1, 1].
    static FASTLED_FORCE_INLINE fixed_point asin(fixed_point x) {
        return from_raw(Base::asin(x).raw());
    }

    // Arc cosine: acos(0.0) ≈ π/2. Input must be in [-1, 1].
    static FASTLED_FORCE_INLINE fixed_point acos(fixed_point x) {
        return from_raw(Base::acos(x).raw());
    }

    // ---- Other math functions ----

    // Square root: sqrt(4.0) → 2.0, sqrt(2.0) ≈ 1.414
    //   FP r = FP::sqrt(FP(9.0f));  // 3.0
    static constexpr FASTLED_FORCE_INLINE fixed_point sqrt(fixed_point x) {
        return from_raw(Base::sqrt(x).raw());
    }

    // Reciprocal square root: rsqrt(4.0) → 0.5, rsqrt(x) ≈ 1/sqrt(x)
    //   FP r = FP::rsqrt(FP(4.0f));  // 0.5
    static constexpr FASTLED_FORCE_INLINE fixed_point rsqrt(fixed_point x) {
        return from_raw(Base::rsqrt(x).raw());
    }

    // Power: pow(2.0, 3.0) → 8.0. Uses exp2/log2 LUTs internally.
    //   FP r = FP::pow(FP(2.0f), FP(10.0f));  // 1024.0
    static FASTLED_FORCE_INLINE fixed_point pow(fixed_point base, fixed_point exp) {
        return from_raw(Base::pow(base, exp).raw());
    }
};

// Convenience aliases that default the sign, so you don't need to spell Sign::SIGNED.
//   fl::sfixed_integer<16, 16> x(3.14f);  // signed 16.16
//   fl::ufixed_integer<8, 8>  y(1.5f);    // unsigned 8.8
template <int IntBits, int FracBits>
using sfixed_integer = fixed_point<IntBits, FracBits, Sign::SIGNED>;

template <int IntBits, int FracBits>
using ufixed_integer = fixed_point<IntBits, FracBits, Sign::UNSIGNED>;

// Backward-compatible aliases (prefer sfixed_integer / ufixed_integer).
template <int IntBits, int FracBits>
using sfixed_point = sfixed_integer<IntBits, FracBits>;

template <int IntBits, int FracBits>
using ufixed_point = ufixed_integer<IntBits, FracBits>;

//-------------------------------------------------------------------------------
// is_fixed_point<T> — type trait that is true_type for any fixed_point<I,F,S>.
// Strips const/volatile/reference qualifiers automatically.
// (Default false_type lives in fl/stl/type_traits.h)
//
// Example:
//   is_fixed_point<fl::sfixed_integer<16,16>>::value   // true
//   is_fixed_point<float>::value                       // false
//   is_fixed_point<const fl::s16x16&>::value           // true
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
//
// These enable ADL so you can write fl::sin(x) instead of x.sin(x).
// Only enabled for fixed_point types (SFINAE via is_fixed_point).
//
// Usage:
//   using FP = fl::fixed_point<16, 16>;
//   FP x(2.25f);
//   FP f = fl::floor(x);         // 2.0
//   FP c = fl::ceil(x);          // 3.0
//   FP s = fl::sin(FP(1.0f));    // ≈ 0.841
//   FP r = fl::sqrt(FP(9.0f));   // 3.0
//-------------------------------------------------------------------------------

// ---- Rounding / decomposition ----

// Round down: fl::floor(FP(2.7f)) → 2.0
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
floor(T x) { return T::floor(x); }

// Round up: fl::ceil(FP(2.1f)) → 3.0
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
ceil(T x) { return T::ceil(x); }

// Fractional part: fl::fract(FP(2.75f)) → 0.75
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
fract(T x) { return T::fract(x); }

// Remainder: fl::mod(FP(5.5f), FP(2.0f)) → 1.5
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
mod(T a, T b) { return T::mod(a, b); }

// ---- Absolute value / sign ----

// Absolute value: fl::abs(FP(-3.0f)) → 3.0
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
abs(T x) { return T::abs(x); }

// Sign: fl::sign(FP(-5.0f)) → -1
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, int>::type
sign(T x) { return T::sign(x); }

// ---- Interpolation / clamping ----

// Linear interpolation: fl::lerp(FP(0.0f), FP(10.0f), FP(0.5f)) → 5.0
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
lerp(T a, T b, T t) { return T::lerp(a, b, t); }

// Clamp to range: fl::clamp(FP(5.0f), FP(0.0f), FP(3.0f)) → 3.0
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
clamp(T x, T lo, T hi) { return T::clamp(x, lo, hi); }

// Step: fl::step(FP(0.5f), FP(0.7f)) → 1.0
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
step(T edge, T x) { return T::step(edge, x); }

// Smooth Hermite interpolation: fl::smoothstep(FP(0.0f), FP(1.0f), FP(0.5f)) ≈ 0.5
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
smoothstep(T edge0, T edge1, T x) { return T::smoothstep(edge0, edge1, x); }

// ---- Roots / powers ----

// Square root: fl::sqrt(FP(9.0f)) → 3.0
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
sqrt(T x) { return T::sqrt(x); }

// Reciprocal square root: fl::rsqrt(FP(4.0f)) → 0.5
template <typename T>
inline constexpr typename enable_if<is_fixed_point<T>::value, T>::type
rsqrt(T x) { return T::rsqrt(x); }

// Power: fl::pow(FP(2.0f), FP(3.0f)) → 8.0
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
pow(T base, T exp) { return T::pow(base, exp); }

// ---- Trigonometry ----
// All angles are in radians.

// Sine: fl::sin(FP(1.5708f)) ≈ 1.0  (π/2)
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
sin(T angle) { return T::sin(angle); }

// Cosine: fl::cos(FP(0.0f)) → 1.0
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
cos(T angle) { return T::cos(angle); }

// Simultaneous sin+cos (faster than calling each separately).
//   FP s, c;
//   fl::sincos(FP(1.0f), s, c);
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, void>::type
sincos(T angle, T& out_sin, T& out_cos) { T::sincos(angle, out_sin, out_cos); }

// Arctangent: fl::atan(FP(1.0f)) ≈ π/4 ≈ 0.785
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
atan(T x) { return T::atan(x); }

// Two-argument arctangent: fl::atan2(FP(1.0f), FP(1.0f)) ≈ π/4
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
atan2(T y, T x) { return T::atan2(y, x); }

// Arc sine: fl::asin(FP(1.0f)) ≈ π/2. Input in [-1, 1].
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
asin(T x) { return T::asin(x); }

// Arc cosine: fl::acos(FP(0.0f)) ≈ π/2. Input in [-1, 1].
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
acos(T x) { return T::acos(x); }

//-------------------------------------------------------------------------------
// powfp<T>(base, exp) — free-function power for fixed_point types.
// Identical to fl::pow() but with a distinct name to avoid ambiguity with
// C stdlib pow when both fl:: and std:: are in scope.
//
//   fl::powfp(FP(2.0f), FP(10.0f))  // 1024.0
//-------------------------------------------------------------------------------
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
powfp(T base, T exp) {
    return T::pow(base, exp);
}

//-------------------------------------------------------------------------------
// expfp<T>(x) — fixed-point exponential: computes e^x.
// Uses powfp(e, x) where e ≈ 2.71828.
//
//   fl::expfp(FP(1.0f))  // ≈ 2.718  (e^1)
//   fl::expfp(FP(0.0f))  // ≈ 1.0    (e^0)
//-------------------------------------------------------------------------------
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
expfp(T x) {
    static const T e_val(static_cast<float>(FL_E));
    return powfp(e_val, x);
}

// exp(x) for fixed-point types — equivalent to expfp(x).
//   fl::exp(FP(1.0f))  // ≈ 2.718
template <typename T>
inline typename enable_if<is_fixed_point<T>::value, T>::type
exp(T x) { return fl::expfp(x); }

// ---- Cross-type operator implementations ----
// Mixed-type multiplication between different fixed-point widths.
// The result type is the wider type to avoid precision loss.

// s0x32 × s16x16 → s16x16
// Scales a fractional-only value (s0x32 range: [-1, 1)) by a 16.16 value.
//   s0x32 frac(0.5f);
//   s16x16 val(10.0f);
//   s16x16 result = frac * val;  // 5.0
constexpr FASTLED_FORCE_INLINE s16x16 s0x32::operator*(s16x16 b) const {
    return s16x16::from_raw(static_cast<i32>(
        (static_cast<i64>(mValue) * b.raw()) >> 31));
}

// s16x16 × s0x32 → s16x16 (commutative)
//   s16x16 result = val * frac;  // same as frac * val
constexpr FASTLED_FORCE_INLINE s16x16 operator*(s16x16 a, s0x32 b) {
    return b * a;
}

} // namespace fl
