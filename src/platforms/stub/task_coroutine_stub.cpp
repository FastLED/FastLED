/// @file task_coroutine_stub.cpp
/// @brief Concrete implementation of TaskCoroutineStub using std::thread

#ifdef FASTLED_STUB_IMPL

#include "task_coroutine_stub.h"
#include <thread>  // ok include (stub platform only)
#include <memory>  // ok include (stub platform only)
#include "fl/stl/atomic.h"
#include "coroutine_runner.h"  // Global coordination for thread safety

namespace fl {
namespace platforms {

//=============================================================================
// TaskCoroutineStubImpl - Concrete implementation
//=============================================================================

/// @brief Concrete std::thread-based implementation of TaskCoroutineStub
///
/// This class provides the actual implementation details, hidden from the
/// header interface. Uses std::thread for coroutine execution with queue-based
/// coordination via CoroutineRunner singleton.
class TaskCoroutineStubImpl : public TaskCoroutineStub {
public:
    TaskCoroutineStubImpl(fl::string name,
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

    ~TaskCoroutineStubImpl() override {
        stop();
    }

    void stop() override {
        if (!mHandle) return;

        // Get context and stop it
        auto* ctx_ptr = static_cast<fl::detail::CoroutineContext*>(mHandle);
        fl::detail::CoroutineRunner::instance().stop(ctx_ptr);

        // Clean up handle (delete the context we own)
        delete ctx_ptr;
        mHandle = nullptr;
    }

    bool isRunning() const override {
        return mHandle != nullptr;
    }

private:
    void* mHandle;
    fl::string mName;
    TaskFunction mFunction;
};

//=============================================================================
// Static factory method
//=============================================================================

TaskCoroutineStub* TaskCoroutineStub::create(fl::string name,
                                              TaskFunction function,
                                              size_t stack_size,
                                              uint8_t priority) {
    return new TaskCoroutineStubImpl(fl::move(name), fl::move(function), stack_size, priority);
}

//=============================================================================
// Factory function - creates platform-specific implementation
//=============================================================================

ITaskCoroutine* createTaskCoroutine(fl::string name,
                                     ITaskCoroutine::TaskFunction function,
                                     size_t stack_size,
                                     uint8_t priority) {
    return TaskCoroutineStub::create(fl::move(name), fl::move(function), stack_size, priority);
}

//=============================================================================
// Static exitCurrent implementation
//=============================================================================

void ITaskCoroutine::exitCurrent() {
    // On host, we can't really delete the current thread from within itself
    // The destructor/stop() will handle cleanup
    // Just return to end the thread function
}

} // namespace platforms
} // namespace fl

#endif // FASTLED_STUB_IMPL
