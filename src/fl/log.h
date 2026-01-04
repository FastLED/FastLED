#pragma once

#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/str.h"
#include "fl/stl/stdint.h"
#include "fl/detail/async_log_queue.h"  // Full definition needed for embedded storage

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

/// @brief Async SPI logging from ISR context (ISR-safe, single producer, const char* only)
/// Queues SPI log messages for deferred output from interrupt handlers
/// @warning X MUST be a const char* literal (e.g., "event") - NO stream expressions or heap allocations
#ifdef FASTLED_LOG_SPI_ASYNC_ENABLED
    #define FL_LOG_SPI_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_spi_async_logger_isr(), X)
#else
    #define FL_LOG_SPI_ASYNC_ISR(X) FL_DBG_NO_OP(X)
#endif

/// @brief Async SPI logging from main thread context (ISR-safe, single producer)
/// Queues SPI log messages for deferred output from main thread
#ifdef FASTLED_LOG_SPI_ASYNC_ENABLED
    #define FL_LOG_SPI_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_spi_async_logger_main(), X)
#else
    #define FL_LOG_SPI_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
#endif

/// @brief Flush all queued SPI log messages (blocking)
/// Drains both ISR and main thread SPI async log buffers. Call from main thread only.
#ifdef FASTLED_LOG_SPI_ASYNC_ENABLED
    #define FL_LOG_SPI_ASYNC_FLUSH() do { \
        fl::get_spi_async_logger_isr().flush(); \
        fl::get_spi_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_SPI_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// RMT Async Logging
// -----------------------------------------------------------------------------

/// @brief Async RMT logging from ISR context (ISR-safe, single producer, const char* only)
/// Queues RMT log messages for deferred output from interrupt handlers
/// @warning X MUST be a const char* literal (e.g., "event") - NO stream expressions or heap allocations
#ifdef FASTLED_LOG_RMT_ASYNC_ENABLED
    #define FL_LOG_RMT_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_rmt_async_logger_isr(), X)
#else
    #define FL_LOG_RMT_ASYNC_ISR(X) FL_DBG_NO_OP(X)
#endif

/// @brief Async RMT logging from main thread context (ISR-safe, single producer)
/// Queues RMT log messages for deferred output from main thread
#ifdef FASTLED_LOG_RMT_ASYNC_ENABLED
    #define FL_LOG_RMT_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_rmt_async_logger_main(), X)
#else
    #define FL_LOG_RMT_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
#endif

/// @brief Flush all queued RMT log messages (blocking)
/// Drains both ISR and main thread RMT async log buffers. Call from main thread only.
#ifdef FASTLED_LOG_RMT_ASYNC_ENABLED
    #define FL_LOG_RMT_ASYNC_FLUSH() do { \
        fl::get_rmt_async_logger_isr().flush(); \
        fl::get_rmt_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_RMT_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// PARLIO Async Logging
// -----------------------------------------------------------------------------

/// @brief Async PARLIO logging from ISR context (ISR-safe, single producer, const char* only)
/// Queues PARLIO log messages for deferred output from interrupt handlers
/// @warning X MUST be a const char* literal (e.g., "event") - NO stream expressions or heap allocations
#ifdef FASTLED_LOG_PARLIO_ASYNC_ENABLED
    #define FL_LOG_PARLIO_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_parlio_async_logger_isr(), X)
#else
    #define FL_LOG_PARLIO_ASYNC_ISR(X) FL_DBG_NO_OP(X)
#endif

/// @brief Async PARLIO logging from main thread context (ISR-safe, single producer)
/// Queues PARLIO log messages for deferred output from main thread
#ifdef FASTLED_LOG_PARLIO_ASYNC_ENABLED
    #define FL_LOG_PARLIO_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_parlio_async_logger_main(), X)
#else
    #define FL_LOG_PARLIO_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
#endif

/// @brief Flush all queued PARLIO log messages (blocking)
/// Drains both ISR and main thread PARLIO async log buffers. Call from main thread only.
#ifdef FASTLED_LOG_PARLIO_ASYNC_ENABLED
    #define FL_LOG_PARLIO_ASYNC_FLUSH() do { \
        fl::get_parlio_async_logger_isr().flush(); \
        fl::get_parlio_async_logger_main().flush(); \
    } while(0)
#else
    #define FL_LOG_PARLIO_ASYNC_FLUSH() do {} while(0)
#endif

// -----------------------------------------------------------------------------
// AUDIO Async Logging
// -----------------------------------------------------------------------------

/// @brief Async AUDIO logging from ISR context (ISR-safe, single producer, const char* only)
/// Queues AUDIO log messages for deferred output from interrupt handlers
/// @warning X MUST be a const char* literal (e.g., "event") - NO stream expressions or heap allocations
#ifdef FASTLED_LOG_AUDIO_ASYNC_ENABLED
    #define FL_LOG_AUDIO_ASYNC_ISR(X) FL_LOG_ASYNC_ISR(fl::get_audio_async_logger_isr(), X)
#else
    #define FL_LOG_AUDIO_ASYNC_ISR(X) FL_DBG_NO_OP(X)
#endif

/// @brief Async AUDIO logging from main thread context (ISR-safe, single producer)
/// Queues AUDIO log messages for deferred output from main thread
#ifdef FASTLED_LOG_AUDIO_ASYNC_ENABLED
    #define FL_LOG_AUDIO_ASYNC_MAIN(X) FL_LOG_ASYNC(fl::get_audio_async_logger_main(), X)
#else
    #define FL_LOG_AUDIO_ASYNC_MAIN(X) FL_DBG_NO_OP(X)
#endif

/// @brief Flush all queued AUDIO log messages (blocking)
/// Drains both ISR and main thread AUDIO async log buffers. Call from main thread only.
#ifdef FASTLED_LOG_AUDIO_ASYNC_ENABLED
    #define FL_LOG_AUDIO_ASYNC_FLUSH() do { \
        fl::get_audio_async_logger_isr().flush(); \
        fl::get_audio_async_logger_main().flush(); \
    } while(0)
#else
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







