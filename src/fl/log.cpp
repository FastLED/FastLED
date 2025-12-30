/// @file log.cpp
/// @brief Async logger implementation using SPSC queue backend

#include "fl/log.h"
#include "fl/detail/async_log_queue.h"
#include "fl/stl/cstdio.h"

namespace fl {

// ============================================================================
// AsyncLogger implementation using AsyncLogQueue backend
// ============================================================================

AsyncLogger::AsyncLogger() : mQueue(new AsyncLogQueue<128, 4096>()) {}

AsyncLogger::~AsyncLogger() {
    delete mQueue;
}

void AsyncLogger::push(const fl::string& msg) {
    mQueue->push(msg);
}

void AsyncLogger::push(const char* msg) {
    mQueue->push(msg);
}

void AsyncLogger::flush() {
    const char* msg;
    fl::u16 len;
    while (mQueue->tryPop(&msg, &len)) {
        // Create a fl::string from the message (copies data)
        fl::string str(msg, len);
        fl::println(str.c_str());
        mQueue->commit();
    }
}

fl::size AsyncLogger::size() const {
    return mQueue->size();
}

bool AsyncLogger::empty() const {
    return mQueue->empty();
}

void AsyncLogger::clear() {
    // Drain queue without printing
    const char* msg;
    fl::u16 len;
    while (mQueue->tryPop(&msg, &len)) {
        mQueue->commit();
    }
}

fl::u32 AsyncLogger::droppedCount() const {
    return mQueue->droppedCount();
}

// ============================================================================
// Global async logger instances (one per category)
// ============================================================================

namespace detail {
    AsyncLogger& parlio_async_logger() {
        static AsyncLogger logger;
        return logger;
    }

    AsyncLogger& rmt_async_logger() {
        static AsyncLogger logger;
        return logger;
    }

    AsyncLogger& spi_async_logger() {
        static AsyncLogger logger;
        return logger;
    }

    AsyncLogger& audio_async_logger() {
        static AsyncLogger logger;
        return logger;
    }
} // namespace detail

// Public accessor functions
AsyncLogger& get_parlio_async_logger() {
    return detail::parlio_async_logger();
}

AsyncLogger& get_rmt_async_logger() {
    return detail::rmt_async_logger();
}

AsyncLogger& get_spi_async_logger() {
    return detail::spi_async_logger();
}

AsyncLogger& get_audio_async_logger() {
    return detail::audio_async_logger();
}

} // namespace fl
