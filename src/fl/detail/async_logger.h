#pragma once

/// @file detail/async_logger.h
/// @brief ISR-safe async logger using SPSC queue backend (zero heap allocation)

#include "fl/detail/async_log_queue.h"
#include "fl/int.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/singleton.h"
#include "fl/task.h"

namespace fl {

class Scheduler;

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
    INTERRUPT_ISR = 8,
    INTERRUPT_MAIN = 9,
    // Add new categories here (max 16 total)
    MAX_CATEGORIES = 10
};

namespace detail {
    // Forward declare BackgroundFlushState for async_log_service
    struct BackgroundFlushState;

    // Forward declare printLoggerDisabledError (defined in async_logger.cpp.hpp)
    void printLoggerDisabledError(const char* category_name, const char* define_name);

    /// @brief Check if logger is enabled and print error once if not
    /// @tparam InfoProvider Type providing category name, define name, and enabled status
    template<typename InfoProvider>
    inline void checkLoggerEnabled() {
        if (!InfoProvider::isEnabled()) {
            static bool error_printed = false;
            if (!error_printed) {
                error_printed = true;
                printLoggerDisabledError(InfoProvider::categoryName(), InfoProvider::defineName());
            }
        }
    }

    /// @brief Info providers for each logger category
    /// Used to supply category name, define name, and enabled status
    struct ParlioLoggerInfo {
        static const char* categoryName() { return "PARLIO"; }
        static const char* defineName() { return "FASTLED_LOG_PARLIO_ENABLED"; }
        static bool isEnabled() {
            #ifdef FASTLED_LOG_PARLIO_ENABLED
            return true;
            #else
            return false;
            #endif
        }
    };

    struct RmtLoggerInfo {
        static const char* categoryName() { return "RMT"; }
        static const char* defineName() { return "FASTLED_LOG_RMT_ENABLED"; }
        static bool isEnabled() {
            #ifdef FASTLED_LOG_RMT_ENABLED
            return true;
            #else
            return false;
            #endif
        }
    };

    struct SpiLoggerInfo {
        static const char* categoryName() { return "SPI"; }
        static const char* defineName() { return "FASTLED_LOG_SPI_ENABLED"; }
        static bool isEnabled() {
            #ifdef FASTLED_LOG_SPI_ENABLED
            return true;
            #else
            return false;
            #endif
        }
    };

    struct AudioLoggerInfo {
        static const char* categoryName() { return "AUDIO"; }
        static const char* defineName() { return "FASTLED_LOG_AUDIO_ENABLED"; }
        static bool isEnabled() {
            #ifdef FASTLED_LOG_AUDIO_ENABLED
            return true;
            #else
            return false;
            #endif
        }
    };

    struct InterruptLoggerInfo {
        static const char* categoryName() { return "INTERRUPT"; }
        static const char* defineName() { return "FASTLED_LOG_INTERRUPT_ENABLED"; }
        static bool isEnabled() {
            #ifdef FASTLED_LOG_INTERRUPT_ENABLED
            return true;
            #else
            return false;
            #endif
        }
    };

    /// @brief Active logger registry for iteration (flush operations)
    /// Only tracks loggers that have been instantiated via template access
    struct ActiveLoggerRegistry {
        fl::vector_fixed<AsyncLogger*, 16> mActiveLoggers;

        static ActiveLoggerRegistry& instance() {
            return SingletonShared<ActiveLoggerRegistry>::instance();
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

    /// @brief Auto-instantiating task for async logger servicing
    /// Registers itself with fl::Scheduler when first accessed
    /// Only instantiated if at least one async logger is used (linker removes if unused)
    class AsyncLoggerServiceTask {
    public:
        static AsyncLoggerServiceTask& instance();

        /// @brief Change the service interval (default 16ms)
        /// @param interval_ms New interval in milliseconds
        /// @note Can be called at any time to dynamically adjust servicing rate
        void setInterval(u32 interval_ms);

        /// @brief Get current service interval
        u32 getInterval() const { return mIntervalMs; }

        /// @brief Configure number of messages to flush per service call
        /// @param messages_per_tick Number of messages (default 5)
        void setMessagesPerTick(fl::size messages_per_tick);

        /// @brief Get messages per tick
        fl::size getMessagesPerTick() const { return mMessagesPerTick; }

        /// @brief Service all registered loggers (called by task)
        void serviceLoggers();

    private:
        friend class fl::SingletonShared<AsyncLoggerServiceTask>;

        AsyncLoggerServiceTask();
        ~AsyncLoggerServiceTask() = default;

        u32 mIntervalMs;
        fl::size mMessagesPerTick;
        fl::task mTask;  // Task object (stored to allow dynamic interval changes)
    };

} // namespace detail

/// @brief Template-based logger accessor with auto-registration and enablement check
/// @tparam N Logger index (0-15, maps to Singleton<AsyncLogger, N>)
/// @tparam InfoProvider Type providing category name and define name via static methods
/// @return Reference to AsyncLogger singleton instance
/// @note Only instantiated template indices are compiled - linker removes unused ones
/// @note Prints error message once if logging is disabled for this category
template<fl::size N, typename InfoProvider>
inline AsyncLogger& get_async_logger_by_index() {
    // Get singleton instance (only this specific N is instantiated)
    static AsyncLogger* logger_ptr = []() {
        AsyncLogger* ptr = &SingletonShared<AsyncLogger, N>::instance();
        detail::ActiveLoggerRegistry::instance().registerLogger(ptr);

        // Auto-instantiate service task on first logger access
        // This ensures automatic background servicing via fl::delay() and fl::Scheduler
        (void)detail::AsyncLoggerServiceTask::instance();

        return ptr;
    }();

    // Check if logging is enabled, print error once if not
    // (Implementation in async_logger.cpp.hpp uses FL_ERROR)
    detail::checkLoggerEnabled<InfoProvider>();

    return *logger_ptr;
}

// Convenience wrappers for category-based access (linker removes unused ones)
// Each wrapper uses an info provider to check if logging is enabled
// and prints an error message on first access if logging is disabled

inline AsyncLogger& get_parlio_async_logger_isr() {
    return get_async_logger_by_index<0, detail::ParlioLoggerInfo>();
}

inline AsyncLogger& get_parlio_async_logger_main() {
    return get_async_logger_by_index<1, detail::ParlioLoggerInfo>();
}

inline AsyncLogger& get_rmt_async_logger_isr() {
    return get_async_logger_by_index<2, detail::RmtLoggerInfo>();
}

inline AsyncLogger& get_rmt_async_logger_main() {
    return get_async_logger_by_index<3, detail::RmtLoggerInfo>();
}

inline AsyncLogger& get_spi_async_logger_isr() {
    return get_async_logger_by_index<4, detail::SpiLoggerInfo>();
}

inline AsyncLogger& get_spi_async_logger_main() {
    return get_async_logger_by_index<5, detail::SpiLoggerInfo>();
}

inline AsyncLogger& get_audio_async_logger_isr() {
    return get_async_logger_by_index<6, detail::AudioLoggerInfo>();
}

inline AsyncLogger& get_audio_async_logger_main() {
    return get_async_logger_by_index<7, detail::AudioLoggerInfo>();
}

inline AsyncLogger& get_interrupt_async_logger_isr() {
    return get_async_logger_by_index<8, detail::InterruptLoggerInfo>();
}

inline AsyncLogger& get_interrupt_async_logger_main() {
    return get_async_logger_by_index<9, detail::InterruptLoggerInfo>();
}

} // namespace fl
