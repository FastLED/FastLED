#pragma once

#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/str.h"
#include "fl/stl/stdint.h"

namespace fl {
    // Forward declarations
    template <fl::size, fl::size> class AsyncLogQueue;

    /// @brief ISR-safe async logger wrapper
    /// Implementation in log.cpp
    class AsyncLogger {
    public:
        AsyncLogger();
        ~AsyncLogger();

        void push(const fl::string& msg);
        void push(const char* msg);
        void flush();
        fl::size size() const;
        bool empty() const;
        void clear();
        fl::u32 droppedCount() const;

    private:
        AsyncLogQueue<128, 4096>* mQueue;  // Pointer to hide implementation details
    };

    // Global async logger accessor functions (defined in log.cpp)
    // Separate queues for ISR and main thread contexts (SPSC requirement)
    AsyncLogger& get_parlio_async_logger_isr();
    AsyncLogger& get_parlio_async_logger_main();
    AsyncLogger& get_rmt_async_logger_isr();
    AsyncLogger& get_rmt_async_logger_main();
    AsyncLogger& get_spi_async_logger_isr();
    AsyncLogger& get_spi_async_logger_main();
    AsyncLogger& get_audio_async_logger_isr();
    AsyncLogger& get_audio_async_logger_main();
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
///     FL_LOG_PARLIO_ASYNC("ISR event " << counter);  // Safe in ISR context
/// }
///
/// void loop() {
///     FL_LOG_PARLIO_ASYNC_FLUSH();  // Drain queued messages (blocking)
/// }
/// ```
///
/// **Memory Footprint:**
/// - Each logger uses ~1KB per 128 messages (configurable via FASTLED_ASYNC_LOG_CAPACITY)
/// - Messages stored as fl::string (heap-allocated, but allocated before ISR)
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
#define FL_LOG_ASYNC(logger, X) \
    do { \
        (logger).push((fl::StrStream() << X).str()); \
    } while(0)

// -----------------------------------------------------------------------------
// SPI Async Logging
// -----------------------------------------------------------------------------

/// @brief Async SPI logging from ISR context (ISR-safe, single producer)
/// Queues SPI log messages for deferred output from interrupt handlers
#ifdef FASTLED_LOG_SPI_ASYNC_ENABLED
    #define FL_LOG_SPI_ASYNC_ISR(X) FL_LOG_ASYNC(fl::get_spi_async_logger_isr(), X)
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

/// @brief Async RMT logging from ISR context (ISR-safe, single producer)
/// Queues RMT log messages for deferred output from interrupt handlers
#ifdef FASTLED_LOG_RMT_ASYNC_ENABLED
    #define FL_LOG_RMT_ASYNC_ISR(X) FL_LOG_ASYNC(fl::get_rmt_async_logger_isr(), X)
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

/// @brief Async PARLIO logging from ISR context (ISR-safe, single producer)
/// Queues PARLIO log messages for deferred output from interrupt handlers
#ifdef FASTLED_LOG_PARLIO_ASYNC_ENABLED
    #define FL_LOG_PARLIO_ASYNC_ISR(X) FL_LOG_ASYNC(fl::get_parlio_async_logger_isr(), X)
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

/// @brief Async AUDIO logging from ISR context (ISR-safe, single producer)
/// Queues AUDIO log messages for deferred output from interrupt handlers
#ifdef FASTLED_LOG_AUDIO_ASYNC_ENABLED
    #define FL_LOG_AUDIO_ASYNC_ISR(X) FL_LOG_ASYNC(fl::get_audio_async_logger_isr(), X)
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

/// @}







