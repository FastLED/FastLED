#pragma once

#include "platforms/math8_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/stl/stdint.h"
#include "fl/sketch_macros.h"

namespace fl {

/// @file math8_avr.h
/// AVR-specific optimized assembly implementations of 8-bit math functions.
/// These implementations use the hardware MUL instruction (available on ATmega, not ATtiny).

/// @ingroup lib8tion
/// @{

/// @defgroup Math_AVR AVR Math Implementations
/// Fast AVR assembly implementations of 8-bit math operations (with hardware MUL)
/// @{

/// Add one byte to another, saturating at 0xFF (AVR assembly)
FL_ALWAYS_INLINE uint8_t qadd8(uint8_t i, uint8_t j) {
    asm volatile(
        /* First, add j to i, conditioning the C flag */
        "add %0, %1    \n\t"

        /* Now test the C flag.
        If C is clear, we branch around a set of 0xFF into i.
        If C is set, we go ahead and set 0xFF into i.
        */
        "brcc L_%=     \n\t"
        "ser %0        \n\t"
        "L_%=: "
        : "+r"(i)
        : "r"(j));
    return i;
}

/// Add one byte to another, saturating at 0x7F and -0x80 (AVR assembly)
FL_ALWAYS_INLINE int8_t qadd7(int8_t i, int8_t j) {
    asm volatile(
        /* First, add j to i, conditioning the V and C flags */
        "add %0, %1    \n\t"

        /* Now test the V flag.
        If V is clear, we branch to end.
        If V is set, we go ahead and set 0x7F into i.
        */
        "brvc L_%=     \n\t"
        "ser %0        \n\t"
        "lsr %0        \n\t"

        /* When both numbers are negative, C is set.
        Adding it to make result negative. */
        "adc %0, __zero_reg__\n\t"
        "L_%=: "
        : "+r"(i)
        : "r"(j));
    return i;
}

/// Subtract one byte from another, saturating at 0x00 (AVR assembly)
FL_ALWAYS_INLINE uint8_t qsub8(uint8_t i, uint8_t j) {
    asm volatile(
        /* First, subtract j from i, conditioning the C flag */
        "sub %0, %1    \n\t"

        /* Now test the C flag.
        If C is clear, we branch around a clear of i.
        If C is set, we go ahead and clear i to 0x00.
        */
        "brcc L_%=     \n\t"
        "clr %0        \n\t"
        "L_%=: "
        : "+r"(i)
        : "r"(j));
    return i;
}

/// Add one byte to another, with 8-bit result (AVR assembly)
FL_ALWAYS_INLINE uint8_t add8(uint8_t i, uint8_t j) {
    // Add j to i, period.
    asm volatile("add %0, %1" : "+r"(i) : "r"(j));
    return i;
}

/// Add one byte to two bytes, with 16-bit result (AVR assembly)
FL_ALWAYS_INLINE uint16_t add8to16(uint8_t i, uint16_t j) {
    // Add i(one byte) to j(two bytes)
    asm volatile("add %A[j], %[i]              \n\t"
                 "adc %B[j], __zero_reg__      \n\t"
                 : [j] "+r"(j)
                 : [i] "r"(i));
    return j;
}

/// Subtract one byte from another, 8-bit result (AVR assembly)
FL_ALWAYS_INLINE uint8_t sub8(uint8_t i, uint8_t j) {
    // Subtract j from i, period.
    asm volatile("sub %0, %1" : "+r"(i) : "r"(j));
    return i;
}

/// Calculate an integer average of two unsigned 8-bit values (AVR assembly)
FL_ALWAYS_INLINE uint8_t avg8(uint8_t i, uint8_t j) {
    asm volatile(
        /* First, add j to i, 9th bit overflows into C flag */
        "add %0, %1    \n\t"
        /* Divide by two, moving C flag into high 8th bit */
        "ror %0        \n\t"
        : "+r"(i)
        : "r"(j));
    return i;
}

/// Calculate an integer average of two unsigned 16-bit values (AVR assembly)
FL_ALWAYS_INLINE uint16_t avg16(uint16_t i, uint16_t j) {
    asm volatile(
        /* First, add jLo (heh) to iLo, 9th bit overflows into C flag */
        "add %A[i], %A[j]    \n\t"
        /* Now, add C + jHi to iHi, 17th bit overflows into C flag */
        "adc %B[i], %B[j]    \n\t"
        /* Divide iHi by two, moving C flag into high 16th bit, old 9th bit now
           in C */
        "ror %B[i]        \n\t"
        /* Divide iLo by two, moving C flag into high 8th bit */
        "ror %A[i]        \n\t"
        : [i] "+r"(i)
        : [j] "r"(j));
    return i;
}

/// Calculate an integer average of two unsigned 8-bit values, rounded up (AVR assembly)
FL_ALWAYS_INLINE uint8_t avg8r(uint8_t i, uint8_t j) {
    asm volatile(
        /* First, add j to i, 9th bit overflows into C flag */
        "add %0, %1          \n\t"
        /* Divide by two, moving C flag into high 8th bit, old 1st bit now in C */
        "ror %0              \n\t"
        /* Add C flag */
        "adc %0, __zero_reg__\n\t"
        : "+r"(i)
        : "r"(j));
    return i;
}

/// Calculate an integer average of two unsigned 16-bit values, rounded up (AVR assembly)
FL_ALWAYS_INLINE uint16_t avg16r(uint16_t i, uint16_t j) {
    asm volatile(
        /* First, add jLo (heh) to iLo, 9th bit overflows into C flag */
        "add %A[i], %A[j]    \n\t"
        /* Now, add C + jHi to iHi, 17th bit overflows into C flag */
        "adc %B[i], %B[j]    \n\t"
        /* Divide iHi by two, moving C flag into high 16th bit, old 9th bit now
           in C */
        "ror %B[i]        \n\t"
        /* Divide iLo by two, moving C flag into high 8th bit, old 1st bit now
           in C */
        "ror %A[i]        \n\t"
        /* Add C flag */
        "adc %A[i], __zero_reg__\n\t"
        "adc %B[i], __zero_reg__\n\t"
        : [i] "+r"(i)
        : [j] "r"(j));
    return i;
}

/// Calculate an integer average of two signed 7-bit integers (AVR assembly)
FL_ALWAYS_INLINE int8_t avg7(int8_t i, int8_t j) {
    asm volatile("asr %1        \n\t"
                 "asr %0        \n\t"
                 "adc %0, %1    \n\t"
                 : "+r"(i)
                 : "r"(j));
    return i;
}

/// Calculate an integer average of two signed 15-bit integers (AVR assembly)
FL_ALWAYS_INLINE int16_t avg15(int16_t i, int16_t j) {
    asm volatile(
        /* first divide j by 2, throwing away lowest bit */
        "asr %B[j]          \n\t"
        "ror %A[j]          \n\t"
        /* now divide i by 2, with lowest bit going into C */
        "asr %B[i]          \n\t"
        "ror %A[i]          \n\t"
        /* add j + C to i */
        "adc %A[i], %A[j]   \n\t"
        "adc %B[i], %B[j]   \n\t"
        : [i] "+r"(i)
        : [j] "r"(j));
    return i;
}

/// Take the absolute value of a signed 8-bit int8_t (AVR assembly)
FL_ALWAYS_INLINE int8_t abs8(int8_t i) {
    asm volatile(
        /* First, check the high bit, and prepare to skip if it's clear */
        "sbrc %0, 7 \n"

        /* Negate the value */
        "neg %0     \n"

        : "+r"(i)
        : "r"(i));
    return i;
}

/// Calculate the remainder of one unsigned 8-bit
/// value divided by another, aka A % M. (AVR assembly)
/// Implemented by repeated subtraction, which is
/// very compact, and very fast if A is "probably"
/// less than M.  If A is a large multiple of M,
/// the loop has to execute multiple times.  However,
/// even in that case, the loop is only two
/// instructions long on AVR, i.e., quick.
/// @param a dividend byte
/// @param m divisor byte
/// @returns remainder of a / m (i.e. a % m)
FL_ALWAYS_INLINE uint8_t mod8(uint8_t a, uint8_t m) {
    asm volatile("L_%=:  sub %[a],%[m]    \n\t"
                 "       brcc L_%=        \n\t"
                 "       add %[a],%[m]    \n\t"
                 : [a] "+r"(a)
                 : [m] "r"(m));
    return a;
}

/// Add two numbers, and calculate the modulo
/// of the sum and a third number, M. (AVR assembly)
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
    asm volatile("       add %[a],%[b]    \n\t"
                 "L_%=:  sub %[a],%[m]    \n\t"
                 "       brcc L_%=        \n\t"
                 "       add %[a],%[m]    \n\t"
                 : [a] "+r"(a)
                 : [b] "r"(b), [m] "r"(m));
    return a;
}

/// Subtract two numbers, and calculate the modulo
/// of the difference and a third number, M. (AVR assembly)
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
    asm volatile("       sub %[a],%[b]    \n\t"
                 "L_%=:  sub %[a],%[m]    \n\t"
                 "       brcc L_%=        \n\t"
                 "       add %[a],%[m]    \n\t"
                 : [a] "+r"(a)
                 : [b] "r"(b), [m] "r"(m));
    return a;
}

/// 8x8 bit multiplication, with 8-bit result (AVR assembly with MUL)
/// Uses the hardware MUL instruction (2 cycle latency)
/// ~10 cycles total
FL_ALWAYS_INLINE uint8_t mul8(uint8_t i, uint8_t j) {
    asm volatile(
        /* Multiply 8-bit i * 8-bit j, giving 16-bit r1,r0 */
        "mul %0, %1          \n\t"
        /* Extract the LOW 8-bits (r0) */
        "mov %0, r0          \n\t"
        /* Restore r1 to "0"; it's expected to always be that */
        "clr __zero_reg__    \n\t"
        : "+r"(i)
        : "r"(j)
        : "r0", "r1");
    return i;
}

/// 8x8 bit multiplication with 8-bit result, saturating at 0xFF (AVR assembly with MUL)
/// Uses hardware MUL with high-byte test for saturation detection
/// ~15 cycles
FL_ALWAYS_INLINE uint8_t qmul8(uint8_t i, uint8_t j) {
    asm volatile(
        /* Multiply 8-bit i * 8-bit j, giving 16-bit r1,r0 */
        "  mul %0, %1          \n\t"
        /* Extract the LOW 8-bits (r0) */
        "  mov %0, r0          \n\t"
        /* If high byte of result is zero, all is well. */
        "  tst r1              \n\t"
        "  breq Lnospill_%=    \n\t"
        /* If high byte of result > 0, saturate to 0xFF */
        "  ser %0              \n\t"
        "Lnospill_%=:          \n\t"
        /* Restore r1 to "0"; it's expected to always be that */
        "  clr __zero_reg__    \n\t"
        : "+r"(i)
        : "r"(j)
        : "r0", "r1");
    return i;
}

/// Blend a variable proportion of one byte to another - 8-bit precision (AVR assembly with MUL)
/// Uses Option 1: result = ((a << 8) + (b - a) * M + 0x80) >> 8
/// Computes with proper rounding for accurate color interpolation
LIB8STATIC uint8_t blend8_8bit(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial = 0;
    uint8_t result;

    // Use inline assembly to construct 16-bit value and perform blending
    uint8_t temp_reg;  // Temporary register for loading 0x80
    asm volatile(
        "  mov %A[partial], %[b]           \n\t"  // Low byte = b
        "  mov %B[partial], %[a]           \n\t"  // High byte = a (partial = a*256 + b)
        "  mul %[a], %[amountOfB]          \n\t"  // r1:r0 = a * amountOfB
        "  sub %A[partial], r0             \n\t"  // partial -= a * amountOfB (low byte)
        "  sbc %B[partial], r1             \n\t"  // partial -= a * amountOfB (high byte)
        "  mul %[b], %[amountOfB]          \n\t"  // r1:r0 = b * amountOfB
        "  add %A[partial], r0             \n\t"  // partial += b * amountOfB (low byte)
        "  adc %B[partial], r1             \n\t"  // partial += b * amountOfB (high byte)
        "  ldi %[temp], 0x80               \n\t"  // Load 128 for rounding (into upper register)
        "  add %A[partial], %[temp]        \n\t"  // partial += 0x80 (low byte)
        "  adc %B[partial], __zero_reg__   \n\t"  // partial += carry (high byte)
        "  clr __zero_reg__                \n\t"  // Restore zero register
        : [partial] "+r"(partial), [temp] "=d"(temp_reg)
        : [amountOfB] "r"(amountOfB), [a] "r"(a), [b] "r"(b)
        : "r0", "r1");

    result = partial >> 8;

    return result;
}

/// Blend a variable proportion of one byte to another - 16-bit precision (AVR C implementation)
/// Uses Option 2: result = ((a << 16) + (b - a) * M * 257 + 0x8000) >> 16
/// Falls back to C implementation for AVR as 32-bit assembly would be complex
LIB8STATIC uint8_t blend8_16bit(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint32_t partial;
    int16_t delta = (int16_t)b - (int16_t)a;

    // Calculate: (a * 65536 + (b - a) * amountOfB * 257 + 32768) / 65536
    partial = ((uint32_t)a << 16);
    partial += (uint32_t)delta * amountOfB * 257;
    partial += 0x8000;

    return partial >> 16;
}

/// Blend a variable proportion of one byte to another (AVR assembly with MUL)
/// Automatically selects between 8-bit and 16-bit precision based on available memory
#if (SKETCH_HAS_LOTS_OF_MEMORY)
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    return blend8_16bit(a, b, amountOfB);
}
#else
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    return blend8_8bit(a, b, amountOfB);
}
#endif

/// @} Math_AVR

/// @} lib8tion

}  // namespace fl
