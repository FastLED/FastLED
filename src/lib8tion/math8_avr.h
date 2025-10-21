#pragma once

#include "lib8tion/config.h"
#include "scale8.h"
#include "lib8tion/lib8static.h"
#include "intmap.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION

/// @file math8_avr.h
/// AVR assembly language implementations of 8-bit math functions.
/// This file contains optimized AVR assembly versions of functions from math8.h

/// @ingroup lib8tion
/// @{

/// @defgroup Math_AVR AVR Math Implementations
/// Fast AVR assembly implementations of 8-bit math operations
/// @{

/// Add one byte to another, saturating at 0xFF (AVR assembly)
LIB8STATIC_ALWAYS_INLINE uint8_t qadd8(uint8_t i, uint8_t j) {
    asm volatile(
        /* First, add j to i, conditioning the C flag */
        "add %0, %1    \n\t"

        /* Now test the C flag.
        If C is clear, we branch around a load of 0xFF into i.
        If C is set, we go ahead and load 0xFF into i.
        */
        "brcc L_%=     \n\t"
        "ldi %0, 0xFF  \n\t"
        "L_%=: "
        : "+d"(i) // r16-r31, restricted by ldi
        : "r"(j));
    return i;
}

/// Add one byte to another, saturating at 0x7F and -0x80 (AVR assembly)
LIB8STATIC_ALWAYS_INLINE int8_t qadd7(int8_t i, int8_t j) {
    asm volatile(
        /* First, add j to i, conditioning the V and C flags */
        "add %0, %1    \n\t"

        /* Now test the V flag.
        If V is clear, we branch to end.
        If V is set, we go ahead and load 0x7F into i.
        */
        "brvc L_%=     \n\t"
        "ldi %0, 0x7F  \n\t"

        /* When both numbers are negative, C is set.
        Adding it to make result negative. */
        "adc %0, __zero_reg__\n\t"
        "L_%=: "
        : "+d"(i) // r16-r31, restricted by ldi
        : "r"(j));
    return i;
}

/// Subtract one byte from another, saturating at 0x00 (AVR assembly)
LIB8STATIC_ALWAYS_INLINE uint8_t qsub8(uint8_t i, uint8_t j) {
    asm volatile(
        /* First, subtract j from i, conditioning the C flag */
        "sub %0, %1    \n\t"

        /* Now test the C flag.
        If C is clear, we branch around a load of 0x00 into i.
        If C is set, we go ahead and load 0x00 into i.
        */
        "brcc L_%=     \n\t"
        "ldi %0, 0x00  \n\t"
        "L_%=: "
        : "+d"(i) // r16-r31, restricted by ldi
        : "r"(j));
    return i;
}

/// Add one byte to another, with 8-bit result (AVR assembly)
LIB8STATIC_ALWAYS_INLINE uint8_t add8(uint8_t i, uint8_t j) {
    // Add j to i, period.
    asm volatile("add %0, %1" : "+r"(i) : "r"(j));
    return i;
}

/// Add one byte to two bytes, with 16-bit result (AVR assembly)
LIB8STATIC_ALWAYS_INLINE uint16_t add8to16(uint8_t i, uint16_t j) {
    // Add i(one byte) to j(two bytes)
    asm volatile("add %A[j], %[i]              \n\t"
                 "adc %B[j], __zero_reg__      \n\t"
                 : [j] "+r"(j)
                 : [i] "r"(i));
    return i;
}

/// Subtract one byte from another, 8-bit result (AVR assembly)
LIB8STATIC_ALWAYS_INLINE uint8_t sub8(uint8_t i, uint8_t j) {
    // Subtract j from i, period.
    asm volatile("sub %0, %1" : "+r"(i) : "r"(j));
    return i;
}

/// Calculate an integer average of two unsigned 8-bit values (AVR assembly)
LIB8STATIC_ALWAYS_INLINE uint8_t avg8(uint8_t i, uint8_t j) {
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
LIB8STATIC_ALWAYS_INLINE uint16_t avg16(uint16_t i, uint16_t j) {
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
LIB8STATIC_ALWAYS_INLINE uint8_t avg8r(uint8_t i, uint8_t j) {
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
LIB8STATIC_ALWAYS_INLINE uint16_t avg16r(uint16_t i, uint16_t j) {
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
LIB8STATIC_ALWAYS_INLINE int8_t avg7(int8_t i, int8_t j) {
    asm volatile("asr %1        \n\t"
                 "asr %0        \n\t"
                 "adc %0, %1    \n\t"
                 : "+r"(i)
                 : "r"(j));
    return i;
}

/// Calculate an integer average of two signed 15-bit integers (AVR assembly)
LIB8STATIC_ALWAYS_INLINE int16_t avg15(int16_t i, int16_t j) {
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

/// 8x8 bit multiplication, with 8-bit result (AVR assembly)
LIB8STATIC_ALWAYS_INLINE uint8_t mul8(uint8_t i, uint8_t j) {
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

/// 8x8 bit multiplication with 8-bit result, saturating at 0xFF (AVR assembly)
LIB8STATIC_ALWAYS_INLINE uint8_t qmul8(uint8_t i, uint8_t j) {
    asm volatile(
        /* Multiply 8-bit i * 8-bit j, giving 16-bit r1,r0 */
        "  mul %0, %1          \n\t"
        /* Extract the LOW 8-bits (r0) */
        "  mov %0, r0          \n\t"
        /* If high byte of result is zero, all is well. */
        "  tst r1              \n\t"
        "  breq Lnospill_%=    \n\t"
        /* If high byte of result > 0, saturate to 0xFF */
        "  ldi %0, 0xFF         \n\t"
        "Lnospill_%=:          \n\t"
        /* Restore r1 to "0"; it's expected to always be that */
        "  clr __zero_reg__    \n\t"
        : "+d"(i) // r16-r31, restricted by ldi
        : "r"(j)
        : "r0", "r1");
    return i;
}

/// Take the absolute value of a signed 8-bit int8_t (AVR assembly)
LIB8STATIC_ALWAYS_INLINE int8_t abs8(int8_t i) {
    asm volatile(
        /* First, check the high bit, and prepare to skip if it's clear */
        "sbrc %0, 7 \n"

        /* Negate the value */
        "neg %0     \n"

        : "+r"(i)
        : "r"(i));
    return i;
}

/// Blend a variable proportion of one byte to another (AVR assembly)
#if (FASTLED_BLEND_FIXED == 1)
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial = 0;
    uint8_t result;

    // Use inline assembly to construct 16-bit value and perform blending
    // This avoids generating illegal 32-bit 'or' instruction on ATtiny85
    asm volatile(
        "  mov %A[partial], %[b]           \n\t"  // Low byte = b
        "  mov %B[partial], %[a]           \n\t"  // High byte = a
        "  mul %[a], %[amountOfB]          \n\t"
        "  sub %A[partial], r0             \n\t"
        "  sbc %B[partial], r1             \n\t"
        "  mul %[b], %[amountOfB]          \n\t"
        "  add %A[partial], r0             \n\t"
        "  adc %B[partial], r1             \n\t"
        "  clr __zero_reg__                \n\t"
        : [partial] "+r"(partial)
        : [amountOfB] "r"(amountOfB), [a] "r"(a), [b] "r"(b)
        : "r0", "r1");

    result = partial >> 8;

    return result;
}
#else
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial = 0;
    uint8_t result;

    // non-SCALE8-fixed version
    // Use inline assembly to avoid illegal 32-bit instructions on ATtiny85

    asm volatile(
        /* partial = b * amountOfB */
        "  mul %[b], %[amountOfB]        \n\t"
        "  movw %A[partial], r0          \n\t"

        /* amountOfB (aka amountOfA) = 255 - amountOfB */
        "  com %[amountOfB]              \n\t"

        /* partial += a * amountOfB (aka amountOfA) */
        "  mul %[a], %[amountOfB]        \n\t"

        "  add %A[partial], r0           \n\t"
        "  adc %B[partial], r1           \n\t"

        "  clr __zero_reg__              \n\t"

        : [partial] "+r"(partial), [amountOfB] "+r"(amountOfB)
        : [a] "r"(a), [b] "r"(b)
        : "r0", "r1");

    result = partial >> 8;

    return result;
}
#endif

/// @} Math_AVR
/// @} lib8tion

FL_DISABLE_WARNING_POP
