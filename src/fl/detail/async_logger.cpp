/// @file detail/async_logger.cpp
/// @brief Async logger implementation using SPSC queue backend

#include "fl/detail/async_logger.h"
#include "fl/stl/cstdio.h"
#include "fl/isr.h"
#include "fl/math_macros.h"

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

    // Attach timer ISR
    if (!fl::isr::attachTimerHandler(config, &state.mTimerHandle)) {
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

} // namespace fl
