#pragma once

#include "fl/system/sketch_macros.h"
#include "fl/stl/strstream.h"  // IWYU pragma: keep - Required by FL_WARN/FL_ERROR/FL_DBG macros
#include "fl/stl/chrono.h"       // IWYU pragma: keep - Required by FL_WARN_EVERY/FL_DBG_EVERY/FL_PRINT_EVERY macros
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep - FL_NO_INLINE for log_emit

// =============================================================================
// FL_LOG_*_ENABLED → FASTLED_LOG_*_ENABLED backward compatibility
// =============================================================================
// Users can now use the shorter FL_LOG_*_ENABLED defines. The old
// FASTLED_LOG_*_ENABLED names still work for backward compatibility.
#if defined(FL_LOG_SPI_ENABLED) && !defined(FASTLED_LOG_SPI_ENABLED)
    #define FASTLED_LOG_SPI_ENABLED
#endif
#if defined(FL_LOG_RMT_ENABLED) && !defined(FASTLED_LOG_RMT_ENABLED)
    #define FASTLED_LOG_RMT_ENABLED
#endif
#if defined(FL_LOG_PARLIO_ENABLED) && !defined(FASTLED_LOG_PARLIO_ENABLED)
    #define FASTLED_LOG_PARLIO_ENABLED
#endif
#if defined(FL_LOG_AUDIO_ENABLED) && !defined(FASTLED_LOG_AUDIO_ENABLED)
    #define FASTLED_LOG_AUDIO_ENABLED
#endif
#if defined(FL_LOG_INTERRUPT_ENABLED) && !defined(FASTLED_LOG_INTERRUPT_ENABLED)
    #define FASTLED_LOG_INTERRUPT_ENABLED
#endif
#if defined(FL_LOG_FLEXIO_ENABLED) && !defined(FASTLED_LOG_FLEXIO_ENABLED)
    #define FASTLED_LOG_FLEXIO_ENABLED
#endif
#if defined(FL_LOG_OBJECTFLED_ENABLED) && !defined(FASTLED_LOG_OBJECTFLED_ENABLED)
    #define FASTLED_LOG_OBJECTFLED_ENABLED
#endif

// Conditional include for async logger functions (only when logging features are enabled)
#if defined(FASTLED_LOG_SPI_ENABLED) || defined(FASTLED_LOG_RMT_ENABLED) || defined(FASTLED_LOG_PARLIO_ENABLED) || defined(FASTLED_LOG_AUDIO_ENABLED) || defined(FASTLED_LOG_INTERRUPT_ENABLED)
    #include "fl/log/async_logger.h"  // IWYU pragma: keep - Required by FL_LOG_*_ASYNC_* macros
#include "fl/stl/noexcept.h"
#endif

// =============================================================================
// FASTLED_LOG_VERBOSITY — release-mode knob to no-op FL_WARN/FL_INFO/
//                          FL_ERROR/FL_PRINT and free ~20-40 KB of
//                          `.flash.rodata` string-pool bloat on embedded
//                          builds. See FastLED #2773 item 2.3.
// =============================================================================
//
// Each `FL_WARN(...)` / `FL_INFO(...)` / `FL_ERROR(...)` / `FL_PRINT(...)`
// call site bakes a `__FILE__ + __LINE__ + user-supplied message` literal
// into `.flash.rodata`. On the ESP32-S3 NEOPIXEL Blink build that pool is
// ~57 KB — the single biggest `.rodata` contributor (see the audit on
// #2473). The strings are live only because their call sites are live.
//
// Levels (mirror standard log-verbosity conventions; higher = more output):
//
//   * `FASTLED_LOG_VERBOSITY == 0` → ALL of FL_WARN/FL_INFO/FL_ERROR/
//     FL_PRINT/FL_DBG expand to `do {} while(0)`. Strings, `fl::sstream`
//     uses, and any transitive `fl::println` references vanish; the
//     downstream printf chain may shrink further as a result.
//   * `FASTLED_LOG_VERBOSITY == 1` → current behavior; FL_WARN /
//     FL_INFO / FL_ERROR / FL_PRINT fire on platforms where
//     `SKETCH_HAS_LARGE_MEMORY` is set. FL_DBG follows its existing
//     `FASTLED_HAS_DBG` gate.
//   * `FASTLED_LOG_VERBOSITY >= 2` → reserved for future "even noisier
//     than debug" output; currently equivalent to level 1.
//
// Default selection (in resolution order):
//
//   * `FASTLED_TESTING` is set     → 1 (host unit tests need full diagnostics)
//   * `NDEBUG` is set (release)    → 0 (drop ~55 KB of FL_WARN/FL_LOG strings
//                                      on ESP32-S3 NEOPIXEL Blink; see #2886)
//   * otherwise (debug builds)     → 1 (preserve current behavior)
//
// Users who want logs back on a release build define
// `-DFASTLED_LOG_VERBOSITY=1` in their build flags or
// `#define FASTLED_LOG_VERBOSITY 1` before `#include <FastLED.h>`. The
// per-channel logging defines (`FASTLED_LOG_RMT_ENABLED`, `FASTLED_LOG_SPI_ENABLED`,
// etc.) are still gated by their own `#define`s — only the verbosity floor
// changes here.
#ifdef FASTLED_TESTING
  // Host unit tests always want the full diagnostic stream so assertion
  // and warning messages can be checked in CI.
  #if !defined(FASTLED_LOG_VERBOSITY) || FASTLED_LOG_VERBOSITY < 1
    #undef FASTLED_LOG_VERBOSITY
    #define FASTLED_LOG_VERBOSITY 1
  #endif
#else
  #ifndef FASTLED_LOG_VERBOSITY
    #ifdef NDEBUG
      #define FASTLED_LOG_VERBOSITY 0
    #else
      #define FASTLED_LOG_VERBOSITY 1
    #endif
  #endif
#endif

// Resolved compile-time gate. Logging fires only when BOTH the platform
// has a sufficient memory budget AND the user hasn't opted out via
// `FASTLED_LOG_VERBOSITY=0`. The two gates are independent — embedded
// targets without `SKETCH_HAS_LARGE_MEMORY` are no-op regardless of the
// verbosity knob (matching the pre-#2773 behavior on AVR/ATtiny).
#if SKETCH_HAS_LARGE_MEMORY && (FASTLED_LOG_VERBOSITY >= 1)
  #define FASTLED_LOG_RUNTIME_ENABLED 1
#else
  #define FASTLED_LOG_RUNTIME_ENABLED 0
#endif

// =============================================================================
// Forward Declarations
// =============================================================================

// Forward declare println to avoid pulling in full fl/stl/cstdio.h machinery
// This prevents ~5KB memory bloat for simple applications
#ifndef FL_PRINTLN_DECLARED
#define FL_PRINTLN_DECLARED
namespace fl {
    void println(const char* str) FL_NOEXCEPT;
}
#endif

// =============================================================================
// Debug Output Helpers
// =============================================================================

namespace fl {
// ".build/src/fl/dbg.h" -> "src/fl/dbg.h"
// "blah/blah/blah.h" -> "blah.h"
const char *fastled_file_offset(const char *file) FL_NOEXCEPT;
} // namespace fl

// =============================================================================
// Centralised log emit (#2963 Proposal B, Option 3)
// =============================================================================
//
// `fl::detail::log_emit(kind, file, line, body)` consumes the
// user-supplied sstream (passed by rvalue reference) and **rewrites
// it in place** to hold the full prefixed message:
//
//   "<file>(<line>): <KIND>: <user payload>"
//
// then emits via `fl::println(body.c_str())`.
//
// Lifetime-safe variant of Path C's "Option 1" attempt. The body
// sstream temporary lives in the **caller's** frame — its
// lifetime extends to the end of the FL_WARN/FL_ERROR/FL_INFO
// full expression, exactly as the OLD inline macro's temporary
// did. So `body.c_str()` passed to `println` has the same
// async-handler-safe lifetime as before: whatever the test
// harness or platform println did synchronously continues to
// work, and async deferred-flush handlers still see a live
// pointer until the semicolon.
//
// The legacy macro inlined the entire
//   `<< file << "(" << line << "): KIND: " << X`
// chain at every FL_WARN site (1,372 sites × ~30-50 B of inlined
// prefix-format code). With this helper, the prefix-format chain
// is compiled ONCE into libFastLED — each call site only emits
// the `fl::sstream() << X` ctor + the user's payload chain + a
// single `log_emit` call.
namespace fl { namespace detail {
enum class log_kind : fl::u8 {
    WARN  = 0,
    ERROR = 1,
    INFO  = 2,
};
// Takes `body` by non-const lvalue reference (not `&&`) because the
// macro's `fl::sstream() << X` expression has type `sstream&` (the
// returned lvalue ref from chained operator<<). The underlying
// temporary's lifetime is the FL_WARN full-expression's, so mutating
// it through this lvalue ref is well-defined and the c_str() handed
// to println outlives the call exactly as the OLD inline macro's
// `(fl::sstream() << ...).c_str()` did.
//
// `FL_NO_INLINE` enforces the centralisation. Without it, at -O2
// with LTO the compiler often inlines `log_emit` at every FL_WARN
// site — defeating the whole point of moving the prefix chain
// out-of-line. The noinline attribute is what makes this proposal
// actually win bytes; bare centralisation alone isn't enough.
void log_emit(log_kind kind, const char* file, int line, fl::sstream& body) FL_NO_INLINE FL_NOEXCEPT;
} } // namespace fl::detail

// =============================================================================
// Error Macros (FL_ERROR)
// =============================================================================

#ifndef FASTLED_ERROR
// FASTLED_ERROR: Supports stream-style formatting with << operator.
// Routes through fl::detail::log_emit so the prefix-format chain
// is single-copy in libFastLED instead of inlined at every site.
#define FASTLED_ERROR(MSG) fl::detail::log_emit( \
    fl::detail::log_kind::ERROR, \
    fl::fastled_file_offset(__FILE__), int(__LINE__), \
    fl::sstream() << MSG)
#define FASTLED_ERROR_IF(COND, MSG) do { if (COND) FASTLED_ERROR(MSG); } while(0)
#endif

#ifndef FL_ERROR
#if FASTLED_LOG_RUNTIME_ENABLED
// FL_ERROR: routes through fl::detail::log_emit (see header banner above).
#define FL_ERROR(X) fl::detail::log_emit( \
    fl::detail::log_kind::ERROR, \
    fl::fastled_file_offset(__FILE__), int(__LINE__), \
    fl::sstream() << X)
#define FL_ERROR_IF(COND, MSG) do { if (COND) FL_ERROR(MSG); } while(0)
#else
// No-op macros — either memory-constrained platform or FASTLED_LOG_VERBOSITY=0.
#define FL_ERROR(X) do { } while(0)
#define FL_ERROR_IF(COND, MSG) do { } while(0)
#endif
#endif

// =============================================================================
// Warning Macros (FL_WARN)
// =============================================================================

#ifndef FASTLED_WARN
// FASTLED_WARN: Supports stream-style formatting with << operator.
#define FASTLED_WARN(MSG) fl::detail::log_emit( \
    fl::detail::log_kind::WARN, \
    fl::fastled_file_offset(__FILE__), int(__LINE__), \
    fl::sstream() << MSG)
#define FASTLED_WARN_IF(COND, MSG) do { if (COND) FASTLED_WARN(MSG); } while(0)
#endif

#ifndef FL_WARN
#if FASTLED_LOG_RUNTIME_ENABLED
// FL_WARN: routes through fl::detail::log_emit (see header banner above).
#define FL_WARN(X) fl::detail::log_emit( \
    fl::detail::log_kind::WARN, \
    fl::fastled_file_offset(__FILE__), int(__LINE__), \
    fl::sstream() << X)
#define FL_WARN_IF(COND, MSG) do { if (COND) FL_WARN(MSG); } while(0)

// FL_WARN_ONCE: Emits warning only once per unique location (static flag per call site)
// Uses static bool flag initialized to false - first call prints, subsequent calls no-op
#define FL_WARN_ONCE(X) do { \
    static bool _warned = false; \
    if (!_warned) { \
        _warned = true; \
        FL_WARN(X); \
    } \
} while(0)

// FL_WARN_FMT: Alias for FL_WARN (kept for backwards compatibility)
#define FL_WARN_FMT(X) FL_WARN(X)
#define FL_WARN_FMT_IF(COND, MSG) FL_WARN_IF(COND, MSG)

// FL_WARN_LIT: Minimal-overhead variant for literal-only messages. Bypasses
// the fl::sstream + operator<< chain that FL_WARN inlines at every call
// site, and drops the `__FILE__(__LINE__): WARN:` prefix (the literal
// itself should identify the origin, e.g. "[RMT] ..."). Use at well-known
// WARN sites where the message is a fixed string and the byte budget
// matters. See FastLED #2856 item 3.2. The non-literal FL_WARN remains
// available for sites that need operator<< formatting or the file/line
// prefix.
#define FL_WARN_LIT(LITERAL) fl::println(LITERAL)
#define FL_LOG_LIT(LITERAL) fl::println(LITERAL)

// FL_WARN_EVERY: Rate-limited warning that prints at most once per interval
// Uses static timestamp to track last print time - throttles output in tight loops
#define FL_WARN_EVERY(MILLIS, X) do { \
    static fl::u32 _last_warn_time = 0; \
    fl::u32 _now = fl::millis(); \
    if (_now - _last_warn_time >= (MILLIS)) { \
        _last_warn_time = _now; \
        FL_WARN(X); \
    } \
} while(0)
#else
// No-op macros — either memory-constrained platform or FASTLED_LOG_VERBOSITY=0.
#define FL_WARN(X) do { } while(0)
#define FL_WARN_IF(COND, MSG) if(false) { void(fl::sstream_noop() << MSG); }
#define FL_WARN_ONCE(X) do { } while(0)
#define FL_WARN_FMT(X) do { } while(0)
#define FL_WARN_FMT_IF(COND, MSG) do { } while(0)
#define FL_WARN_EVERY(MILLIS, X) do { } while(0)
#define FL_WARN_LIT(LITERAL) do { } while(0)
#define FL_LOG_LIT(LITERAL) do { } while(0)
#endif
#endif

// =============================================================================
// Info Macros (FL_INFO)
// =============================================================================

#ifndef FASTLED_INFO
// FASTLED_INFO: Supports stream-style formatting with << operator.
#define FASTLED_INFO(MSG) fl::detail::log_emit( \
    fl::detail::log_kind::INFO, \
    fl::fastled_file_offset(__FILE__), int(__LINE__), \
    fl::sstream() << MSG)
#define FASTLED_INFO_IF(COND, MSG) do { if (COND) FASTLED_INFO(MSG); } while(0)
#endif

#ifndef FL_INFO
#if FASTLED_LOG_RUNTIME_ENABLED
// FL_INFO: routes through fl::detail::log_emit (see header banner above).
#define FL_INFO(X) fl::detail::log_emit( \
    fl::detail::log_kind::INFO, \
    fl::fastled_file_offset(__FILE__), int(__LINE__), \
    fl::sstream() << X)
#define FL_INFO_IF(COND, MSG) do { if (COND) FL_INFO(MSG); } while(0)

// FL_INFO_ONCE: Emits info only once per unique location (static flag per call site)
// Uses static bool flag initialized to false - first call prints, subsequent calls no-op
#define FL_INFO_ONCE(X) do { \
    static bool _infoed = false; \
    if (!_infoed) { \
        _infoed = true; \
        FL_INFO(X); \
    } \
} while(0)
#else
// No-op macros — either memory-constrained platform or FASTLED_LOG_VERBOSITY=0.
#define FL_INFO(X) do { } while(0)
#define FL_INFO_IF(COND, MSG) do { } while(0)
#define FL_INFO_ONCE(X) do { } while(0)
#endif
#endif

// =============================================================================
// Debug Macros (FL_DBG)
// =============================================================================

#if defined(FL_IS_WASM) || !defined(RELEASE) || defined(FASTLED_TESTING)
#define FASTLED_FORCE_DBG 1
#endif

// Reusable no-op macro for disabled debug output - avoids linker symbol pollution
#define FL_DBG_NO_OP(X) do { if (false) { fl::sstream_noop() << X; } } while(0)

// Debug printing control:
// - FASTLED_DISABLE_DBG=1: Explicitly disable FL_DBG output (highest priority)
// - FASTLED_LOG_VERBOSITY=0: Disable FL_DBG along with all other macros
// - FASTLED_FORCE_DBG: Force enable FL_DBG (auto-set for debug builds)
// - SKETCH_HAS_LARGE_MEMORY: Enable FL_DBG when platform has enough memory
//
// Priority: FASTLED_DISABLE_DBG > FASTLED_LOG_VERBOSITY > FASTLED_FORCE_DBG > SKETCH_HAS_LARGE_MEMORY
#if defined(FASTLED_DISABLE_DBG) && FASTLED_DISABLE_DBG
// Explicit disable takes highest priority - useful for reducing serial spam
#define FASTLED_HAS_DBG 0
#define _FASTLED_DGB(X) FL_DBG_NO_OP(X)
#elif FASTLED_LOG_VERBOSITY < 1
// FASTLED_LOG_VERBOSITY=0 disables FL_DBG too. See #2773 item 2.3.
#define FASTLED_HAS_DBG 0
#define _FASTLED_DGB(X) FL_DBG_NO_OP(X)
#elif !defined(FASTLED_FORCE_DBG) && !SKETCH_HAS_LARGE_MEMORY
// By default, debug printing is disabled to prevent memory bloat in simple applications
#define FASTLED_HAS_DBG 0
#define _FASTLED_DGB(X) FL_DBG_NO_OP(X)
#else
// Explicit debug mode enabled - uses fl::println()
#define FASTLED_HAS_DBG 1
#define _FASTLED_DGB(X)                                                        \
    fl::println(                                                               \
        (fl::sstream() << (fl::fastled_file_offset(__FILE__))                \
                         << "(" << int(__LINE__) << "): " << X)                     \
            .c_str())
#endif

#define FASTLED_DBG(X) _FASTLED_DGB(X)

#ifndef FASTLED_DBG_IF
#define FASTLED_DBG_IF(COND, MSG)                                              \
    if (COND)                                                                  \
    FASTLED_DBG(MSG)
#endif // FASTLED_DBG_IF

// Short-form aliases for convenience (following pattern from warn.h)
#ifndef FL_DBG
#define FL_DBG FASTLED_DBG
#define FL_DBG_IF FASTLED_DBG_IF

// FL_DBG_EVERY: Rate-limited debug output that prints at most once per interval
// Uses static timestamp to track last print time - throttles output in tight loops
#if FASTLED_HAS_DBG
#define FL_DBG_EVERY(MILLIS, X) do { \
    static fl::u32 _last_dbg_time = 0; \
    fl::u32 _now = fl::millis(); \
    if (_now - _last_dbg_time >= (MILLIS)) { \
        _last_dbg_time = _now; \
        FL_DBG(X); \
    } \
} while(0)
#else
#define FL_DBG_EVERY(MILLIS, X) FL_DBG_NO_OP(X)
#endif
#endif

// =============================================================================
// AsyncLogger Class (in fl/log/async_logger.h)
// =============================================================================
// AsyncLogger, LogCategory enum, and accessor functions are now in fl/log/async_logger.h
// This provides template-based lazy instantiation with linker-friendly auto-registration

/// @file fl/log/log.h
/// @brief Centralized logging categories for FastLED hardware interfaces and subsystems
///
/// This file provides category-specific logging macros for different FastLED subsystems.
/// Each category can be independently enabled/disabled via preprocessor defines at compile-time.
///
/// Usage:
///   - Enable category debugging: Define FL_LOG_<CATEGORY>_ENABLED before including this file
///   - Use the macro: FL_LOG_<CATEGORY>("message" << value)
///   - Logging is compile-time controlled; disabled categories produce no code
///
/// Example:
///   #define FL_LOG_SPI_ENABLED
///   #include "fl/log/log.h"
///
///   FL_LOG_SPI("Initializing SPI bus " << bus_id);

// =============================================================================
// General Logging Macros
// =============================================================================

/// @brief Print without prefix (like FL_WARN but without "WARN: " prefix)
/// Uses sstream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
/// Supports stream-style formatting with << operator
///
/// Example:
///   FL_PRINT("Value: " << x);
///   FL_PRINT(ss.str());
#ifndef FL_PRINT
#if FASTLED_LOG_RUNTIME_ENABLED
#define FL_PRINT(X) fl::println((fl::sstream() << X).c_str())

// FL_PRINT_EVERY: Rate-limited print that outputs at most once per interval
// Uses static timestamp to track last print time - throttles output in tight loops
#define FL_PRINT_EVERY(MILLIS, X) do { \
    static fl::u32 _last_print_time = 0; \
    fl::u32 _now = fl::millis(); \
    if (_now - _last_print_time >= (MILLIS)) { \
        _last_print_time = _now; \
        FL_PRINT(X); \
    } \
} while(0)
#else
// No-op macro for memory-constrained platforms
#define FL_PRINT(X) do { } while(0)
#define FL_PRINT_EVERY(MILLIS, X) do { } while(0)
#endif
#endif

// =============================================================================
// Hardware Interface Logging Categories
// =============================================================================

/// @defgroup log_hw_interfaces Hardware Interface Logging
/// @{

/// @brief Serial Peripheral Interface (SPI) logging
/// Logs SPI configuration, initialization, and transfers
#ifdef FASTLED_LOG_SPI_ENABLED
    #define FL_LOG_SPI(X) FL_WARN(X)
#else
    #define FL_LOG_SPI(X) FL_DBG_NO_OP(X)
#endif

/// @brief Remote Control Module (RMT) logging (ESP32)
/// Logs RMT channel configuration, timing, and signal generation
#ifdef FASTLED_LOG_RMT_ENABLED
    #define FL_LOG_RMT(X) FL_WARN(X)
#else
    #define FL_LOG_RMT(X) FL_DBG_NO_OP(X)
#endif

/// @brief Parallel I/O (Parlio) logging (ESP32-P4)
/// Logs Parlio configuration, GPIO setup, and parallel transfers
#ifdef FASTLED_LOG_PARLIO_ENABLED
    #define FL_LOG_PARLIO(X) FL_WARN(X)
#else
    #define FL_LOG_PARLIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief Audio processing logging
/// Logs audio sample processing, FFT computation, beat detection, and detector updates
#ifdef FASTLED_LOG_AUDIO_ENABLED
    #define FL_LOG_AUDIO(X) FL_WARN(X)
#else
    #define FL_LOG_AUDIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief Interrupt handling logging
/// Logs interrupt installation, handler registration, and ISR events
#ifdef FASTLED_LOG_INTERRUPT_ENABLED
    #define FL_LOG_INTERRUPT(X) FL_WARN(X)
#else
    #define FL_LOG_INTERRUPT(X) FL_DBG_NO_OP(X)
#endif

/// @brief FlexIO logging (Teensy 4.x)
/// Logs FlexIO configuration, pin setup, DMA, and signal generation
#ifdef FASTLED_LOG_FLEXIO_ENABLED
    #define FL_LOG_FLEXIO(X) FL_WARN(X)
#else
    #define FL_LOG_FLEXIO(X) FL_DBG_NO_OP(X)
#endif

/// @brief ObjectFLED logging (Teensy 4.x)
/// Logs ObjectFLED configuration, pin mapping, and DMA transfers
#ifdef FASTLED_LOG_OBJECTFLED_ENABLED
    #define FL_LOG_OBJECTFLED(X) FL_WARN(X)
#else
    #define FL_LOG_OBJECTFLED(X) FL_DBG_NO_OP(X)
#endif

/// @}

// =============================================================================
// Async Logging Infrastructure (ISR-Safe)
// =============================================================================

/// @defgroup log_async Async Logging (ISR-Safe)
/// @brief ISR-safe logging using ring buffers for deferred output
///
/// Async logging enables safe logging from interrupt service routines (ISRs)
/// by queueing messages in a ring buffer and draining them later from the
/// main thread. This prevents watchdog timeouts caused by slow Serial I/O
/// operations inside ISRs.
///
/// **Automatic Servicing (Recommended):**
/// Async loggers are automatically serviced by fl::task system when using fl::delay().
/// No manual service calls required!
/// ```cpp
/// void IRAM_ATTR my_isr_handler() {
///     FL_LOG_PARLIO_ASYNC_ISR("DMA complete");  // ISR-safe (const char* only)
/// }
///
/// void loop() {
///     FL_LOG_PARLIO_ASYNC_MAIN("Processing batch " << batch_id);  // Main thread
///     fl::delay(10);  // Loggers serviced automatically every 16ms (60fps)!
/// }
/// ```
///
/// **Manual Control (Optional):**
/// For fine-grained control, you can still manually service or flush loggers:
/// ```cpp
/// void loop() {
///     fl::async_log_service();       // Manual service (for legacy timer-based flush)
///     FL_LOG_PARLIO_ASYNC_FLUSH();   // Flush PARLIO immediately (all queued messages)
/// }
/// ```
///
/// **Custom Configuration (Optional):**
/// Configure service interval and messages-per-tick BEFORE first logger access:
/// ```cpp
/// void setup() {
///     fl::configureAsyncLogService(50, 10);  // 50ms interval, 10 msgs/tick
///     FL_LOG_PARLIO_ASYNC_MAIN("Setup complete");  // Task auto-instantiates here
/// }
/// ```
///
/// **Memory Footprint (Zero Heap Allocation):**
/// - Each logger uses ~5KB static storage (128 descriptors + 4KB arena)
/// - Singletons instantiated lazily on first access (linker removes unused loggers)
/// - ISR messages: Stored as const char* (zero overhead, string literals only)
/// - Main thread messages: Stored as fl::string (arena-allocated, no heap)
/// - Ring buffer auto-evicts oldest messages when full (FIFO overflow)
///
/// **Thread Safety:**
/// - push() is ISR-safe (no locks, single-writer pattern)
/// - flush() must be called from main thread only
///
/// @{

/// @brief Generic async logging macro (captures stream expression into string)
/// @param logger Reference to AsyncLogger instance (e.g., get_parlio_async_logger())
/// @param X Stream-style expression (e.g., "msg " << var)
/// @warning This macro uses heap allocation (fl::string) and should NOT be used in ISR context
/// @see FL_LOG_ASYNC_ISR for ISR-safe const char* only variant
#define FL_LOG_ASYNC(logger, X) \
    do { \
        (logger).push((fl::sstream() << X).str()); \
    } while(0)

/// @brief ISR-safe async logging macro (const char* literals only, zero heap allocation)
/// @param logger Reference to AsyncLogger instance (e.g., get_parlio_async_logger_isr())
/// @param msg Compile-time const char* literal (e.g., "event occurred")
/// @warning msg MUST be a string literal or const char* - NO stream expressions or fl::string
/// @note This is the ONLY safe async logging method for ISR context (no heap allocations)
/// @example FL_LOG_ASYNC_ISR(fl::get_parlio_async_logger_isr(), "DMA complete")
#define FL_LOG_ASYNC_ISR(logger, msg) \
    do { \
        (logger).push(msg); \
    } while(0)

// -----------------------------------------------------------------------------
// SPI Async Logging
// -----------------------------------------------------------------------------

#ifdef FASTLED_LOG_SPI_ENABLED
    #define FL_LOG_SPI_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_spi_async_logger_isr(), X)
    #define FL_LOG_SPI_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_spi_async_logger_main(), X)
    #define FL_LOG_SPI_ASYNC_FLUSH() do { \
        fl::get_spi_async_logger_isr().flush(); \
        fl::get_spi_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_SPI_ASYNC_ISR(X) FL_DBG_NO_OP(X)
    #define FL_LOG_SPI_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
    #define FL_LOG_SPI_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// RMT Async Logging
// -----------------------------------------------------------------------------

#ifdef FASTLED_LOG_RMT_ENABLED
    #define FL_LOG_RMT_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_rmt_async_logger_isr(), X)
    #define FL_LOG_RMT_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_rmt_async_logger_main(), X)
    #define FL_LOG_RMT_ASYNC_FLUSH() do { \
        fl::get_rmt_async_logger_isr().flush(); \
        fl::get_rmt_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_RMT_ASYNC_ISR(X) FL_DBG_NO_OP(X)
    #define FL_LOG_RMT_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
    #define FL_LOG_RMT_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// PARLIO Async Logging
// -----------------------------------------------------------------------------

#ifdef FASTLED_LOG_PARLIO_ENABLED
    #define FL_LOG_PARLIO_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_parlio_async_logger_isr(), X)
    #define FL_LOG_PARLIO_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_parlio_async_logger_main(), X)
    #define FL_LOG_PARLIO_ASYNC_FLUSH() do { \
        fl::get_parlio_async_logger_isr().flush(); \
        fl::get_parlio_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_PARLIO_ASYNC_ISR(X) FL_DBG_NO_OP(X)
    #define FL_LOG_PARLIO_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
    #define FL_LOG_PARLIO_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// AUDIO Async Logging
// -----------------------------------------------------------------------------

#ifdef FASTLED_LOG_AUDIO_ENABLED
    #define FL_LOG_AUDIO_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_audio_async_logger_isr(), X)
    #define FL_LOG_AUDIO_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_audio_async_logger_main(), X)
    #define FL_LOG_AUDIO_ASYNC_FLUSH() do { \
        fl::get_audio_async_logger_isr().flush(); \
        fl::get_audio_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_AUDIO_ASYNC_ISR(X) FL_DBG_NO_OP(X)
    #define FL_LOG_AUDIO_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
    #define FL_LOG_AUDIO_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// INTERRUPT Async Logging
// -----------------------------------------------------------------------------

#ifdef FASTLED_LOG_INTERRUPT_ENABLED
    #define FL_LOG_INTERRUPT_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_interrupt_async_logger_isr(), X)
    #define FL_LOG_INTERRUPT_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_interrupt_async_logger_main(), X)
    #define FL_LOG_INTERRUPT_ASYNC_FLUSH() do { \
        fl::get_interrupt_async_logger_isr().flush(); \
        fl::get_interrupt_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_INTERRUPT_ASYNC_ISR(X) FL_DBG_NO_OP(X)
    #define FL_LOG_INTERRUPT_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
    #define FL_LOG_INTERRUPT_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// FLEXIO Async Logging
// -----------------------------------------------------------------------------

#ifdef FASTLED_LOG_FLEXIO_ENABLED
    #define FL_LOG_FLEXIO_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_flexio_async_logger_isr(), X)
    #define FL_LOG_FLEXIO_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_flexio_async_logger_main(), X)
    #define FL_LOG_FLEXIO_ASYNC_FLUSH() do { \
        fl::get_flexio_async_logger_isr().flush(); \
        fl::get_flexio_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_FLEXIO_ASYNC_ISR(X) FL_DBG_NO_OP(X)
    #define FL_LOG_FLEXIO_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
    #define FL_LOG_FLEXIO_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// OBJECTFLED Async Logging
// -----------------------------------------------------------------------------

#ifdef FASTLED_LOG_OBJECTFLED_ENABLED
    #define FL_LOG_OBJECTFLED_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_objectfled_async_logger_isr(), X)
    #define FL_LOG_OBJECTFLED_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_objectfled_async_logger_main(), X)
    #define FL_LOG_OBJECTFLED_ASYNC_FLUSH() do { \
        fl::get_objectfled_async_logger_isr().flush(); \
        fl::get_objectfled_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_OBJECTFLED_ASYNC_ISR(X) FL_DBG_NO_OP(X)
    #define FL_LOG_OBJECTFLED_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
    #define FL_LOG_OBJECTFLED_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// Background Flush Service
// -----------------------------------------------------------------------------

/// @brief Service function to check timer flag and flush all async loggers
/// @deprecated This function is now optional - async loggers are automatically
///             serviced by fl::task system when using fl::delay()
/// Call this from your main loop() if using enableBackgroundFlush()
/// This function is lightweight - returns immediately if no flush needed
namespace fl {
    void async_log_service() FL_NOEXCEPT;

    /// @brief Configure async logger automatic servicing task
    /// @param interval_ms Service interval in milliseconds (default 16ms = 60fps)
    /// @param messages_per_tick Messages to flush per service call (default 5)
    /// @note Can be called at any time to dynamically adjust servicing rate
    /// @note If not called, defaults are used (16ms interval, 5 messages/tick)
    ///
    /// Example:
    /// ```cpp
    /// void setup() {
    ///     // Configure before accessing any loggers
    ///     fl::configureAsyncLogService(50, 10);  // 50ms interval, 10 msgs/tick
    ///
    ///     FL_LOG_PARLIO_ASYNC_MAIN("Setup complete");
    /// }
    ///
    /// void loop() {
    ///     fl::delay(10);  // Loggers serviced automatically!
    /// }
    /// ```
    void configureAsyncLogService(u32 interval_ms = 16, fl::size messages_per_tick = 5) FL_NOEXCEPT;
}

/// @}
