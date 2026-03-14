#pragma once

/// @file fl/gfx/alpha.h
/// @brief Unsigned alpha types with UNORM semantics (GPU industry standard).

#include "fl/stl/type_traits.h"

namespace fl {

namespace alpha_detail {

// True when T is a non-bool integer type (signed or unsigned, any width).
template <typename T>
struct is_non_bool_integer {
    static constexpr bool value =
        fl::is_integral<T>::value &&
        !fl::is_same<typename fl::remove_cv<T>::type, bool>::value;
};

// SFINAE helper — drop into a template parameter list:
//   template <typename IntT, alpha_detail::enable_if_integer_t<IntT> = 0>
template <typename T>
using enable_if_integer_t =
    typename fl::enable_if<is_non_bool_integer<T>::value, int>::type;

} // namespace alpha_detail

/// Unsigned 8-bit alpha / brightness — UNORM8.
/// Represents values in [0.0, 1.0] inclusive.
///
/// Interpretation: raw / 255
///   raw 0   = 0.0
///   raw 128 = 128/255 ~ 0.502
///   raw 255 = 255/255 = 1.0
///
/// FastLED's scale8() uses the (scale+1)>>8 approximation for
/// efficient UNORM scaling: scale8(x, 255) == x (exact identity).
///
/// Implicit conversion to/from unsigned char preserves full backward
/// compatibility with code that treats this as a plain u8.
struct alpha8 {
    unsigned char value;

    constexpr alpha8() : value(0) {}

    // Accept any non-bool integer type (signed or unsigned, any width).
    template <typename IntT, alpha_detail::enable_if_integer_t<IntT> = 0>
    constexpr alpha8(IntT v) : value(static_cast<unsigned char>(v)) {}  // NOLINT — implicit by design

    explicit constexpr alpha8(float f) : value(_clamp(f)) {}
    explicit constexpr alpha8(double f) : value(_clamp(static_cast<float>(f))) {}
    constexpr operator unsigned char() const { return value; }

    constexpr unsigned char raw() const { return value; }
    constexpr float to_float() const { return value / 255.0f; }

    static constexpr alpha8 from_float(float f) {
        return alpha8(static_cast<unsigned char>(_clamp(f)));
    }

    alpha8& operator+=(unsigned char rhs) { value += rhs; return *this; }
    alpha8& operator-=(unsigned char rhs) { value -= rhs; return *this; }
    alpha8& operator*=(unsigned char rhs) { value *= rhs; return *this; }
    alpha8& operator/=(unsigned char rhs) { value /= rhs; return *this; }
    alpha8& operator>>=(int rhs) { value >>= rhs; return *this; }
    alpha8& operator<<=(int rhs) { value <<= rhs; return *this; }
    alpha8& operator++() { ++value; return *this; }
    alpha8 operator++(int) { alpha8 t = *this; ++value; return t; }
    alpha8& operator--() { --value; return *this; }
    alpha8 operator--(int) { alpha8 t = *this; --value; return t; }

  private:
    static constexpr unsigned char _clamp(float f) {
        return static_cast<unsigned char>(
            f <= 0.0f ? 0 : (f >= 1.0f ? 255 : static_cast<unsigned char>(f * 255.0f + 0.5f)));
    }
};

/// Unsigned 16-bit alpha / brightness — UNORM16.
/// Represents values in [0.0, 1.0] inclusive.
///
/// Interpretation: raw / 65535
///   raw 0     = 0.0
///   raw 32768 = 32768/65535 ~ 0.500
///   raw 65535 = 65535/65535 = 1.0
struct alpha16 {
    unsigned short value;

    constexpr alpha16() : value(0) {}

    // Accept any non-bool integer type (signed or unsigned, any width).
    template <typename IntT, alpha_detail::enable_if_integer_t<IntT> = 0>
    constexpr alpha16(IntT v) : value(static_cast<unsigned short>(v)) {}  // NOLINT — implicit by design

    explicit constexpr alpha16(float f) : value(_clamp(f)) {}
    explicit constexpr alpha16(double f) : value(_clamp(static_cast<float>(f))) {}
    constexpr operator unsigned short() const { return value; }

    constexpr unsigned short raw() const { return value; }
    constexpr float to_float() const { return value / 65535.0f; }

    static constexpr alpha16 from_float(float f) {
        return alpha16(static_cast<unsigned short>(_clamp(f)));
    }

    alpha16& operator+=(unsigned short rhs) { value += rhs; return *this; }
    alpha16& operator-=(unsigned short rhs) { value -= rhs; return *this; }
    alpha16& operator*=(unsigned short rhs) { value *= rhs; return *this; }
    alpha16& operator/=(unsigned short rhs) { value /= rhs; return *this; }
    alpha16& operator>>=(int rhs) { value >>= rhs; return *this; }
    alpha16& operator<<=(int rhs) { value <<= rhs; return *this; }
    alpha16& operator++() { ++value; return *this; }
    alpha16 operator++(int) { alpha16 t = *this; ++value; return t; }
    alpha16& operator--() { --value; return *this; }
    alpha16 operator--(int) { alpha16 t = *this; --value; return t; }

  private:
    static constexpr unsigned short _clamp(float f) {
        return static_cast<unsigned short>(
            f <= 0.0f ? 0 : (f >= 1.0f ? 65535 : static_cast<unsigned short>(f * 65535.0f + 0.5f)));
    }
};

} // namespace fl
