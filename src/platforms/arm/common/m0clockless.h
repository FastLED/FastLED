// ok no namespace fl
#ifndef __INC_M0_CLOCKLESS_H
#define __INC_M0_CLOCKLESS_H

/******************************************************************************
 * M0 CLOCKLESS LED DRIVER - TRAMPOLINE HEADER
 *
 * This header selects between two implementations:
 * 1. Assembly version (m0clockless_asm.h) - Cycle-accurate, hand-optimized
 * 2. C++ version (m0clockless_c.h) - Portable, compiler-optimized
 *
 * SELECTION MECHANISM:
 * - Define FASTLED_M0_USE_C_IMPLEMENTATION to force C++ version
 * - Define FASTLED_M0_FORCE_ASM to force assembly version
 * - Otherwise, defaults to assembly for M0/M0+, C++ for other platforms
 *
 * WHY TWO VERSIONS?
 *
 * Assembly version (m0clockless_asm.h):
 * - Pros: Cycle-accurate timing, zero overhead, battle-tested
 * - Cons: Platform-specific (M0/M0+ only), harder to maintain
 * - Use when: M0/M0+ platform with strict timing requirements
 *
 * C++ version (m0clockless_c.h):
 * - Pros: Readable, portable, faster on M4/M7/M33+, easier to debug
 * - Cons: Timing not guaranteed, compiler-dependent
 * - Use when: Newer ARM platforms, development, or porting
 *
 * PERFORMANCE NOTES:
 * - On M0/M0+ @ 48MHz: Assembly version is recommended
 * - On M4+ @ 120MHz+: C++ version can match or exceed assembly performance
 * - On M7 @ 216MHz+: C++ version often faster due to better pipeline
 *
 * USAGE:
 * Just include this header - it will automatically select the best version.
 * To override, define FASTLED_M0_USE_C_IMPLEMENTATION or FASTLED_M0_FORCE_ASM
 * before including.
 ******************************************************************************/

/******************************************************************************
 * AUTOMATIC SELECTION LOGIC
 ******************************************************************************/

// Allow user to force C++ implementation
#if defined(FASTLED_M0_USE_C_IMPLEMENTATION)
    #define __USE_M0_C_VERSION__ 1
#elif defined(FASTLED_M0_FORCE_ASM)
    #define __USE_M0_C_VERSION__ 0
#elif defined(__ARM_ARCH_6M__)
    // Cortex-M0: Use assembly for best performance
    #define __USE_M0_C_VERSION__ 0
#elif defined(FASTLED_ARM_M0_PLUS) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
    // Cortex-M0+, M3, M4, M7: C++ version can leverage faster hardware
    #define __USE_M0_C_VERSION__ 0  // not enabled yet though.
#else
    // Unknown platform: default to C++ version (more portable)
    #define __USE_M0_C_VERSION__ 1
#endif

/******************************************************************************
 * INCLUDE SELECTED IMPLEMENTATION
 ******************************************************************************/

#if __USE_M0_C_VERSION__
    // Using C++ implementation
    #include "m0clockless_c.h"

#else
    // Using assembly implementation
    #include "m0clockless_asm.h"

#endif

// Clean up internal macro
#undef __USE_M0_C_VERSION__

/******************************************************************************
 * CONFIGURATION NOTES
 *
 * To force C++ version globally, add to your platformio.ini or build flags:
 *   build_flags = -DFASTLED_M0_USE_C_IMPLEMENTATION
 *
 * To force assembly version globally:
 *   build_flags = -DFASTLED_M0_FORCE_ASM
 *
 * Per-file override (in your sketch/code):
 *   #define FASTLED_M0_USE_C_IMPLEMENTATION
 *   #include <FastLED.h>
 *
 ******************************************************************************/

#endif // __INC_M0_CLOCKLESS_H
