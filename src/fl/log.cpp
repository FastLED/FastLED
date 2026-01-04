/// @file log.cpp
/// @brief Async logger implementation using SPSC queue backend

#include "fl/log.h"
#include "fl/detail/async_log_queue.h"
#include "fl/stl/cstdio.h"
#include "fl/isr.h"
#include "fl/math_macros.h"
#include "fl/stl/vector.h"
#include "fl/singleton.h"

namespace fl {

// ============================================================================
// Background flush infrastructure (timer-based automatic flushing)
// ============================================================================

namespace detail {
    // Global state for background flushing
    struct BackgroundFlushState {
        volatile bool mNeedsFlush;         // Flag set by ISR, cleared by service function
        fl::isr::isr_handle_t mTimerHandle; // ISR timer handle
        fl::size mMessagesPerTick;         // Max messages to flush per timer tick
        bool mEnabled;                     // Whether background flushing is enabled

        BackgroundFlushState()
            : mNeedsFlush(false)
            , mTimerHandle()  // Default constructor
            , mMessagesPerTick(5)
            , mEnabled(false) {}
    };

    BackgroundFlushState& get_flush_state() {
        static BackgroundFlushState state;
        return state;
    }

    void FL_IRAM async_log_flush_timer_isr(void* user_data) {
        BackgroundFlushState* state = static_cast<BackgroundFlushState*>(user_data);
        state->mNeedsFlush = true;
    }
} // namespace detail

// ============================================================================
// AsyncLogger implementation using AsyncLogQueue backend (zero heap allocation)
// ============================================================================

AsyncLogger::AsyncLogger() : mQueue() {}

void AsyncLogger::push(const fl::string& msg) {
    mQueue.push(msg);
}

void AsyncLogger::push(const char* msg) {
    mQueue.push(msg);
}

void AsyncLogger::flush() {
    const char* msg;
    fl::u16 len;

    // 256-byte stack buffer - handles most messages in a single chunk
    // Avoids heap allocation entirely by chunking long messages
    char buffer[256];

    while (mQueue.tryPop(&msg, &len)) {
        fl::u16 offset = 0;

        // Process message in chunks to avoid heap allocation
        while (offset < len) {
            fl::u16 chunk_size = fl_min(len - offset, static_cast<fl::u16>(sizeof(buffer) - 1));
            fl::memcpy(buffer, msg + offset, chunk_size);
            buffer[chunk_size] = '\0';  // Null-terminate

            offset += chunk_size;

            if (offset >= len) {
                // Last chunk or whole message - use println (adds newline)
                fl::println(buffer);
            } else {
                // Not last chunk - use print (no newline)
                fl::print(buffer);
            }
        }

        mQueue.commit();
    }
}

fl::size AsyncLogger::size() const {
    return mQueue.size();
}

bool AsyncLogger::empty() const {
    return mQueue.empty();
}

void AsyncLogger::clear() {
    // Drain queue without printing
    const char* msg;
    fl::u16 len;
    while (mQueue.tryPop(&msg, &len)) {
        mQueue.commit();
    }
}

fl::u32 AsyncLogger::droppedCount() const {
    return mQueue.droppedCount();
}

fl::size AsyncLogger::flushN(fl::size maxMessages) {
    fl::size flushed = 0;
    const char* msg;
    fl::u16 len;

    // 256-byte stack buffer - handles most messages in a single chunk
    // Avoids heap allocation entirely by chunking long messages
    char buffer[256];

    while (flushed < maxMessages && mQueue.tryPop(&msg, &len)) {
        fl::u16 offset = 0;

        // Process message in chunks to avoid heap allocation
        while (offset < len) {
            fl::u16 chunk_size = fl_min(len - offset, static_cast<fl::u16>(sizeof(buffer) - 1));
            fl::memcpy(buffer, msg + offset, chunk_size);
            buffer[chunk_size] = '\0';  // Null-terminate

            offset += chunk_size;

            if (offset >= len) {
                // Last chunk or whole message - use println (adds newline)
                fl::println(buffer);
            } else {
                // Not last chunk - use print (no newline)
                fl::print(buffer);
            }
        }

        mQueue.commit();
        flushed++;
    }

    return flushed;
}

bool AsyncLogger::enableBackgroundFlush(fl::u32 interval_ms, fl::size messages_per_tick) {
    detail::BackgroundFlushState& state = detail::get_flush_state();

    // If already enabled, disable first
    if (state.mEnabled) {
        disableBackgroundFlush();
    }

    // Configure flush parameters
    state.mMessagesPerTick = messages_per_tick;

    // Setup ISR timer configuration
    fl::isr::isr_config_t config;
    config.handler = detail::async_log_flush_timer_isr;
    config.user_data = &state;
    config.frequency_hz = 1000 / interval_ms;  // Convert ms to Hz
    config.priority = fl::isr::ISR_PRIORITY_LOW;  // Low priority to avoid interfering with LED timing
    config.flags = fl::isr::ISR_FLAG_IRAM_SAFE;

    // Attach timer ISR
    if (!fl::isr::attachTimerHandler(config, &state.mTimerHandle)) {
        return false;  // Platform doesn't support timers or attachment failed
    }

    state.mEnabled = true;
    return true;
}

void AsyncLogger::disableBackgroundFlush() {
    detail::BackgroundFlushState& state = detail::get_flush_state();

    if (state.mEnabled && state.mTimerHandle.is_valid()) {
        fl::isr::detachHandler(state.mTimerHandle);
        state.mTimerHandle = fl::isr::isr_handle_t();  // Reset to default (invalid) handle
        state.mEnabled = false;
        state.mNeedsFlush = false;
    }
}

bool AsyncLogger::isBackgroundFlushEnabled() const {
    return detail::get_flush_state().mEnabled;
}

// ============================================================================
// Global async logger registry (zero heap allocation with Singleton pattern)
// ============================================================================
// NOTE: SPSC queue design requires separate queues for ISR vs main thread
//       to avoid race conditions when both contexts call push() concurrently
//
// Each logger category uses Singleton<AsyncLogger, N> for static storage.
// The registry tracks which singletons have been instantiated for iteration.

namespace detail {
    /// @brief Registry for async loggers with lazy instantiation (zero heap allocation)
    /// Uses Singleton<AsyncLogger, N> for each category instead of new/delete
    /// Tracks instantiated loggers via fl::vector_fixed<AsyncLogger*, 16>
    class AsyncLoggerRegistry {
    public:
        static constexpr fl::size MAX_LOGGERS = 16;

        AsyncLoggerRegistry() {
            // Initialize all slots to nullptr (singletons not yet accessed)
            for (fl::size i = 0; i < MAX_LOGGERS; ++i) {
                mLoggers.push_back(nullptr);
            }
        }

        /// @brief Get logger for given category, creating it lazily via Singleton
        /// @param category Logger category index (must be < MAX_LOGGERS)
        /// @return Reference to AsyncLogger instance (from Singleton static storage)
        AsyncLogger& get(fl::size category) {
            // Bounds check
            if (category >= MAX_LOGGERS) {
                // Fallback to slot 0 to avoid nullptr deref (should never happen)
                category = 0;
            }

            // Lazy instantiation: get singleton instance and register it
            if (mLoggers[category] == nullptr) {
                // Use switch to map category index to compile-time Singleton<T, N>
                // This allows linker to remove unused singletons
                switch (category) {
                    case 0: mLoggers[0] = &Singleton<AsyncLogger, 0>::instance(); break;
                    case 1: mLoggers[1] = &Singleton<AsyncLogger, 1>::instance(); break;
                    case 2: mLoggers[2] = &Singleton<AsyncLogger, 2>::instance(); break;
                    case 3: mLoggers[3] = &Singleton<AsyncLogger, 3>::instance(); break;
                    case 4: mLoggers[4] = &Singleton<AsyncLogger, 4>::instance(); break;
                    case 5: mLoggers[5] = &Singleton<AsyncLogger, 5>::instance(); break;
                    case 6: mLoggers[6] = &Singleton<AsyncLogger, 6>::instance(); break;
                    case 7: mLoggers[7] = &Singleton<AsyncLogger, 7>::instance(); break;
                    case 8: mLoggers[8] = &Singleton<AsyncLogger, 8>::instance(); break;
                    case 9: mLoggers[9] = &Singleton<AsyncLogger, 9>::instance(); break;
                    case 10: mLoggers[10] = &Singleton<AsyncLogger, 10>::instance(); break;
                    case 11: mLoggers[11] = &Singleton<AsyncLogger, 11>::instance(); break;
                    case 12: mLoggers[12] = &Singleton<AsyncLogger, 12>::instance(); break;
                    case 13: mLoggers[13] = &Singleton<AsyncLogger, 13>::instance(); break;
                    case 14: mLoggers[14] = &Singleton<AsyncLogger, 14>::instance(); break;
                    case 15: mLoggers[15] = &Singleton<AsyncLogger, 15>::instance(); break;
                    default: mLoggers[0] = &Singleton<AsyncLogger, 0>::instance(); break;  // Fallback
                }
            }

            return *mLoggers[category];
        }

        /// @brief Iterate over all instantiated loggers (for service functions)
        /// @param func Callback function receiving AsyncLogger& reference
        template <typename Func>
        void forEach(Func func) {
            for (fl::size i = 0; i < mLoggers.size(); ++i) {
                if (mLoggers[i] != nullptr) {
                    func(*mLoggers[i]);
                }
            }
        }

    private:
        fl::vector_fixed<AsyncLogger*, MAX_LOGGERS> mLoggers;  // Pointers to singleton instances (zero heap)
    };

    /// @brief Get global logger registry singleton
    AsyncLoggerRegistry& getLoggerRegistry() {
        static AsyncLoggerRegistry registry;
        return registry;
    }
} // namespace detail

// Public accessor function - registry-based lookup with lazy Singleton instantiation
AsyncLogger& get_async_logger(LogCategory category) {
    return detail::getLoggerRegistry().get(static_cast<fl::size>(category));
}

// ============================================================================
// Background flush service function (call from main loop)
// ============================================================================

void async_log_service() {
    detail::BackgroundFlushState& state = detail::get_flush_state();

    // Quick check - return immediately if no flush needed
    if (!state.mNeedsFlush) {
        return;
    }

    // Clear flag
    state.mNeedsFlush = false;

    // Flush all instantiated async loggers (registry-based iteration)
    // This automatically handles all registered categories without hardcoding
    fl::size n = state.mMessagesPerTick;
    detail::getLoggerRegistry().forEach([n](AsyncLogger& logger) {
        logger.flushN(n);
    });
}

} // namespace fl
