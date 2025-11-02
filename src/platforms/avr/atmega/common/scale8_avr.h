#pragma once

#include "platforms/math8_config.h"
#include "crgb.h"
#include "fastled_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"

namespace fl {

/// @file scale8.h
/// AVR assembly language implementations of 8-bit scaling functions (ATmega with MUL).
/// This file contains optimized AVR assembly versions using hardware MUL instruction.
/// For ATtiny (no MUL), see platforms/attiny/scale8.h

/// @ingroup lib8tion
/// @{

/// @defgroup Scaling_AVR AVR Scaling Implementations
/// Fast AVR assembly implementations of 8-bit scaling operations (ATmega with MUL)
/// @{

/// Scale one byte by a second one (AVR assembly with MUL)
/// @note Takes 4 clocks on AVR with MUL, 2 clocks on ARM
FL_ALWAYS_INLINE uint8_t scale8(uint8_t i, fract8 scale) {
    asm volatile(
#if (FASTLED_SCALE8_FIXED == 1)
        // Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0
        "mul %0, %1          \n\t"
        // Add i to r0, possibly setting the carry flag
        "add r0, %0         \n\t"
        // load 0 into i (using clr instead of ldi to avoid upper register requirement)
        "clr %0             \n\t"
        // walk and chew gum at the same time
        "adc %0, r1          \n\t"
#else
        /* Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0 */
        "mul %0, %1          \n\t"
        /* Move the high 8-bits of the product (r1) back to i */
        "mov %0, r1          \n\t"
    /* Restore r1 to "0"; it's expected to always be that */
#endif
        "clr __zero_reg__    \n\t"

        : "+r"(i)    /* writes to i */
        : "r"(scale) /* uses scale */
        : "r0", "r1" /* clobbers r0, r1 */
    );
    /* Return the result */
    return i;
}

/// The "video" version of scale8() (AVR assembly with MUL)
FL_ALWAYS_INLINE uint8_t scale8_video(uint8_t i, fract8 scale) {
    uint8_t j = 0;
    asm volatile("  tst %[i]\n\t"
                 "  breq L_%=\n\t"
                 "  mul %[i], %[scale]\n\t"
                 "  mov %[j], r1\n\t"
                 "  clr __zero_reg__\n\t"
                 "  cpse %[scale], r1\n\t"
                 "  inc %[j]\n\t"
                 "L_%=: \n\t"
                 : [j] "+r"(j)
                 : [i] "r"(i), [scale] "r"(scale)
                 : "r0", "r1");
    return j;
}

/// This version of scale8() does not clean up the R1 register on AVR (AVR assembly with MUL)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE uint8_t scale8_LEAVING_R1_DIRTY(uint8_t i,
                                                         fract8 scale) {
    asm volatile(
#if (FASTLED_SCALE8_FIXED == 1)
        // Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0
        "mul %0, %1          \n\t"
        // Add i to r0, possibly setting the carry flag
        "add r0, %0         \n\t"
        // load 0 into i (using clr instead of ldi to avoid upper register requirement)
        "clr %0             \n\t"
        // walk and chew gum at the same time
        "adc %0, r1          \n\t"
#else
        /* Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0 */
        "mul %0, %1    \n\t"
        /* Move the high 8-bits of the product (r1) back to i */
        "mov %0, r1    \n\t"
#endif
        /* R1 IS LEFT DIRTY HERE; YOU MUST ZERO IT OUT YOURSELF  */
        /* "clr __zero_reg__    \n\t" */
        : "+r"(i)    /* writes to i */
        : "r"(scale) /* uses scale */
        : "r0", "r1" /* clobbers r0, r1 */
    );
    // Return the result
    return i;
}

/// In place modifying version of scale8() that does not clean up the R1 (AVR assembly with MUL)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE void nscale8_LEAVING_R1_DIRTY(uint8_t &i,
                                                       fract8 scale) {
    asm volatile(
#if (FASTLED_SCALE8_FIXED == 1)
        // Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0
        "mul %0, %1          \n\t"
        // Add i to r0, possibly setting the carry flag
        "add r0, %0         \n\t"
        // load 0 into i (using clr instead of ldi to avoid upper register requirement)
        "clr %0             \n\t"
        // walk and chew gum at the same time
        "adc %0, r1          \n\t"
#else
        /* Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0 */
        "mul %0, %1    \n\t"
        /* Move the high 8-bits of the product (r1) back to i */
        "mov %0, r1    \n\t"
#endif
        /* R1 IS LEFT DIRTY HERE; YOU MUST ZERO IT OUT YOURSELF */
        /* "clr __zero_reg__    \n\t" */

        : "+r"(i)    /* writes to i */
        : "r"(scale) /* uses scale */
        : "r0", "r1" /* clobbers r0, r1 */
    );
}

/// This version of scale8_video() does not clean up the R1 register on AVR (AVR assembly with MUL)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE uint8_t scale8_video_LEAVING_R1_DIRTY(uint8_t i,
                                                               fract8 scale) {
    uint8_t j = 0;
    asm volatile("  tst %[i]\n\t"
                 "  breq L_%=\n\t"
                 "  mul %[i], %[scale]\n\t"
                 "  mov %[j], r1\n\t"
                 "  breq L_%=\n\t"
                 "  inc %[j]\n\t"
                 "L_%=: \n\t"
                 : [j] "+r"(j)
                 : [i] "r"(i), [scale] "r"(scale)
                 : "r0", "r1");
    return j;
}

/// In place modifying version of scale8_video() that does not clean up the R1 (AVR assembly with MUL)
/// @warning You **MUST** call cleanup_R1() after using this function!
FL_ALWAYS_INLINE void nscale8_video_LEAVING_R1_DIRTY(uint8_t &i,
                                                             fract8 scale) {
    asm volatile("  tst %[i]\n\t"
                 "  breq L_%=\n\t"
                 "  mul %[i], %[scale]\n\t"
                 "  mov %[i], r1\n\t"
                 "  breq L_%=\n\t"
                 "  inc %[i]\n\t"
                 "L_%=: \n\t"
                 : [i] "+r"(i)
                 : [scale] "r"(scale)
                 : "r0", "r1");
}

/// Clean up the r1 register after a series of *LEAVING_R1_DIRTY calls (AVR assembly)
FL_ALWAYS_INLINE void cleanup_R1() {
    // Restore r1 to "0"; it's expected to always be that
    asm volatile("clr __zero_reg__  \n\t" : : : "r1");
}

/// Scale a 16-bit unsigned value by an 8-bit value (AVR assembly with MUL)
FL_ALWAYS_INLINE uint16_t scale16by8(uint16_t i, fract8 scale) {
    if (scale == 0) {
        return 0; // Fixes non zero output when scale == 0 and
                  // FASTLED_SCALE8_FIXED==1
    }
#if FASTLED_SCALE8_FIXED == 1
    uint16_t result = 0;
    asm volatile(
        // result.A = HighByte( (i.A x scale) + i.A )
        "  mul %A[i], %[scale]                 \n\t"
        "  add r0, %A[i]                       \n\t"
        "  adc %A[result], r1                  \n\t"

        // result.A-B += i.B x scale
        "  mul %B[i], %[scale]                 \n\t"
        "  add %A[result], r0                  \n\t"
        "  adc %B[result], r1                  \n\t"

        // cleanup r1
        "  clr __zero_reg__                    \n\t"

        // result.A-B += i.B
        "  add %A[result], %B[i]               \n\t"
        "  adc %B[result], __zero_reg__        \n\t"

        : [result] "+r"(result)
        : [i] "r"(i), [scale] "r"(scale)
        : "r0", "r1");
    return result;
#else
    uint16_t result = 0;
    asm volatile(
        // result.A = HighByte(i.A x j )
        "  mul %A[i], %[scale]                 \n\t"
        "  mov %A[result], r1                  \n\t"

        // result.A-B += i.B x j
        "  mul %B[i], %[scale]                 \n\t"
        "  add %A[result], r0                  \n\t"
        "  adc %B[result], r1                  \n\t"

        // cleanup r1
        "  clr __zero_reg__                    \n\t"

        : [result] "+r"(result)
        : [i] "r"(i), [scale] "r"(scale)
        : "r0", "r1");
    return result;
#endif
}

/// Scale a 16-bit unsigned value by an 16-bit value (AVR assembly with MUL)
LIB8STATIC uint16_t scale16(uint16_t i, fract16 scale) {
#if FASTLED_SCALE8_FIXED == 1
    // implemented sort of like
    //   result = ((i * scale) + i ) / 65536
    //
    // why not like this, you may ask?
    //   result = (i * (scale+1)) / 65536
    // the answer is that if scale is 65535, then scale+1
    // will be zero, which is not what we want.
    uint32_t result;
    asm volatile(
        // result.A-B  = i.A x scale.A
        "  mul %A[i], %A[scale]                 \n\t"
        "  movw %A[result], r0                   \n\t"
        : [result] "=r"(result)
        : [i] "r"(i), [scale] "r"(scale)
        : "r0", "r1");

    asm volatile(
        // result.C-D  = i.B x scale.B
        "  mul %B[i], %B[scale]                 \n\t"
        "  movw %C[result], r0                   \n\t"
        : [result] "+r"(result)
        : [i] "r"(i), [scale] "r"(scale)
        : "r0", "r1");

    const uint8_t zero = 0;
    asm volatile(
        // result.B-D += i.B x scale.A
        "  mul %B[i], %A[scale]                 \n\t"

        "  add %B[result], r0                   \n\t"
        "  adc %C[result], r1                   \n\t"
        "  adc %D[result], %[zero]              \n\t"

        // result.B-D += i.A x scale.B
        "  mul %A[i], %B[scale]                 \n\t"

        "  add %B[result], r0                   \n\t"
        "  adc %C[result], r1                   \n\t"
        "  adc %D[result], %[zero]              \n\t"

        // cleanup r1
        "  clr r1                               \n\t"

        : [result] "+r"(result)
        : [i] "r"(i), [scale] "r"(scale), [zero] "r"(zero)
        : "r0", "r1");

    asm volatile(
        // result.A-D += i.A-B
        "  add %A[result], %A[i]                \n\t"
        "  adc %B[result], %B[i]                \n\t"
        "  adc %C[result], %[zero]              \n\t"
        "  adc %D[result], %[zero]              \n\t"
        : [result] "+r"(result)
        : [i] "r"(i), [zero] "r"(zero));

    result = result >> 16;
    return result;
#else
    uint32_t result;
    asm volatile(
        // result.A-B  = i.A x scale.A
        "  mul %A[i], %A[scale]                 \n\t"
        "  movw %A[result], r0                   \n\t"

        : [result] "=r"(result)
        : [i] "r"(i), [scale] "r"(scale)
        : "r0", "r1");

    asm volatile(
        // result.C-D  = i.B x scale.B
        "  mul %B[i], %B[scale]                 \n\t"
        "  movw %C[result], r0                   \n\t"
        : [result] "+r"(result)
        : [i] "r"(i), [scale] "r"(scale)
        : "r0", "r1");

    const uint8_t zero = 0;
    asm volatile(
        // result.B-D += i.B x scale.A
        "  mul %B[i], %A[scale]                 \n\t"

        "  add %B[result], r0                   \n\t"
        "  adc %C[result], r1                   \n\t"
        "  adc %D[result], %[zero]              \n\t"

        // result.B-D += i.A x scale.B
        "  mul %A[i], %B[scale]                 \n\t"

        "  add %B[result], r0                   \n\t"
        "  adc %C[result], r1                   \n\t"
        "  adc %D[result], %[zero]              \n\t"

        // cleanup r1
        "  clr r1                               \n\t"

        : [result] "+r"(result)
        : [i] "r"(i), [scale] "r"(scale), [zero] "r"(zero)
        : "r0", "r1");

    result = result >> 16;
    return result;
#endif
}

/// @} Scaling_AVR

}  // namespace fl
