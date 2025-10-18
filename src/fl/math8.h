#pragma once

#include "lib8tion/config.h"
#include "lib8tion/scale8.h"
#include "lib8tion/lib8static.h"
#include "lib8tion/intmap.h"
#include "fl/namespace.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION

FASTLED_NAMESPACE_BEGIN

/// @file math8.h
/// Fast, efficient 8-bit math functions specifically
/// designed for high-performance LED programming.

/// @ingroup lib8tion
/// @{

/// @defgroup Math Basic Math Operations
/// Fast, efficient 8-bit math functions specifically
/// designed for high-performance LED programming.
///
/// Because of the AVR (Arduino) and ARM assembly language
/// implementations provided, using these functions often
/// results in smaller and faster code than the equivalent
/// program using plain "C" arithmetic and logic.
/// @{

// Select appropriate implementation based on platform configuration
#if defined(__AVR__)
#include "platforms/avr/math8.h"
#elif defined(FASTLED_TEENSY3)
#include "platforms/arm/math8.h"
#else
#include "platforms/shared/math8.h"
#endif

/// Calculate the remainder of one unsigned 8-bit
/// value divided by anoter, aka A % M.
/// Implemented by repeated subtraction, which is
/// very compact, and very fast if A is "probably"
/// less than M.  If A is a large multiple of M,
/// the loop has to execute multiple times.  However,
/// even in that case, the loop is only two
/// instructions long on AVR, i.e., quick.
/// @param a dividend byte
/// @param m divisor byte
/// @returns remainder of a / m (i.e. a % m)
LIB8STATIC_ALWAYS_INLINE uint8_t mod8(uint8_t a, uint8_t m) {
#if defined(__AVR__)
    asm volatile("L_%=:  sub %[a],%[m]    \n\t"
                 "       brcc L_%=        \n\t"
                 "       add %[a],%[m]    \n\t"
                 : [a] "+r"(a)
                 : [m] "r"(m));
#else
    while (a >= m)
        a -= m;
#endif
    return a;
}

/// Add two numbers, and calculate the modulo
/// of the sum and a third number, M.
/// In other words, it returns (A+B) % M.
/// It is designed as a compact mechanism for
/// incrementing a "mode" switch and wrapping
/// around back to "mode 0" when the switch
/// goes past the end of the available range.
/// e.g. if you have seven modes, this switches
/// to the next one and wraps around if needed:
///   @code{.cpp}
///   mode = addmod8( mode, 1, 7);
///   @endcode
/// @param a dividend byte
/// @param b value to add to the dividend
/// @param m divisor byte
/// @returns remainder of (a + b) / m
/// @see mod8() for notes on performance.
LIB8STATIC uint8_t addmod8(uint8_t a, uint8_t b, uint8_t m) {
#if defined(__AVR__)
    asm volatile("       add %[a],%[b]    \n\t"
                 "L_%=:  sub %[a],%[m]    \n\t"
                 "       brcc L_%=        \n\t"
                 "       add %[a],%[m]    \n\t"
                 : [a] "+r"(a)
                 : [b] "r"(b), [m] "r"(m));
#else
    a += b;
    while (a >= m)
        a -= m;
#endif
    return a;
}

/// Subtract two numbers, and calculate the modulo
/// of the difference and a third number, M.
/// In other words, it returns (A-B) % M.
/// It is designed as a compact mechanism for
/// decrementing a "mode" switch and wrapping
/// around back to "mode 0" when the switch
/// goes past the start of the available range.
/// e.g. if you have seven modes, this switches
/// to the previous one and wraps around if needed:
///   @code{.cpp}
///   mode = submod8( mode, 1, 7);
///   @endcode
/// @param a dividend byte
/// @param b value to subtract from the dividend
/// @param m divisor byte
/// @returns remainder of (a - b) / m
/// @see mod8() for notes on performance.
LIB8STATIC uint8_t submod8(uint8_t a, uint8_t b, uint8_t m) {
#if defined(__AVR__)
    asm volatile("       sub %[a],%[b]    \n\t"
                 "L_%=:  sub %[a],%[m]    \n\t"
                 "       brcc L_%=        \n\t"
                 "       add %[a],%[m]    \n\t"
                 : [a] "+r"(a)
                 : [b] "r"(b), [m] "r"(m));
#else
    a -= b;
    while (a >= m)
        a -= m;
#endif
    return a;
}

/// Square root for 16-bit integers.
/// About three times faster and five times smaller
/// than Arduino's general `sqrt` on AVR.
LIB8STATIC uint8_t sqrt16(uint16_t x) {
    if (x <= 1) {
        return x;
    }

    uint8_t low = 1; // lower bound
    uint8_t hi, mid;

    if (x > 7904) {
        hi = 255;
    } else {
        hi = (x >> 5) + 8; // initial estimate for upper bound
    }

    do {
        mid = (low + hi) >> 1;
        if ((uint16_t)(mid * mid) > x) {
            hi = mid - 1;
        } else {
            if (mid == 255) {
                return 255;
            }
            low = mid + 1;
        }
    } while (hi >= low);

    return low - 1;
}

LIB8STATIC_ALWAYS_INLINE uint8_t sqrt8(uint8_t x) {
    return sqrt16(map8_to_16(x));
}

/// @} Math
/// @} lib8tion

FASTLED_NAMESPACE_END

FL_DISABLE_WARNING_POP
