/// @file task_coroutine_stub.h
/// @brief Host-platform TaskCoroutine implementation using std::thread
///
/// This provides REAL coroutine execution on host platforms for unit testing.
/// Uses std::thread instead of FreeRTOS tasks.
///
/// **Embedded Behavior**: Like embedded systems (ESP32/Arduino), coroutine threads
/// are "daemon" threads that don't block process exit. Threads are detached but
/// contexts are tracked via shared_ptr for optional cleanup.

#pragma once

// Note: Using std::thread in .h file for inline implementation (stub platform only)
// This is acceptable for test/stub platform code where performance isn't critical
#include <thread>  // ok include (stub platform only)
#include <memory>  // ok include (stub platform only)
#include "fl/stl/atomic.h"
#include "coroutine_runner.h"  // Global coordination for thread safety

namespace fl {

//=============================================================================
// TaskCoroutine Host Implementation (std::thread-based, queue-coordinated)
//=============================================================================

inline TaskCoroutine::TaskCoroutine(fl::string name,
                                     TaskFunction function,
                                     size_t /*stack_size*/,   // Ignored on host
                                     uint8_t /*priority*/)    // Ignored on host
    : mHandle(nullptr)
    , mName(fl::move(name))
    , mFunction(fl::move(function)) {

    // Create context for this coroutine using factory method
    auto ctx = fl::detail::CoroutineContext::create();
    fl::detail::CoroutineContext* ctx_ptr = ctx.release();  // Transfer ownership to raw pointer

    // Register in global executor queue
    fl::detail::CoroutineRunner::instance().enqueue(ctx_ptr);

    // Store context as handle (for cleanup)
    mHandle = ctx_ptr;

    // Launch detached std::thread with queue-based coordination
    // Thread is detached (daemon-like) but context remains for cleanup
    std::thread([ctx_ptr, function]() {  // okay std namespace (stub platform only)
        // Wait for executor to signal us
        ctx_ptr->wait();

        // Check if we should stop before even starting
        if (ctx_ptr->should_stop()) {
            ctx_ptr->set_completed(true);
            fl::detail::CoroutineRunner::instance().signal_next();
            return;
        }

        // Execute the user's coroutine function
        function();

        // Mark as completed and signal next coroutine
        ctx_ptr->set_completed(true);
        fl::detail::CoroutineRunner::instance().signal_next();
    }).detach();  // Detach - daemon thread, won't block exit
}

inline TaskCoroutine::~TaskCoroutine() {
    stop();
}

inline void TaskCoroutine::stop() {
    if (!mHandle) return;

    // Get context and stop it
    auto* ctx_ptr = static_cast<fl::detail::CoroutineContext*>(mHandle);
    fl::detail::CoroutineRunner::instance().stop(ctx_ptr);

    // Clean up handle (delete the context we own)
    delete ctx_ptr;
    mHandle = nullptr;
}

inline void TaskCoroutine::exitCurrent() {
    // On host, we can't really delete the current thread from within itself
    // The destructor/stop() will handle cleanup
    // Just return to end the thread function
}

inline TaskCoroutine::Handle TaskCoroutine::createTaskImpl(const fl::string& /*name*/,
                                                            const TaskFunction& /*function*/,
                                                            size_t /*stack_size*/,
                                                            uint8_t /*priority*/) {
    // Not used - constructor handles creation directly
    return nullptr;
}

inline void TaskCoroutine::deleteTaskImpl(Handle /*handle*/) {
    // Not used - destructor handles cleanup
}

inline void TaskCoroutine::exitCurrentImpl() {
    // Not used - exitCurrent() handles this
}

} // namespace fl
