#pragma once

#include "fl/stdint.h"
#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN


/// @addtogroup FractionalTypes
/// @{

/// Template class for representing fractional ints.
/// @tparam T underlying type for data storage
/// @tparam F number of fractional bits
/// @tparam I number of integer bits
template<class T, int F, int I> class qfx {
    T i:I;  ///< Integer value of number
    T f:F;  ///< Fractional value of number
public:
    /// Constructor, storing a float as a fractional int
    qfx(float fx) { i = fx; f = (fx-i) * (1<<F); }
    /// Constructor, storing a fractional int directly
    qfx(uint8_t _i, uint8_t _f) {i=_i; f=_f; }

    /// Multiply the fractional int by a value
    uint32_t operator*(uint32_t v) { return (v*i) + ((v*f)>>F); }
    /// @copydoc operator*(uint32_t)
    uint16_t operator*(uint16_t v) { return (v*i) + ((v*f)>>F); }
    /// @copydoc operator*(uint32_t)
    int32_t operator*(int32_t v) { return (v*i) + ((v*f)>>F); }
    /// @copydoc operator*(uint32_t)
    int16_t operator*(int16_t v) { return (v*i) + ((v*f)>>F); }
#if defined(FASTLED_ARM) | defined(FASTLED_RISCV) | defined(FASTLED_APOLLO3)
    /// @copydoc operator*(uint32_t)
    int operator*(int v) { return (v*i) + ((v*f)>>F); }
#endif
};

template<class T, int F, int I> static uint32_t operator*(uint32_t v, qfx<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static uint16_t operator*(uint16_t v, qfx<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static int32_t operator*(int32_t v, qfx<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static int16_t operator*(int16_t v, qfx<T,F,I> & q) { return q * v; }
#if defined(FASTLED_ARM) | defined(FASTLED_RISCV) | defined(FASTLED_APOLLO3)
template<class T, int F, int I> static int operator*(int v, qfx<T,F,I> & q) { return q * v; }
#endif

/// A 4.4 integer (4 bits integer, 4 bits fraction)
typedef qfx<uint8_t, 4,4> q44;
/// A 6.2 integer (6 bits integer, 2 bits fraction)
typedef qfx<uint8_t, 6,2> q62;
/// A 8.8 integer (8 bits integer, 8 bits fraction)
typedef qfx<uint16_t, 8,8> q88;
/// A 12.4 integer (12 bits integer, 4 bits fraction)
typedef qfx<uint16_t, 12,4> q124;

/// @} FractionalTypes

FASTLED_NAMESPACE_END
