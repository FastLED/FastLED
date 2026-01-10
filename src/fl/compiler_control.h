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
  // C++14/17 extension warnings (for compatibility when using SIMD intrinsic headers)
  #define FL_DISABLE_WARNING_C14_EXTENSIONS FL_DISABLE_WARNING(c++14-extensions)
  #define FL_DISABLE_WARNING_C17_EXTENSIONS FL_DISABLE_WARNING(c++17-extensions)
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
  // GCC doesn't have C++14/17 extension warnings, use no-op
  #define FL_DISABLE_WARNING_C14_EXTENSIONS
  #define FL_DISABLE_WARNING_C17_EXTENSIONS
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
  // Other compilers don't have C++14/17 extension warnings, use no-op
  #define FL_DISABLE_WARNING_C14_EXTENSIONS
  #define FL_DISABLE_WARNING_C17_EXTENSIONS
#endif

// END WARNING SPECIFIC MACROS THAT MAY NOT BE UNIVERSAL.

// Convenient diagnostic control aliases
#define FL_DIAGNOSTIC_PUSH FL_DISABLE_WARNING_PUSH
#define FL_DIAGNOSTIC_POP FL_DISABLE_WARNING_POP
#define FL_DIAGNOSTIC_IGNORE_C14_EXTENSIONS FL_DISABLE_WARNING_C14_EXTENSIONS

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

// Function-level optimization attributes
// GCC supports per-function optimization attributes
// Clang doesn't support optimize("O3"), but supports hot attribute for aggressive optimization
#if defined(__GNUC__) && !defined(__clang__)
  #define FL_OPTIMIZE_FUNCTION __attribute__((optimize("O3")))
  #define FL_OPTIMIZE_O2 __attribute__((optimize("O2")))
#elif defined(__clang__)
  #define FL_OPTIMIZE_FUNCTION __attribute__((hot))
  #define FL_OPTIMIZE_O2 __attribute__((hot))
#else
  #define FL_OPTIMIZE_FUNCTION
  #define FL_OPTIMIZE_O2
#endif

#ifndef FL_LINK_WEAK
#define FL_LINK_WEAK __attribute__((weak))
#endif

// Loop unrolling hint
// Usage: FL_UNROLL(8) for (int i = 0; i < n; i++) { ... }
// Note: Some compilers (e.g., AVR-GCC) may not support this pragma and will
// emit warnings. The macro expands to nothing on unsupported compilers.
#if defined(__AVR__)
  // AVR-GCC does not support #pragma GCC unroll
  #define FL_UNROLL(N)
#elif defined(__GNUC__) && !defined(__clang__)
  // GCC supports #pragma GCC unroll N
  #define FL_UNROLL(N) _Pragma(FL_STRINGIFY(GCC unroll N))
#elif defined(__clang__)
  // Clang supports #pragma unroll N (or #pragma clang loop unroll_count(N))
  #define FL_UNROLL(N) _Pragma(FL_STRINGIFY(unroll N))
#else
  // Unsupported compiler: no-op
  #define FL_UNROLL(N)
#endif

// Mark functions/variables as maybe unused (for compile-time test functions)
#if defined(__GNUC__) || defined(__clang__)
  #define FL_MAYBE_UNUSED __attribute__((unused))
#else
  #define FL_MAYBE_UNUSED
#endif

// Prevent inlining on AVR to reduce register pressure
// AVR has only 16 general-purpose registers (r0-r15), and complex functions
// with divisions can exhaust available registers during LTO optimization.
// This macro forces functions to be out-of-line on AVR while allowing
// inlining on other platforms for performance.
//
// Usage: FL_NO_INLINE_IF_AVR void myComplexFunction() { ... }
//
// Typical use cases:
//   - Functions containing multiple divisions (AVR has no hardware divide)
//   - Template functions with many intermediate variables
//   - Functions called from multiple instantiation sites with LTO enabled
#if defined(__AVR__)
  #define FL_NO_INLINE_IF_AVR __attribute__((noinline))
#else
  #define FL_NO_INLINE_IF_AVR
#endif

// Mark functions to run during C++ static initialization (before main())
// Used for auto-registration of platform-specific implementations
#if defined(__GNUC__) || defined(__clang__)
  #define FL_CONSTRUCTOR __attribute__((constructor))
#elif defined(_MSC_VER)
  // MSVC requires different approach - use #pragma init_seg
  #define FL_CONSTRUCTOR
  #pragma message("Warning: FL_CONSTRUCTOR not fully supported on MSVC")
#else
  #define FL_CONSTRUCTOR
#endif

// Prevent linker from discarding symbols (functions/variables)
// Used to ensure static constructors and other "unused" symbols are retained
#ifndef FL_KEEP_ALIVE
  #if defined(__EMSCRIPTEN__)
    // Emscripten: Use EMSCRIPTEN_KEEPALIVE to export symbols to JavaScript
    #include <emscripten.h>
    #define FL_KEEP_ALIVE EMSCRIPTEN_KEEPALIVE
  #elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: Mark symbol as used to prevent linker optimization
    #define FL_KEEP_ALIVE __attribute__((used))
  #elif defined(_MSC_VER)
    // MSVC: Force symbol retention through optimization pragma
    // Note: MSVC's linker will still strip unreferenced symbols even with this,
    // so static constructors may need explicit references or /OPT:NOREF linker flag
    #define FL_KEEP_ALIVE __pragma(optimize("", off)) __pragma(optimize("", on))
  #else
    // Fallback for unknown compilers
    #define FL_KEEP_ALIVE
  #endif
#endif

// FL_INIT: Convenient macro for static initialization functions
// Registers a function to run during C++ static initialization (before main())
//
// Usage:
//   namespace detail { void init_my_feature() { /* code */ } }
//   FL_INIT(init_my_feature_wrapper, detail::init_my_feature);
//
// The first parameter provides a unique name for the generated wrapper function,
// ensuring no symbol collisions across the codebase.
//
// Implementation details:
//   - GCC/Clang: Uses __attribute__((constructor))
//   - MSVC: Uses .CRT$XCU section to register function pointer at startup
//
// MSVC .CRT$XCU mechanism:
//   - MSVC runs function pointers placed in special CRT sections at startup
//   - Sections are ordered lexicographically: .CRT$XCA, .CRT$XCU (user code), .CRT$XCZ
//   - The function pointer must have static linkage to avoid ODR issues
//   - Works in static libraries if object file is linked (use /WHOLEARCHIVE if needed)
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS

#if defined(_MSC_VER)
  #pragma section(".CRT$XCU", read)

  #define FL_INIT(wrapper_name, func) \
    namespace static_init { \
      static void wrapper_name() { func(); } \
      __declspec(allocate(".CRT$XCU")) \
      static void (*wrapper_name##_ptr)(void) = wrapper_name; \
    }

#elif defined(__GNUC__) || defined(__clang__)
  #define FL_INIT(wrapper_name, func) \
    namespace static_init { \
      FL_CONSTRUCTOR FL_KEEP_ALIVE \
      void wrapper_name() { func(); } \
    }

#else
  #error Unsupported compiler for FL_INIT
#endif

FL_DISABLE_WARNING_POP


// FL_RUN_ONCE - Ensures a function is called only once (thread-safe on platforms with mutexes)
//
// Usage:
//   void initialize_system() { /* code */ }
//   FL_RUN_ONCE(initialize_system());
//
// This macro uses a static boolean flag to ensure the wrapped code executes only once,
// even if called multiple times. The implementation is thread-safe on platforms that
// support multithreading (via FASTLED_MULTITHREADED).
//
// Note: This is a runtime guard (checked each call), unlike FL_INIT which runs at
// static initialization time. FL_RUN_ONCE is useful for lazy initialization or
// when the caller doesn't know if initialization has occurred.
#define FL_RUN_ONCE(code) \
    do { \
        static bool fl_run_once_flag = false; \
        if (!fl_run_once_flag) { \
            fl_run_once_flag = true; \
            code; \
        } \
    } while(0)


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
//   ESP32: Places code in internal SRAM with unique section names (.iram1.text.N)
//   ESP8266: Places code in internal SRAM with unique section names (.iram.text.N)
//   STM32: Places code in fast RAM with unique section names (.text_ram.N)
//   Other platforms: No-op (functions execute from normal memory)
//
// Each FL_IRAM usage automatically generates a unique section using __COUNTER__
// for better debugging, linker control, and IRAM usage tracking.
//
// Platform-specific definitions are in led_sysdefs_*.h headers.
// This fallback applies only to platforms that don't define FL_IRAM.
#ifndef FL_IRAM
  #define FL_IRAM  // No-op on platforms without IRAM support
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

// Restrict pointer attribute for better optimization (no aliasing)
// Tells the compiler that a pointer is the only way to access the underlying
// object for the scope of that pointer, enabling better optimization.
//
// Usage: void process(int* FL_RESTRICT_PARAM a, int* FL_RESTRICT_PARAM b);
//
// The restrict keyword indicates that 'a' and 'b' do not alias (point to
// overlapping memory), allowing the compiler to optimize more aggressively.
#if defined(__cplusplus)
  // C++ doesn't have 'restrict' keyword, use compiler-specific extensions
  #if defined(__GNUC__) || defined(__clang__)
    #define FL_RESTRICT_PARAM __restrict__
  #elif defined(_MSC_VER)
    #define FL_RESTRICT_PARAM __restrict
  #else
    #define FL_RESTRICT_PARAM
  #endif
#else
  // C99 has native 'restrict' keyword
  #define FL_RESTRICT_PARAM restrict
#endif

// Function name macro for debugging and tracing
// Provides the current function name in a portable way across compilers.
//
// Usage: FL_DBG("Entering " << FL_FUNCTION);
//        FL_SCOPED_TRACE() uses this internally
//
// Compiler support:
//   - C++11+: __func__ (standard)
//   - GCC/Clang: __FUNCTION__ (extension, same as __func__ in most cases)
//   - MSVC: __FUNCTION__ (non-standard but widely supported)
//
// Note: __PRETTY_FUNCTION__ (GCC/Clang) includes full signature but is too verbose
#if defined(__cplusplus)
  // C++11 standard: __func__ is a predefined identifier
  #define FL_FUNCTION __func__
#elif defined(__GNUC__) || defined(__clang__)
  // GCC/Clang C: Use __FUNCTION__ extension
  #define FL_FUNCTION __FUNCTION__
#elif defined(_MSC_VER)
  // MSVC: __FUNCTION__ is supported
  #define FL_FUNCTION __FUNCTION__
#else
  // Fallback: Use __func__ (C99 standard)
  #define FL_FUNCTION __func__
#endif
