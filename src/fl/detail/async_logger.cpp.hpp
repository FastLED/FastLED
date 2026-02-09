/// @file detail/async_logger.cpp
/// @brief Async logger implementation using SPSC queue backend

#include "fl/detail/async_logger.h"
#include "fl/stl/cstdio.h"
#include "fl/isr.h"
#include "fl/math_macros.h"
#include "fl/log.h"  // For FL_ERROR macro
#include "fl/task.h"  // For fl::task and fl::Scheduler
#include "fl/async.h"  // For fl::Scheduler::instance()

namespace fl {

namespace detail {
    /// @brief Print error message for disabled logger (non-template helper)
    /// Called from checkLoggerEnabled template function
    /// IMPORTANT: This must NOT be inline - needs external linkage for cross-TU calls
    void printLoggerDisabledError(const char* category_name, const char* define_name) {
        FL_ERROR(category_name << " ASYNC LOGGING NOT ENABLED. "
            << "Add '#define " << define_name << "' before including FastLED.h");
    }
} // namespace detail

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

    void FL_IRAM async_log_flush_timer_isr(void* user_data) {
        BackgroundFlushState* state = static_cast<BackgroundFlushState*>(user_data);
        state->mNeedsFlush = true;
        // Debug: Toggle a counter to verify ISR is firing (visible in debugger)
        static volatile fl::u32 isr_fire_count = 0;
        isr_fire_count++;
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
    detail::BackgroundFlushState& state = Singleton<detail::BackgroundFlushState>::instance();

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

    // Attach timer ISR (returns 0 on success, negative on error)
    if (fl::isr::attachTimerHandler(config, &state.mTimerHandle) != 0) {
        return false;  // Platform doesn't support timers or attachment failed
    }

    state.mEnabled = true;
    return true;
}

void AsyncLogger::disableBackgroundFlush() {
    detail::BackgroundFlushState& state = Singleton<detail::BackgroundFlushState>::instance();

    if (state.mEnabled && state.mTimerHandle.is_valid()) {
        fl::isr::detachHandler(state.mTimerHandle);
        state.mTimerHandle = fl::isr::isr_handle_t();  // Reset to default (invalid) handle
        state.mEnabled = false;
        state.mNeedsFlush = false;
    }
}

bool AsyncLogger::isBackgroundFlushEnabled() const {
    return Singleton<detail::BackgroundFlushState>::instance().mEnabled;
}

// ============================================================================
// Background flush service function (call from main loop)
// ============================================================================

void async_log_service() {
    // Get background flush state
    detail::BackgroundFlushState& state = Singleton<detail::BackgroundFlushState>::instance();

    // Quick check - return immediately if no flush needed
    if (!state.mNeedsFlush) {
        return;
    }

    // Clear flag
    state.mNeedsFlush = false;

    // Flush all instantiated async loggers (uses ActiveLoggerRegistry)
    // Only flushes loggers that have been accessed via template functions
    fl::size n = state.mMessagesPerTick;
    detail::ActiveLoggerRegistry::instance().forEach([n](AsyncLogger& logger) {
        logger.flushN(n);
    });
}

// ============================================================================
// Auto-instantiating service task implementation
// ============================================================================

namespace detail {

AsyncLoggerServiceTask& AsyncLoggerServiceTask::instance() {
    static AsyncLoggerServiceTask task;
    return task;
}

AsyncLoggerServiceTask::AsyncLoggerServiceTask()
    : mIntervalMs(16)
    , mMessagesPerTick(5)
    , mTask()
{
    // Create and register task with scheduler
    // Default 16ms interval = 60 Hz (one frame at 60fps)
    mTask = fl::task::every_ms(mIntervalMs)
        .then([this]() {
            // Service all registered async loggers
            this->serviceLoggers();
        });

    fl::Scheduler::instance().add_task(mTask);
}

void AsyncLoggerServiceTask::setInterval(u32 interval_ms) {
    mIntervalMs = interval_ms;

    // Dynamically update task interval if task exists
    if (mTask.is_valid()) {
        mTask.set_interval_ms(interval_ms);
    }
}

void AsyncLoggerServiceTask::setMessagesPerTick(fl::size messages_per_tick) {
    mMessagesPerTick = messages_per_tick;
}

void AsyncLoggerServiceTask::serviceLoggers() {
    // Flush N messages from all registered loggers
    detail::ActiveLoggerRegistry::instance().forEach([this](AsyncLogger& logger) {
        logger.flushN(mMessagesPerTick);
    });
}

} // namespace detail

// ============================================================================
// Public configuration API
// ============================================================================

void configureAsyncLogService(u32 interval_ms, fl::size messages_per_tick) {
    detail::AsyncLoggerServiceTask::instance().setInterval(interval_ms);
    detail::AsyncLoggerServiceTask::instance().setMessagesPerTick(messages_per_tick);
}

} // namespace fl
