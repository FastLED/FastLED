#pragma once

/// @file detail/async_logger.h
/// @brief ISR-safe async logger using SPSC queue backend (zero heap allocation)

#include "fl/detail/async_log_queue.h"
#include "fl/int.h"
#include "fl/stl/stdint.h"
#include "fl/str.h"
#include "fl/singleton.h"

namespace fl {

/// @brief ISR-safe async logger wrapper (zero heap allocation)
/// Uses embedded AsyncLogQueue instead of heap-allocated pointer
/// Registers itself automatically in ActiveLoggerRegistry on first access
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

namespace detail {
    // Forward declare BackgroundFlushState for async_log_service
    struct BackgroundFlushState;

    /// @brief Active logger registry for iteration (flush operations)
    /// Only tracks loggers that have been instantiated via template access
    struct ActiveLoggerRegistry {
        fl::vector_fixed<AsyncLogger*, 16> mActiveLoggers;

        static ActiveLoggerRegistry& instance() {
            static ActiveLoggerRegistry registry;
            return registry;
        }

        void registerLogger(AsyncLogger* logger) {
            // Check if already registered
            for (fl::size i = 0; i < mActiveLoggers.size(); ++i) {
                if (mActiveLoggers[i] == logger) {
                    return;  // Already registered
                }
            }
            // Add to active list
            mActiveLoggers.push_back(logger);
        }

        template<typename Func>
        void forEach(Func func) {
            for (fl::size i = 0; i < mActiveLoggers.size(); ++i) {
                func(*mActiveLoggers[i]);
            }
        }
    };
} // namespace detail

/// @brief Template-based logger accessor with auto-registration
/// @tparam N Logger index (0-15, maps to Singleton<AsyncLogger, N>)
/// @return Reference to AsyncLogger singleton instance
/// @note Only instantiated template indices are compiled - linker removes unused ones
template<fl::size N>
inline AsyncLogger& get_async_logger_by_index() {
    // Get singleton instance (only this specific N is instantiated)
    static AsyncLogger* logger_ptr = []() {
        AsyncLogger* ptr = &Singleton<AsyncLogger, N>::instance();
        detail::ActiveLoggerRegistry::instance().registerLogger(ptr);
        return ptr;
    }();

    return *logger_ptr;
}

// Convenience wrappers for category-based access (linker removes unused ones)
inline AsyncLogger& get_parlio_async_logger_isr() { return get_async_logger_by_index<0>(); }
inline AsyncLogger& get_parlio_async_logger_main() { return get_async_logger_by_index<1>(); }
inline AsyncLogger& get_rmt_async_logger_isr() { return get_async_logger_by_index<2>(); }
inline AsyncLogger& get_rmt_async_logger_main() { return get_async_logger_by_index<3>(); }
inline AsyncLogger& get_spi_async_logger_isr() { return get_async_logger_by_index<4>(); }
inline AsyncLogger& get_spi_async_logger_main() { return get_async_logger_by_index<5>(); }
inline AsyncLogger& get_audio_async_logger_isr() { return get_async_logger_by_index<6>(); }
inline AsyncLogger& get_audio_async_logger_main() { return get_async_logger_by_index<7>(); }

} // namespace fl
