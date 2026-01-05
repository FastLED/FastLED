#pragma once

#include "fl/sketch_macros.h"
#include "fl/stl/strstream.h"
#include "fl/str.h"
#include "fl/int.h"
#include "fl/stl/stdint.h"
#include "fl/detail/async_log_queue.h"  // Full definition needed for embedded storage

// =============================================================================
// Forward Declarations
// =============================================================================

// Forward declare println to avoid pulling in full fl/stl/cstdio.h machinery
// This prevents ~5KB memory bloat for simple applications
#ifndef FL_PRINTLN_DECLARED
#define FL_PRINTLN_DECLARED
namespace fl {
    void println(const char* str);
}
#endif

// =============================================================================
// Debug Output Helpers
// =============================================================================

namespace fl {
// ".build/src/fl/dbg.h" -> "src/fl/dbg.h"
// "blah/blah/blah.h" -> "blah.h"
const char *fastled_file_offset(const char *file);
} // namespace fl

// =============================================================================
// Error Macros (FL_ERROR)
// =============================================================================

#ifndef FASTLED_ERROR
// FASTLED_ERROR: Supports stream-style formatting with << operator
// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
#define FASTLED_ERROR(MSG) fl::println((fl::StrStream() << "ERROR: " << MSG).c_str())
#define FASTLED_ERROR_IF(COND, MSG) do { if (COND) FASTLED_ERROR(MSG); } while(0)
#endif

#ifndef FL_ERROR
#if SKETCH_HAS_LOTS_OF_MEMORY
// FL_ERROR: Supports both string literals and stream-style formatting with << operator
// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
#define FL_ERROR(X) fl::println((fl::StrStream() << "ERROR: " << X).c_str())
#define FL_ERROR_IF(COND, MSG) do { if (COND) FL_ERROR(MSG); } while(0)
#else
// No-op macros for memory-constrained platforms
#define FL_ERROR(X) do { } while(0)
#define FL_ERROR_IF(COND, MSG) do { } while(0)
#endif
#endif

// =============================================================================
// Warning Macros (FL_WARN)
// =============================================================================

#ifndef FASTLED_WARN
// FASTLED_WARN: Supports stream-style formatting with << operator
// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
#define FASTLED_WARN(MSG) fl::println((fl::StrStream() << "WARN: " << MSG).c_str())
#define FASTLED_WARN_IF(COND, MSG) do { if (COND) FASTLED_WARN(MSG); } while(0)
#endif

#ifndef FL_WARN
#if SKETCH_HAS_LOTS_OF_MEMORY
// FL_WARN: Supports both string literals and stream-style formatting with << operator
// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
#define FL_WARN(X) fl::println((fl::StrStream() << "WARN: " << X).c_str())
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
#else
// No-op macros for memory-constrained platforms
#define FL_WARN(X) do { } while(0)
#define FL_WARN_IF(COND, MSG) do { } while(0)
#define FL_WARN_ONCE(X) do { } while(0)
#define FL_WARN_FMT(X) do { } while(0)
#define FL_WARN_FMT_IF(COND, MSG) do { } while(0)
#endif
#endif

// =============================================================================
// Debug Macros (FL_DBG)
// =============================================================================

#if __EMSCRIPTEN__ || !defined(RELEASE) || defined(FASTLED_TESTING)
#define FASTLED_FORCE_DBG 1
#endif

// Reusable no-op macro for disabled debug output - avoids linker symbol pollution
#define FL_DBG_NO_OP(X) do { if (false) { fl::FakeStrStream() << X; } } while(0)

// Debug printing: Enable only when explicitly requested to avoid ~5KB memory bloat
#if !defined(FASTLED_FORCE_DBG) && !SKETCH_HAS_LOTS_OF_MEMORY
// By default, debug printing is disabled to prevent memory bloat in simple applications
#define FASTLED_HAS_DBG 0
#define _FASTLED_DGB(X) FL_DBG_NO_OP(X)
#else
// Explicit debug mode enabled - uses fl::println()
#define FASTLED_HAS_DBG 1
#define _FASTLED_DGB(X)                                                        \
    fl::println(                                                               \
        (fl::StrStream() << (fl::fastled_file_offset(__FILE__))                \
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
#endif

// =============================================================================
// AsyncLogger Class
// =============================================================================

namespace fl {
    /// @brief ISR-safe async logger wrapper (zero heap allocation)
    /// Implementation in log.cpp
    /// Uses embedded AsyncLogQueue instead of heap-allocated pointer
    class AsyncLogger {
    public:
        AsyncLogger();
        ~AsyncLogger() = default;  // No cleanup needed (embedded storage)

        void push(const fl::string& msg);
        void push(const char* msg);
        void flush();
        fl::size size() const;
        bool empty() const;
        void clear();
        fl::u32 droppedCount() const;

        /// @brief Flush up to N messages from queue (bounded flush)
        /// @param maxMessages Maximum number of messages to process
        /// @return Number of messages actually flushed
        fl::size flushN(fl::size maxMessages);

        /// @brief Enable background timer-based flushing (opt-in)
        /// @param interval_ms Flush interval in milliseconds (e.g., 100 = 10 Hz)
        /// @param messages_per_tick Maximum messages to flush per timer tick (default 5)
        /// @return true if enabled successfully, false if platform doesn't support timers
        bool enableBackgroundFlush(fl::u32 interval_ms, fl::size messages_per_tick = 5);

        /// @brief Disable background flushing
        void disableBackgroundFlush();

        /// @brief Check if background flushing is enabled
        bool isBackgroundFlushEnabled() const;

    private:
        AsyncLogQueue<128, 4096> mQueue;  // Embedded storage (zero heap allocation)
    };

    /// @brief Logger category identifiers for registry-based access
    /// Each category has separate ISR and main thread loggers (SPSC requirement)
    enum class LogCategory : fl::u8 {
        PARLIO_ISR = 0,
        PARLIO_MAIN = 1,
        RMT_ISR = 2,
        RMT_MAIN = 3,
        SPI_ISR = 4,
        SPI_MAIN = 5,
        AUDIO_ISR = 6,
        AUDIO_MAIN = 7,
        // Add new categories here (max 16 total)
        MAX_CATEGORIES = 8
    };

    // Global async logger accessor functions (defined in log.cpp)
    // Registry-based lazy instantiation - loggers created on first access
    AsyncLogger& get_async_logger(LogCategory category);

    // Convenience wrappers for backward compatibility
    inline AsyncLogger& get_parlio_async_logger_isr() { return get_async_logger(LogCategory::PARLIO_ISR); }
    inline AsyncLogger& get_parlio_async_logger_main() { return get_async_logger(LogCategory::PARLIO_MAIN); }
    inline AsyncLogger& get_rmt_async_logger_isr() { return get_async_logger(LogCategory::RMT_ISR); }
    inline AsyncLogger& get_rmt_async_logger_main() { return get_async_logger(LogCategory::RMT_MAIN); }
    inline AsyncLogger& get_spi_async_logger_isr() { return get_async_logger(LogCategory::SPI_ISR); }
    inline AsyncLogger& get_spi_async_logger_main() { return get_async_logger(LogCategory::SPI_MAIN); }
    inline AsyncLogger& get_audio_async_logger_isr() { return get_async_logger(LogCategory::AUDIO_ISR); }
    inline AsyncLogger& get_audio_async_logger_main() { return get_async_logger(LogCategory::AUDIO_MAIN); }
}

/// @file fl/log.h
/// @brief Centralized logging categories for FastLED hardware interfaces and subsystems
///
/// This file provides category-specific logging macros for different FastLED subsystems.
/// Each category can be independently enabled/disabled via preprocessor defines at compile-time.
///
/// Usage:
///   - Enable category debugging: Define FASTLED_LOG_<CATEGORY>_ENABLED before including this file
///   - Use the macro: FL_LOG_<CATEGORY>("message" << value)
///   - Logging is compile-time controlled; disabled categories produce no code
///
/// Example:
///   #define FASTLED_LOG_SPI_ENABLED
///   #include "fl/log.h"
///
///   FL_LOG_SPI("Initializing SPI bus " << bus_id);

// =============================================================================
// General Logging Macros
// =============================================================================

/// @brief Print without prefix (like FL_WARN but without "WARN: " prefix)
/// Uses StrStream for dynamic formatting (avoids printf bloat ~40KB, adds ~3KB)
/// Supports stream-style formatting with << operator
///
/// Example:
///   FL_PRINT("Value: " << x);
///   FL_PRINT(ss.str());
#ifndef FL_PRINT
#if SKETCH_HAS_LOTS_OF_MEMORY
#define FL_PRINT(X) fl::println((fl::StrStream() << X).c_str())
#else
// No-op macro for memory-constrained platforms
#define FL_PRINT(X) do { } while(0)
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
/// **Usage Pattern:**
/// ```cpp
/// void IRAM_ATTR my_isr_handler() {
///     FL_LOG_PARLIO_ASYNC_ISR("DMA complete");  // ISR-safe (const char* only, zero heap allocation)
/// }
///
/// void loop() {
///     FL_LOG_PARLIO_ASYNC_MAIN("Processing batch " << batch_id);  // Main thread (supports stream expressions)
///     FL_LOG_PARLIO_ASYNC_FLUSH();  // Drain queued messages (blocking)
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
        (logger).push((fl::StrStream() << X).str()); \
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
// Background Flush Service
// -----------------------------------------------------------------------------

/// @brief Service function to check timer flag and flush all async loggers
/// Call this from your main loop() if using enableBackgroundFlush()
/// This function is lightweight - returns immediately if no flush needed
namespace fl {
    void async_log_service();
}

/// @}
