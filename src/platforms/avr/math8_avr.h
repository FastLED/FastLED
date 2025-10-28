#pragma once

#include "platforms/math8_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"

namespace fl {

/// @file math8_avr.h
/// AVR-specific optimized assembly implementations of 8-bit math functions.
/// These implementations use the hardware MUL instruction (available on ATmega, not ATtiny).

/// @ingroup lib8tion
/// @{

/// @defgroup Math_AVR AVR Math Implementations
/// Fast AVR assembly implementations of 8-bit math operations (with hardware MUL)
/// @{

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
        "  ldi %0, 0xFF        \n\t"
        "Lnospill_%=:          \n\t"
        /* Restore r1 to "0"; it's expected to always be that */
        "  clr __zero_reg__    \n\t"
        : "+d"(i)
        : "r"(j)
        : "r0", "r1");
    return i;
}

/// Blend a variable proportion of one byte to another (AVR assembly with MUL)
/// Computes: result = ((a * (255 - amountOfB)) + (b * amountOfB)) >> 8
/// Uses two hardware MUL instructions
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

}  // namespace fl
