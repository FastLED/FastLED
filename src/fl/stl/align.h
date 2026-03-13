#pragma once

/// @file align.h
/// Alignment macros and utilities for FastLED
///
/// Provides platform-independent alignment control with special handling
/// for GCC 4.8.3 (Roger Clark STM32 core) and 8-bit AVR platforms.
/// Also provides assume_aligned<N>(ptr) optimizer hint.

#include "fl/stl/cstddef.h"
#include "platforms/is_platform.h"  // IWYU pragma: keep

// ============================================================================
// FL_ALIGNAS(N) - Numeric alignment specifier
// ============================================================================
// Aligns storage to N bytes (must be a power of 2).
// Usage: struct FL_ALIGNAS(8) AlignedStruct { char data[10]; };
#if defined(FL_IS_AVR)
    // AVR (8-bit): No alignment required - make it a no-op to save RAM
    #define FL_ALIGNAS(N) /* nothing */
#elif defined(ESP8266)
    // ESP8266: Cap at 4 bytes - memalign() not available in libstdc++.
    // Over-aligned types (alignas(N) with N > default new alignment)
    // trigger aligned operator new, which calls memalign() and fails at link time.
    #define FL_ALIGNAS(N) __attribute__((aligned(4)))
#elif defined(__GNUC__) && !defined(__clang__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 500
    // GCC 4.x: Use __attribute__ syntax (more reliable than alignas)
    #define FL_ALIGNAS(N) __attribute__((aligned(N)))
#else
    // Modern compilers (GCC 5.0+, Clang, MSVC): Use C++11 alignas
    #define FL_ALIGNAS(N) alignas(N)
#endif

// ============================================================================
// FL_ALIGN - Fixed alignment (Emscripten: 8 bytes, others: no-op)
// ============================================================================
#ifdef FL_IS_WASM
    #define FL_ALIGN_BYTES 8
    #define FL_ALIGN FL_ALIGNAS(FL_ALIGN_BYTES)
#else
    #define FL_ALIGN_BYTES 1
    #define FL_ALIGN
#endif

// ============================================================================
// FL_ALIGN_AS(T) - Type-based alignment using alignof(T)
// ============================================================================
// Aligns storage to match the alignment requirements of type T.
// Usage: struct FL_ALIGN_AS(int) AlignedStruct { char data[10]; };
#if defined(FL_IS_AVR)
    // AVR (8-bit): No alignment required - make it a no-op to save RAM
    #define FL_ALIGN_AS(T) /* nothing */
#elif defined(ESP8266)
    // ESP8266: Cap at 4 bytes (see FL_ALIGNAS above for rationale)
    #define FL_ALIGN_AS(T) __attribute__((aligned(4)))
#elif defined(__GNUC__) && !defined(__clang__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 500
    // GCC 4.x: Use __attribute__ syntax with __alignof__ (more reliable than alignas)
    // This avoids potential issues with alignas(alignof(T)) on older GCC versions
    #define FL_ALIGN_AS(T) __attribute__((aligned(__alignof__(T))))
#else
    // Modern compilers (GCC 5.0+, Clang, MSVC, Emscripten): Use C++11 alignas
    #define FL_ALIGN_AS(T) alignas(alignof(T))
#endif

// ============================================================================
// FL_ALIGN_MAX - Maximum platform alignment (for generic storage)
// ============================================================================
// Aligns storage to the maximum alignment requirement on the platform.
// Suitable for generic type-erased storage that may hold any type.
// Usage: struct FL_ALIGN_MAX GenericStorage { char bytes[64]; };
#if defined(FL_IS_AVR)
    // AVR (8-bit): No alignment required - make it a no-op to save RAM
    #define FL_ALIGN_MAX /* nothing */
#elif defined(ESP8266)
    // ESP8266: Cap at 4 bytes (see FL_ALIGNAS above for rationale)
    #define FL_ALIGN_MAX __attribute__((aligned(4)))
#elif defined(__GNUC__) && !defined(__clang__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 500
    // GCC 4.x: Use __attribute__ syntax with max alignment
    // Note: max_align_t might not be available on all GCC 4.x targets, use conservative 8-byte alignment
    #define FL_ALIGN_MAX __attribute__((aligned(8)))
#else
    // Modern compilers: Use C++11 alignas(max_align_t)
    #define FL_ALIGN_MAX alignas(max_align_t)
#endif

// ============================================================================
// FL_ALIGN_AS_T(expr) - Template expression-based alignment
// ============================================================================
// Workaround for GCC 4.8.3 bug: alignas() doesn't recognize template-dependent
// expressions as compile-time constants. Use __attribute__ syntax instead.
// For GCC < 5.0, use conservative fixed alignment; for modern compilers, use computed value.
//
// Usage: class FL_ALIGN_AS_T(max_align<Types...>::value) variant {};
#if defined(FL_IS_AVR)
    // AVR (8-bit): No alignment required, make it a no-op to save RAM
    #define FL_ALIGN_AS_T(expr) /* nothing */
#elif defined(ESP8266)
    // ESP8266: Cap at 4 bytes (see FL_ALIGNAS above for rationale)
    #define FL_ALIGN_AS_T(expr) __attribute__((aligned(4)))
#elif defined(__GNUC__) && !defined(__clang__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 500
    // GCC 4.x has alignas() bug with template-dependent expressions
    // ARM/32-bit platforms: Use 8-byte alignment (safe for double/i64)
    #define FL_ALIGN_AS_T(expr) __attribute__((aligned(8)))
#else
    // Modern compilers (GCC 5.0+, Clang): Use template-computed optimal alignment
    #define FL_ALIGN_AS_T(expr) alignas(expr)
#endif

// ============================================================================
// fl::assume_aligned<N>(ptr) - Pointer alignment hint for the optimizer
// ============================================================================
// Tells the compiler that ptr is aligned to N bytes, enabling better codegen
// (SIMD loads, wider memory ops). This is a hint only — it does not enforce
// alignment at the call site.

namespace fl {

template <fl::size_t N, typename T>
inline T *assume_aligned(T *ptr) {
#if defined(FL_IS_AVR)
    return ptr;
#elif defined(FL_IS_CLANG) || defined(FL_IS_GCC)
    return static_cast<T *>(__builtin_assume_aligned(ptr, N));
#elif defined(FL_IS_WIN_MSVC)
    // MSVC: __assume tells the optimizer the pointer is N-byte aligned.
    // No __builtin_assume_aligned equivalent exists on MSVC.
    __assume(((char *)ptr - (char *)0) % N == 0);
    return ptr;
#else
    return ptr;
#endif
}

template <fl::size_t N, typename T>
inline const T *assume_aligned(const T *ptr) {
#if defined(FL_IS_AVR)
    return ptr;
#elif defined(FL_IS_CLANG) || defined(FL_IS_GCC)
    return static_cast<const T *>(__builtin_assume_aligned(ptr, N));
#elif defined(FL_IS_WIN_MSVC)
    __assume(((const char *)ptr - (const char *)0) % N == 0);
    return ptr;
#else
    return ptr;
#endif
}

} // namespace fl
