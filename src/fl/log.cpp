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
// Global async logger instances (separate ISR and main thread queues)
// ============================================================================
// NOTE: SPSC queue design requires separate queues for ISR vs main thread
//       to avoid race conditions when both contexts call push() concurrently

namespace detail {
    // PARLIO loggers (ISR + main)
    AsyncLogger& parlio_async_logger_isr() {
        static AsyncLogger logger;
        return logger;
    }

    AsyncLogger& parlio_async_logger_main() {
        static AsyncLogger logger;
        return logger;
    }

    // RMT loggers (ISR + main)
    AsyncLogger& rmt_async_logger_isr() {
        static AsyncLogger logger;
        return logger;
    }

    AsyncLogger& rmt_async_logger_main() {
        static AsyncLogger logger;
        return logger;
    }

    // SPI loggers (ISR + main)
    AsyncLogger& spi_async_logger_isr() {
        static AsyncLogger logger;
        return logger;
    }

    AsyncLogger& spi_async_logger_main() {
        static AsyncLogger logger;
        return logger;
    }

    // AUDIO loggers (ISR + main)
    AsyncLogger& audio_async_logger_isr() {
        static AsyncLogger logger;
        return logger;
    }

    AsyncLogger& audio_async_logger_main() {
        static AsyncLogger logger;
        return logger;
    }
} // namespace detail

// Public accessor functions
AsyncLogger& get_parlio_async_logger_isr() {
    return detail::parlio_async_logger_isr();
}

AsyncLogger& get_parlio_async_logger_main() {
    return detail::parlio_async_logger_main();
}

AsyncLogger& get_rmt_async_logger_isr() {
    return detail::rmt_async_logger_isr();
}

AsyncLogger& get_rmt_async_logger_main() {
    return detail::rmt_async_logger_main();
}

AsyncLogger& get_spi_async_logger_isr() {
    return detail::spi_async_logger_isr();
}

AsyncLogger& get_spi_async_logger_main() {
    return detail::spi_async_logger_main();
}

AsyncLogger& get_audio_async_logger_isr() {
    return detail::audio_async_logger_isr();
}

AsyncLogger& get_audio_async_logger_main() {
    return detail::audio_async_logger_main();
}

} // namespace fl
