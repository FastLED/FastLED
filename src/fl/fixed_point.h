#pragma once

// Template alias for signed fixed-point types.
// Usage: fl::fixed_point<16, 16> resolves to fl::s16x16, etc.

#include "fl/fixed_point/fixed_point_traits.h"
#include "fl/fixed_point/fixed_point_base.h"
#include "fl/fixed_point/s4x12.h"
#include "fl/fixed_point/s8x8.h"
#include "fl/fixed_point/s8x24.h"
#include "fl/fixed_point/s12x4.h"
#include "fl/fixed_point/s16x16.h"
#include "fl/fixed_point/s24x8.h"

namespace fl {

// Template fixed-point class for main thread code (not ISR-safe).
// For ISR code, use concrete types (s16x16, s8x8, etc.) directly.
//
// All methods are inherited from fixed_point_base via CRTP.
// This class exists primarily to enable template-based testing.
template <int IntBits, int FracBits>
class fixed_point : public fixed_point_base<fixed_point<IntBits, FracBits>, IntBits, FracBits> {
  private:
    using Base = fixed_point_base<fixed_point<IntBits, FracBits>, IntBits, FracBits>;

  public:
    // Type aliases and constants
    using traits = typename Base::traits;
    using raw_type = typename Base::raw_type;
    using unsigned_raw_type = typename Base::unsigned_raw_type;
    using intermediate_type = typename Base::intermediate_type;
    using unsigned_intermediate_type = typename Base::unsigned_intermediate_type;
    using poly_intermediate_type = typename Base::poly_intermediate_type;
    using Base::INT_BITS;
    using Base::FRAC_BITS;

    // Access methods
    raw_type raw() const { return Base::raw(); }
    raw_type to_int() const { return Base::to_int(); }
    float to_float() const { return Base::to_float(); }

    // Static factory
    static FASTLED_FORCE_INLINE fixed_point from_raw(raw_type raw) {
        return Base::from_raw(raw);
    }

    // Math functions
    static FASTLED_FORCE_INLINE fixed_point mod(fixed_point a, fixed_point b) {
        return Base::mod(a, b);
    }

    static FASTLED_FORCE_INLINE fixed_point floor(fixed_point x) {
        return Base::floor(x);
    }

    static FASTLED_FORCE_INLINE fixed_point ceil(fixed_point x) {
        return Base::ceil(x);
    }

    static FASTLED_FORCE_INLINE fixed_point fract(fixed_point x) {
        return Base::fract(x);
    }

    static FASTLED_FORCE_INLINE fixed_point abs(fixed_point x) {
        return Base::abs(x);
    }

    static FASTLED_FORCE_INLINE int sign(fixed_point x) {
        return Base::sign(x);
    }

    static FASTLED_FORCE_INLINE fixed_point lerp(fixed_point a, fixed_point b, fixed_point t) {
        return Base::lerp(a, b, t);
    }

    static FASTLED_FORCE_INLINE fixed_point clamp(fixed_point x, fixed_point lo, fixed_point hi) {
        return Base::clamp(x, lo, hi);
    }

    static FASTLED_FORCE_INLINE fixed_point step(fixed_point edge, fixed_point x) {
        return Base::step(edge, x);
    }

    static FASTLED_FORCE_INLINE fixed_point smoothstep(fixed_point edge0, fixed_point edge1, fixed_point x) {
        return Base::smoothstep(edge0, edge1, x);
    }

    // Trigonometry
    static FASTLED_FORCE_INLINE fixed_point sin(fixed_point angle) {
        return Base::sin(angle);
    }

    static FASTLED_FORCE_INLINE fixed_point cos(fixed_point angle) {
        return Base::cos(angle);
    }

    static FASTLED_FORCE_INLINE void sincos(fixed_point angle, fixed_point &out_sin, fixed_point &out_cos) {
        Base::sincos(angle, out_sin, out_cos);
    }

    // Advanced math
    static FASTLED_FORCE_INLINE fixed_point sqrt(fixed_point x) {
        return Base::sqrt(x);
    }

    static FASTLED_FORCE_INLINE fixed_point rsqrt(fixed_point x) {
        return Base::rsqrt(x);
    }

    static FASTLED_FORCE_INLINE fixed_point pow(fixed_point base, fixed_point exp) {
        return Base::pow(base, exp);
    }

    // Inverse trigonometry
    static FASTLED_FORCE_INLINE fixed_point atan(fixed_point x) {
        return Base::atan(x);
    }

    static FASTLED_FORCE_INLINE fixed_point atan2(fixed_point y, fixed_point x) {
        return Base::atan2(y, x);
    }

    static FASTLED_FORCE_INLINE fixed_point asin(fixed_point x) {
        return Base::asin(x);
    }

    static FASTLED_FORCE_INLINE fixed_point acos(fixed_point x) {
        return Base::acos(x);
    }

    // Member function versions (operate on *this)
    FASTLED_FORCE_INLINE fixed_point floor() const {
        return Base::floor();
    }

    FASTLED_FORCE_INLINE fixed_point ceil() const {
        return Base::ceil();
    }

    FASTLED_FORCE_INLINE fixed_point fract() const {
        return Base::fract();
    }

    FASTLED_FORCE_INLINE fixed_point abs() const {
        return Base::abs();
    }

    FASTLED_FORCE_INLINE int sign() const {
        return Base::sign();
    }

    FASTLED_FORCE_INLINE fixed_point sin() const {
        return Base::sin();
    }

    FASTLED_FORCE_INLINE fixed_point cos() const {
        return Base::cos();
    }

    FASTLED_FORCE_INLINE fixed_point atan() const {
        return Base::atan();
    }

    FASTLED_FORCE_INLINE fixed_point asin() const {
        return Base::asin();
    }

    FASTLED_FORCE_INLINE fixed_point acos() const {
        return Base::acos();
    }

    FASTLED_FORCE_INLINE fixed_point sqrt() const {
        return Base::sqrt();
    }

    FASTLED_FORCE_INLINE fixed_point rsqrt() const {
        return Base::rsqrt();
    }

    // Constructors
    constexpr fixed_point() : Base() {}
    explicit constexpr fixed_point(float f) : Base(f) {}

    // Operators are automatically inherited (not name lookup dependent)
};

} // namespace fl
