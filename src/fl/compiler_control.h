#pragma once

// Stringify helper for pragma arguments
#define FL_STRINGIFY2(x) #x
#define FL_STRINGIFY(x) FL_STRINGIFY2(x)

// BEGIN BASE MACROS
#if defined(__clang__)
  #define FL_DISABLE_WARNING_PUSH         _Pragma("clang diagnostic push")
  #define FL_DISABLE_WARNING_POP          _Pragma("clang diagnostic pop")
  // Usage: FL_DISABLE_WARNING(float-equal)
  #define FL_DISABLE_WARNING(warning)     _Pragma(FL_STRINGIFY(clang diagnostic ignored "-W" #warning))

#elif defined(__GNUC__) && (__GNUC__*100 + __GNUC_MINOR__) >= 406
  #define FL_DISABLE_WARNING_PUSH         _Pragma("GCC diagnostic push")
  #define FL_DISABLE_WARNING_POP          _Pragma("GCC diagnostic pop")
  // Usage: FL_DISABLE_WARNING(float-equal)
  #define FL_DISABLE_WARNING(warning)     _Pragma(FL_STRINGIFY(GCC diagnostic ignored "-W" #warning))
#else
  #define FL_DISABLE_WARNING_PUSH
  #define FL_DISABLE_WARNING_POP
  #define FL_DISABLE_WARNING(warning)
#endif
// END BASE MACROS

// WARNING SPECIFIC MACROS THAT MAY NOT BE UNIVERSAL.
#if defined(__clang__)
  #define FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS \
    FL_DISABLE_WARNING(global-constructors)
  #define FL_DISABLE_WARNING_SELF_ASSIGN \
    FL_DISABLE_WARNING(self-assign)
  #define FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED \
    FL_DISABLE_WARNING(self-assign-overloaded)
  // Clang doesn't have format-truncation warning, use no-op
  #define FL_DISABLE_FORMAT_TRUNCATION
  #define FL_DISABLE_WARNING_NULL_DEREFERENCE FL_DISABLE_WARNING(null-dereference)
  #define FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH
  #define FL_DISABLE_WARNING_UNUSED_PARAMETER FL_DISABLE_WARNING(unused-parameter)
  #define FL_DISABLE_WARNING_RETURN_TYPE
  #define FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION FL_DISABLE_WARNING(implicit-int-conversion)
  #define FL_DISABLE_WARNING_FLOAT_CONVERSION FL_DISABLE_WARNING(float-conversion)
  #define FL_DISABLE_WARNING_SIGN_CONVERSION FL_DISABLE_WARNING(sign-conversion)
  #define FL_DISABLE_WARNING_SHORTEN_64_TO_32 FL_DISABLE_WARNING(shorten-64-to-32)
  // Clang doesn't have volatile warning, use no-op
  #define FL_DISABLE_WARNING_VOLATILE
  #define FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS FL_DISABLE_WARNING(deprecated-declarations)
  // Clang doesn't have subobject-linkage warning, use no-op
  #define FL_DISABLE_WARNING_SUBOBJECT_LINKAGE
#elif defined(__GNUC__) && (__GNUC__*100 + __GNUC_MINOR__) >= 406
  // GCC doesn't have global-constructors warning, use no-op
  #define FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
  // GCC doesn't have self-assign warning, use no-op
  #define FL_DISABLE_WARNING_SELF_ASSIGN
  // GCC doesn't have self-assign-overloaded warning, use no-op
  #define FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
  // GCC has format-truncation warning
  #define FL_DISABLE_FORMAT_TRUNCATION \
    FL_DISABLE_WARNING(format-truncation)
  #define FL_DISABLE_WARNING_NULL_DEREFERENCE
  #define FL_DISABLE_WARNING_UNUSED_PARAMETER \
    FL_DISABLE_WARNING(unused-parameter)
  #define FL_DISABLE_WARNING_RETURN_TYPE \
    FL_DISABLE_WARNING(return-type)

  // implicit-fallthrough warning requires GCC >= 7.0
  #if (__GNUC__*100 + __GNUC_MINOR__) >= 700
    #define FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH FL_DISABLE_WARNING(implicit-fallthrough)
  #else
    #define FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH
  #endif
  // GCC doesn't support these conversion warnings on older versions
  #define FL_DISABLE_WARNING_FLOAT_CONVERSION
  #define FL_DISABLE_WARNING_SIGN_CONVERSION
  #define FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
  // GCC doesn't have shorten-64-to-32 warning, use no-op
  #define FL_DISABLE_WARNING_SHORTEN_64_TO_32
  // volatile warning requires GCC >= 10.0 (added in GCC 10)
  #if defined(__AVR__) || ((__GNUC__*100 + __GNUC_MINOR__) < 1000)
    #define FL_DISABLE_WARNING_VOLATILE
  #else
    #define FL_DISABLE_WARNING_VOLATILE FL_DISABLE_WARNING(volatile)
  #define FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS FL_DISABLE_WARNING(deprecated-declarations)
  #endif
  // GCC has subobject-linkage warning
  #define FL_DISABLE_WARNING_SUBOBJECT_LINKAGE FL_DISABLE_WARNING(subobject-linkage)
#else
  #define FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
  #define FL_DISABLE_WARNING_SELF_ASSIGN
  #define FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
  #define FL_DISABLE_FORMAT_TRUNCATION
  #define FL_DISABLE_WARNING_NULL_DEREFERENCE
  #define FL_DISABLE_WARNING_UNUSED_PARAMETER
  #define FL_DISABLE_WARNING_RETURN_TYPE
  #define FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
  #define FL_DISABLE_WARNING_FLOAT_CONVERSION
  #define FL_DISABLE_WARNING_SIGN_CONVERSION
  #define FL_DISABLE_WARNING_SHORTEN_64_TO_32
  #define FL_DISABLE_WARNING_VOLATILE
  #define FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
  #define FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS FL_DISABLE_WARNING(deprecated-declarations)
  // Other compilers don't have subobject-linkage warning, use no-op
  #define FL_DISABLE_WARNING_SUBOBJECT_LINKAGE
#endif

// END WARNING SPECIFIC MACROS THAT MAY NOT BE UNIVERSAL.

// Fast math optimization controls with additional aggressive flags
#if defined(__clang__)
  #define FL_FAST_MATH_BEGIN \
    _Pragma("clang diagnostic push") \
    _Pragma("STDC FP_CONTRACT ON")

  #define FL_FAST_MATH_END   _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
  #define FL_FAST_MATH_BEGIN \
    _Pragma("GCC push_options") \
    _Pragma("GCC optimize (\"fast-math\")") \
    _Pragma("GCC optimize (\"tree-vectorize\")") \
    _Pragma("GCC optimize (\"unroll-loops\")")

  #define FL_FAST_MATH_END   _Pragma("GCC pop_options")

#elif defined(_MSC_VER)
  #define FL_FAST_MATH_BEGIN __pragma(float_control(precise, off))
  #define FL_FAST_MATH_END   __pragma(float_control(precise, on))
#else
  #define FL_FAST_MATH_BEGIN /* nothing */
  #define FL_FAST_MATH_END   /* nothing */
#endif

// Optimization Level O3
#if defined(__clang__)
  #define FL_OPTIMIZATION_LEVEL_O3_BEGIN \
    _Pragma("clang diagnostic push")

  #define FL_OPTIMIZATION_LEVEL_O3_END   _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
  #define FL_OPTIMIZATION_LEVEL_O3_BEGIN \
    _Pragma("GCC push_options") \
    _Pragma("GCC optimize (\"O3\")")

  #define FL_OPTIMIZATION_LEVEL_O3_END   _Pragma("GCC pop_options")
#else
  #define FL_OPTIMIZATION_LEVEL_O3_BEGIN /* nothing */
  #define FL_OPTIMIZATION_LEVEL_O3_END   /* nothing */
#endif

// Optimization Level O0 (Debug/No optimization)
#if defined(__clang__)
  #define FL_OPTIMIZATION_LEVEL_O0_BEGIN \
    _Pragma("clang diagnostic push")

  #define FL_OPTIMIZATION_LEVEL_O0_END   _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
  #define FL_OPTIMIZATION_LEVEL_O0_BEGIN \
    _Pragma("GCC push_options") \
    _Pragma("GCC optimize (\"O0\")")

  #define FL_OPTIMIZATION_LEVEL_O0_END   _Pragma("GCC pop_options")
#else
  #define FL_OPTIMIZATION_LEVEL_O0_BEGIN /* nothing */
  #define FL_OPTIMIZATION_LEVEL_O0_END   /* nothing */
#endif

// Optimization for exact timing (cycle-accurate code)
// Disables optimizations that can affect instruction timing predictability.
// Default level is O2 for balance between performance and timing predictability.
// Override with: #define FL_TIMING_OPT_LEVEL 3 before including this header.
#ifndef FL_TIMING_OPT_LEVEL
#define FL_TIMING_OPT_LEVEL 2
#endif

#if defined(__GNUC__)
  #define _FL_STR1(x) #x
  #define _FL_STR(x)  _FL_STR1(x)

  #define FL_BEGIN_OPTIMIZE_FOR_EXACT_TIMING        \
      _Pragma("GCC push_options")                   \
      _Pragma("GCC optimize(\"O" _FL_STR(FL_TIMING_OPT_LEVEL) "\")") \
      _Pragma("GCC optimize(\"-fno-lto\")")         \
      _Pragma("GCC optimize(\"-fno-schedule-insns\")")  \
      _Pragma("GCC optimize(\"-fno-schedule-insns2\")") \
      _Pragma("GCC optimize(\"-fno-reorder-blocks\")")  \
      _Pragma("GCC optimize(\"-fno-tree-vectorize\")")  \
      _Pragma("GCC optimize(\"-fno-tree-reassoc\")")    \
      _Pragma("GCC optimize(\"-fno-unroll-loops\")")

  #define FL_END_OPTIMIZE_FOR_EXACT_TIMING \
      _Pragma("GCC pop_options")
#else
  #define FL_BEGIN_OPTIMIZE_FOR_EXACT_TIMING /* nothing */
  #define FL_END_OPTIMIZE_FOR_EXACT_TIMING   /* nothing */
#endif

#ifndef FL_LINK_WEAK
#define FL_LINK_WEAK __attribute__((weak))
#endif

// Mark functions/variables as maybe unused (for compile-time test functions)
#if defined(__GNUC__) || defined(__clang__)
  #define FL_MAYBE_UNUSED __attribute__((unused))
#else
  #define FL_MAYBE_UNUSED
#endif


// C linkage macros for compatibility with C++ name mangling
#ifdef __cplusplus
  #define FL_EXTERN_C_BEGIN extern "C" {
  #define FL_EXTERN_C_END   }
  #define FL_EXTERN_C       extern "C"
#else
  #define FL_EXTERN_C_BEGIN
  #define FL_EXTERN_C_END
  #define FL_EXTERN_C
#endif

// Platform-specific IRAM attribute for functions that must run from IRAM
// (ESP32/ESP8266: required for ISR handlers and functions called from ISRs)
//
// Usage: void FL_IRAM myInterruptHandler() { ... }
//
// Platform behavior:
//   ESP32: Uses IRAM_ATTR from esp_attr.h (places code in internal SRAM)
//   ESP8266: Uses IRAM_ATTR from Arduino SDK (places code in internal SRAM)
//   STM32: Uses .text_ram section attribute (places code in fast RAM)
//   Other platforms: No-op (functions execute from normal memory)
#if defined(ESP32)
  #ifdef __cplusplus
    FL_EXTERN_C_BEGIN
    #include "esp_attr.h"  // Provides IRAM_ATTR for ESP32
    FL_EXTERN_C_END
  #else
    #include "esp_attr.h"
  #endif
  #define FL_IRAM IRAM_ATTR
#elif defined(ESP8266)
  // ESP8266: IRAM_ATTR is provided by Arduino ESP8266 SDK (via ets_sys.h -> c_types.h)
  // No need to include headers - it's already defined by the platform
  #ifndef IRAM_ATTR
    #define IRAM_ATTR __attribute__((section(".iram.text")))
  #endif
  #define FL_IRAM IRAM_ATTR
#elif defined(__arm__) && defined(STM32)
  // STM32: Place in fast RAM section (.text_ram)
  #define FL_IRAM __attribute__((section(".text_ram")))
#else
  #define FL_IRAM  // No-op on other platforms
#endif


// Inline constexpr macro for C++11/17 compatibility
// In C++17+, constexpr variables are implicitly inline (external linkage)
// In C++11/14, we now use enum-based timing types instead of weak symbols
// (see fl/chipsets/led_timing.h for enum-based TIMING_* definitions)
#define FL_INLINE_CONSTEXPR inline constexpr

// C++14 constexpr function support
// C++11 constexpr functions cannot have local variables or multiple return statements.
// These restrictions were relaxed in C++14. For C++11 compatibility, we use inline
// functions instead of constexpr for complex functions.
#if __cplusplus >= 201402L
#define FL_CONSTEXPR14 constexpr
#else
#define FL_CONSTEXPR14 inline
#endif

// Mark functions whose return value must be used (generates compiler warning if ignored)
// Preferred over [[nodiscard]] for C++11/14 compatibility
//
// Usage: FL_NODISCARD bool try_lock();
//
// By default, this generates a WARNING when the return value is ignored.
// To promote to a compile ERROR, use compiler flags:
//   GCC/Clang: -Werror=unused-result
//   MSVC:      /we6031
//
// Note: These flags ONLY affect functions explicitly marked with FL_NODISCARD.
// Regular functions without this attribute can still have their return values
// ignored without warnings/errors.
#if __cplusplus >= 201703L
  // C++17+: Use standard [[nodiscard]] attribute
  #define FL_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
  // GCC/Clang: Use warn_unused_result attribute
  #define FL_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
  // MSVC: Use SAL annotation
  #define FL_NODISCARD _Check_return_
#else
  // Unsupported compiler: no-op
  #define FL_NODISCARD
#endif

