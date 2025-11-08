#pragma once

/// @file align.h
/// Alignment macros for FastLED
///
/// Provides platform-independent alignment control with special handling
/// for GCC 4.8.3 (Roger Clark STM32 core) and 8-bit AVR platforms.

#if defined(FASTLED_TESTING) || defined(__EMSCRIPTEN__)
// alignof is a built-in keyword in C++11, no header needed
// max_align_t is not used in this file
#endif

// ============================================================================
// FL_ALIGN - Fixed alignment (Emscripten: 8 bytes, others: no-op)
// FL_ALIGN_AS(T) - Type-based alignment using alignof(T)
// ============================================================================
#ifdef __EMSCRIPTEN__
#define FL_ALIGN_BYTES 8
#define FL_ALIGN alignas(FL_ALIGN_BYTES)
#define FL_ALIGN_AS(T) alignas(alignof(T))
#else
#define FL_ALIGN_BYTES 1
#define FL_ALIGN
#define FL_ALIGN_AS(T)
#endif

// ============================================================================
// FL_ALIGN_AS_T(expr) - Template expression-based alignment
// ============================================================================
// Workaround for GCC 4.8.3 bug: alignas() doesn't recognize template-dependent
// expressions as compile-time constants. Use __attribute__ syntax instead.
// For GCC < 5.0, use conservative fixed alignment; for modern compilers, use computed value.
//
// Usage: class FL_ALIGN_AS_T(max_align<Types...>::value) Variant {};
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 500
    // GCC 4.x has alignas() bug with template-dependent expressions
    #if defined(__AVR__)
        // AVR (8-bit): No alignment required, make it a no-op to save RAM
        #define FL_ALIGN_AS_T(expr) /* nothing */
    #else
        // ARM/32-bit platforms: Use 8-byte alignment (safe for double/int64_t)
        // ARM Cortex-M (STM32): Max alignment is 8 bytes (double/int64_t)
        // ESP32/ESP8266: Max alignment is 4-8 bytes (wastes 0-4 bytes, acceptable)
        #define FL_ALIGN_AS_T(expr) __attribute__((aligned(8)))
    #endif
#else
    // Modern compilers (GCC 5.0+, Clang): Use template-computed optimal alignment
    #define FL_ALIGN_AS_T(expr) alignas(expr)
#endif
