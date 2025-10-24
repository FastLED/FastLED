#pragma once

#include "platforms/math8_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION

namespace fl {

/// @file math8_avr.h
/// AVR-specific optimized assembly implementations of 8-bit math functions.
/// These implementations use the hardware MUL instruction (available on ATmega, not ATtiny).

/// @ingroup lib8tion
/// @{

/// @defgroup Math_AVR AVR Math Implementations
/// Fast AVR assembly implementations of 8-bit math operations (with hardware MUL)
/// @{

/// 8x8 bit multiplication, with 8-bit result (AVR assembly with MUL or shift-and-add)
/// Uses the hardware MUL instruction when available (2 cycle latency)
/// ~10 cycles total, or ~32 cycles for shift-and-add on ATtiny
#if !defined(LIB8_ATTINY)
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
#else
// Fallback for platforms without hardware MUL (e.g., ATtiny)
LIB8STATIC_ALWAYS_INLINE uint8_t mul8(uint8_t i, uint8_t j) {
    uint8_t result = 0;
    uint8_t cnt = 0x80;
    asm volatile(
        "LOOP_%=:                       \n\t"
        "  sbrc %[j], 0                 \n\t"  /* Check if bit is set */
        "  add %[result], %[i]          \n\t"  /* Add i to result if bit is set */
        "  ror %[result]                \n\t"  /* Rotate result right */
        "  lsr %[j]                     \n\t"  /* Shift j right */
        "  lsr %[cnt]                   \n\t"  /* Shift counter */
        "  brcc LOOP_%=                 \n\t"  /* Loop if counter not zero */
        : [result] "+r"(result), [cnt] "+r"(cnt)
        : [i] "r"(i), [j] "r"(j)
        :);
    return result;
}
#endif

/// 8x8 bit multiplication with 8-bit result, saturating at 0xFF (AVR assembly with MUL or shift-and-add)
/// Uses hardware MUL with high-byte test for saturation detection
/// ~15 cycles, or ~40 cycles for shift-and-add on ATtiny
#if !defined(LIB8_ATTINY)
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
#else
// Fallback for platforms without hardware MUL (e.g., ATtiny)
LIB8STATIC_ALWAYS_INLINE uint8_t qmul8(uint8_t i, uint8_t j) {
    uint8_t result = 0;
    uint8_t cnt = 0x80;
    asm volatile(
        "LOOP_%=:                       \n\t"
        "  sbrc %[j], 0                 \n\t"  /* Check if bit is set */
        "  add %[result], %[i]          \n\t"  /* Add i to result if bit is set */
        "  brcs SATURATE_%=             \n\t"  /* If carry set, saturate */
        "  ror %[result]                \n\t"  /* Rotate result right */
        "  lsr %[j]                     \n\t"  /* Shift j right */
        "  lsr %[cnt]                   \n\t"  /* Shift counter */
        "  brcc LOOP_%=                 \n\t"  /* Loop if counter not zero */
        "  rjmp DONE_%=                 \n\t"
        "SATURATE_%=:                   \n\t"
        "  ldi %[result], 0xFF          \n\t"  /* Set result to 0xFF */
        "DONE_%=:                       \n\t"
        : [result] "+d"(result), [cnt] "+r"(cnt)  /* d = r16-r31 for ldi */
        : [i] "r"(i), [j] "r"(j)
        :);
    return result;
}
#endif

/// Blend a variable proportion of one byte to another (AVR assembly with MUL or shift-and-add)
/// Computes: result = ((a * (255 - amountOfB)) + (b * amountOfB)) >> 8
/// Uses two hardware MUL instructions or shift-and-add fallback
#if (FASTLED_BLEND_FIXED == 1)
#if !defined(LIB8_ATTINY)
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
// Fallback for platforms without hardware MUL (e.g., ATtiny)
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial = 0;
    uint8_t result;
    uint8_t amountOfA = 255 - amountOfB;
    uint8_t mult_cnt = 0x80;
    uint8_t temp_work = 0;

    // Use shift-and-add assembly for two 8x8->16 bit multiplies
    asm volatile(
        // Initialize partial to 0
        "  mov %A[partial], __zero_reg__ \n\t"
        "  mov %B[partial], __zero_reg__ \n\t"
        "  mov %[temp_work], __zero_reg__ \n\t"
        "  mov %[mult_cnt], 0x80         \n\t"

        // Compute: partial = a * amountOfA (first multiply)
        "MULT_A_%=:                      \n\t"
        "  sbrc %[amountOfA], 0          \n\t"  // skip if bit 0 of amountOfA clear
        "  add %[temp_work], %[a]        \n\t"  // add a to temp if bit set
        "  adc %A[partial], __zero_reg__ \n\t"  // propagate carry to partial.low
        "  adc %B[partial], __zero_reg__ \n\t"  // propagate carry to partial.high
        "  ror %[temp_work]              \n\t"  // rotate temp right through carry
        "  ror %A[partial]               \n\t"  // rotate partial.low right
        "  ror %B[partial]               \n\t"  // rotate partial.high right
        "  lsr %[amountOfA]              \n\t"  // shift amountOfA right
        "  lsr %[mult_cnt]               \n\t"  // shift counter
        "brcc MULT_A_%=                  \n\t"  // loop if counter not zero

        // Now add: partial += b * amountOfB (second multiply)
        "  mov %[mult_cnt], 0x80         \n\t"
        "  mov %[temp_work], __zero_reg__ \n\t"

        "MULT_B_%=:                      \n\t"
        "  sbrc %[amountOfB], 0          \n\t"  // skip if bit 0 of amountOfB clear
        "  add %[temp_work], %[b]        \n\t"  // add b to temp if bit set
        "  adc %A[partial], __zero_reg__ \n\t"  // propagate carry to partial.low
        "  adc %B[partial], __zero_reg__ \n\t"  // propagate carry to partial.high
        "  ror %[temp_work]              \n\t"  // rotate temp right through carry
        "  ror %A[partial]               \n\t"  // rotate partial.low right
        "  ror %B[partial]               \n\t"  // rotate partial.high right
        "  lsr %[amountOfB]              \n\t"  // shift amountOfB right
        "  lsr %[mult_cnt]               \n\t"  // shift counter
        "brcc MULT_B_%=                  \n\t"  // loop if counter not zero

        : [partial] "+r"(partial), [temp_work] "+r"(temp_work),
          [mult_cnt] "+r"(mult_cnt), [amountOfA] "+r"(amountOfA),
          [amountOfB] "+r"(amountOfB)
        : [a] "r"(a), [b] "r"(b)
        :);

    result = partial >> 8;
    return result;
}
#endif
#else
#if !defined(LIB8_ATTINY)
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
#else
// Fallback for platforms without hardware MUL (e.g., ATtiny)
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial = 0;
    uint8_t result;
    uint8_t amountOfA = 255 - amountOfB;
    uint8_t mult_cnt = 0x80;
    uint8_t temp_work = 0;

    // Use shift-and-add assembly for two 8x8->16 bit multiplies
    asm volatile(
        // Initialize partial to 0
        "  mov %A[partial], __zero_reg__ \n\t"
        "  mov %B[partial], __zero_reg__ \n\t"
        "  mov %[temp_work], __zero_reg__ \n\t"
        "  mov %[mult_cnt], 0x80         \n\t"

        // Compute: partial = a * amountOfA (first multiply)
        "MULT_A_%=:                      \n\t"
        "  sbrc %[amountOfA], 0          \n\t"  // skip if bit 0 of amountOfA clear
        "  add %[temp_work], %[a]        \n\t"  // add a to temp if bit set
        "  adc %A[partial], __zero_reg__ \n\t"  // propagate carry to partial.low
        "  adc %B[partial], __zero_reg__ \n\t"  // propagate carry to partial.high
        "  ror %[temp_work]              \n\t"  // rotate temp right through carry
        "  ror %A[partial]               \n\t"  // rotate partial.low right
        "  ror %B[partial]               \n\t"  // rotate partial.high right
        "  lsr %[amountOfA]              \n\t"  // shift amountOfA right
        "  lsr %[mult_cnt]               \n\t"  // shift counter
        "brcc MULT_A_%=                  \n\t"  // loop if counter not zero

        // Now add: partial += b * amountOfB (second multiply)
        "  mov %[mult_cnt], 0x80         \n\t"
        "  mov %[temp_work], __zero_reg__ \n\t"

        "MULT_B_%=:                      \n\t"
        "  sbrc %[amountOfB], 0          \n\t"  // skip if bit 0 of amountOfB clear
        "  add %[temp_work], %[b]        \n\t"  // add b to temp if bit set
        "  adc %A[partial], __zero_reg__ \n\t"  // propagate carry to partial.low
        "  adc %B[partial], __zero_reg__ \n\t"  // propagate carry to partial.high
        "  ror %[temp_work]              \n\t"  // rotate temp right through carry
        "  ror %A[partial]               \n\t"  // rotate partial.low right
        "  ror %B[partial]               \n\t"  // rotate partial.high right
        "  lsr %[amountOfB]              \n\t"  // shift amountOfB right
        "  lsr %[mult_cnt]               \n\t"  // shift counter
        "brcc MULT_B_%=                  \n\t"  // loop if counter not zero

        : [partial] "+r"(partial), [temp_work] "+r"(temp_work),
          [mult_cnt] "+r"(mult_cnt), [amountOfA] "+r"(amountOfA),
          [amountOfB] "+r"(amountOfB)
        : [a] "r"(a), [b] "r"(b)
        :);

    result = partial >> 8;
    return result;
}
#endif
#endif

/// @} Math_AVR

/// @} lib8tion

}  // namespace fl

FL_DISABLE_WARNING_POP
