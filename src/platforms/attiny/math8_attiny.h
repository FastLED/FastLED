#pragma once

#include "platforms/math8_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"



namespace fl {

/// @file math8_attiny.h
/// ATtiny-specific optimized assembly implementations of 8-bit math functions.
/// These implementations use shift-and-add algorithms (no hardware MUL instruction).

/// @ingroup lib8tion
/// @{

/// @defgroup Math_ATtiny ATtiny Math Implementations (Shift-and-Add)
/// Fast shift-and-add assembly implementations for ATtiny (no MUL instruction)
/// @{

/// 8x8 bit multiplication, with 8-bit result (ATtiny shift-and-add assembly)
/// Uses shift-and-add (Russian Peasant) algorithm
/// ~32 cycles vs ~80+ for C compiler's __mulqi3
FL_ALWAYS_INLINE uint8_t mul8(uint8_t i, uint8_t j) {
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

/// 8x8 bit multiplication with 8-bit result, saturating at 0xFF (ATtiny shift-and-add assembly)
/// Uses shift-and-add with overflow detection via carry flag
/// ~40 cycles vs ~90+ for C with saturation
FL_ALWAYS_INLINE uint8_t qmul8(uint8_t i, uint8_t j) {
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
        "  ser %[result]                \n\t"  /* Set result to 0xFF */
        "DONE_%=:                       \n\t"
        : [result] "+r"(result), [cnt] "+r"(cnt)  /* any register (using ser instead of ldi) */
        : [i] "r"(i), [j] "r"(j)
        :);
    return result;
}

/// Blend a variable proportion of one byte to another (ATtiny assembly)
/// Computes: result = ((a * (255 - amountOfB)) + (b * amountOfB)) >> 8
/// Uses two shift-and-add 8x8->16 bit multiplies
/// ~90 cycles vs ~120+ for C with compiler multiply
LIB8STATIC uint8_t blend8(uint8_t a, uint8_t b, uint8_t amountOfB) {
    uint16_t partial = 0;
    uint8_t result;
    uint8_t amountOfA = 255 - amountOfB;
    uint8_t mult_cnt = 0x80;
    uint8_t temp_work = 0;

    // Use shift-and-add assembly for two 8x8->16 bit multiplies
    asm volatile(
        // Initialize partial and temp_work to 0 (mult_cnt already initialized to 0x80 in C)
        "  mov %A[partial], __zero_reg__ \n\t"
        "  mov %B[partial], __zero_reg__ \n\t"
        "  mov %[temp_work], __zero_reg__ \n\t"

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
        // Reinitialize mult_cnt to 0x80 and temp_work to 0
        "  ser %[mult_cnt]               \n\t"  // Set all bits: 0xFF
        "  lsr %[mult_cnt]               \n\t"  // Shift right: 0x7F, C=1
        "  adc %[mult_cnt], __zero_reg__ \n\t"  // Add carry: 0x7F + 1 = 0x80
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

/// @} Math_ATtiny

/// @} lib8tion

}  // namespace fl
