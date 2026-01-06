/// @file task_coroutine_stub.cpp
/// @brief Concrete implementation of TaskCoroutineStub using std::thread

#ifdef FASTLED_STUB_IMPL

#include "task_coroutine_stub.h"
#include <thread>  // ok include (stub platform only)
#include <chrono>  // ok include (stub platform only)
#include <memory>  // ok include (stub platform only)
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "coroutine_runner.h"  // Global coordination for thread safety
<<<<<<< Updated upstream
#include "fl/dbg.h"  // Debug output
=======
#include "fl/warn.h"  // For FL_DBG
>>>>>>> Stashed changes

namespace fl {
namespace platforms {

//=============================================================================
// Global thread registry for DLL cleanup
//=============================================================================

#ifdef TEST_DLL_MODE
namespace {
    fl::vector<std::thread> g_coroutine_threads;  // okay std namespace (stub platform only)
    fl::mutex g_coroutine_mutex;

    void register_coroutine_thread(std::thread&& t) {  // okay std namespace (stub platform only)
        fl::unique_lock<fl::mutex> lock(g_coroutine_mutex);
        g_coroutine_threads.push_back(std::move(t));  // okay std namespace (stub platform only)
    }
}
#endif

/// @brief Clean up all coroutine threads before DLL unload
///
/// This function must be called before the DLL unloads to ensure all
/// coroutine threads are properly joined. Otherwise, detached threads
/// will continue executing code from the unloaded DLL, causing access violations.
///
/// This function is only needed in DLL mode. In normal mode, threads are
/// detached and will continue running as daemon threads.
void cleanup_coroutine_threads() {
#ifdef TEST_DLL_MODE
    // FL_DBG("cleanup_coroutine_threads: Starting cleanup");
    // First, signal all coroutines to stop
    fl::detail::CoroutineRunner::instance().stop_all();
    // FL_DBG("cleanup_coroutine_threads: Called stop_all()");

    // Join all threads (they should exit quickly after stop signal)
    fl::unique_lock<fl::mutex> lock(g_coroutine_mutex);
    // FL_DBG("cleanup_coroutine_threads: Joining " << g_coroutine_threads.size() << " threads");
    for (size_t i = 0; i < g_coroutine_threads.size(); ++i) {
        auto& t = g_coroutine_threads[i];
        if (t.joinable()) {
            // FL_DBG("cleanup_coroutine_threads: Joining thread " << i);
            t.join();  // Wait for thread to exit
            // FL_DBG("cleanup_coroutine_threads: Thread " << i << " joined");
        }
    }
    g_coroutine_threads.clear();
    // FL_DBG("cleanup_coroutine_threads: Cleanup complete");
#endif
}

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
        : mName(fl::move(name))
        , mFunction(fl::move(function)) {

        // Create context for this coroutine using factory method
        // IMPORTANT: Keep shared_ptr ownership - do NOT convert to raw pointer
        mContext = fl::detail::CoroutineContext::create();
        // FL_DBG("TaskCoroutineStub: Created context " << fl::hex << reinterpret_cast<uintptr_t>(mContext.get()) << " for task '" << mName << "'");

<<<<<<< Updated upstream
        // Register in global executor queue (queue stores weak_ptr)
        // FL_DBG("TaskCoroutineStub: Before getting runner instance");
        auto& runner = fl::detail::CoroutineRunner::instance();
        // FL_DBG("TaskCoroutineStub: Got runner instance at " << fl::hex << reinterpret_cast<uintptr_t>(&runner));
=======
        FL_DBG("[TaskCoroutineStubImpl] Creating coroutine '" << name << "'");
>>>>>>> Stashed changes

        // FL_DBG("TaskCoroutineStub: About to enqueue context " << fl::hex << reinterpret_cast<uintptr_t>(mContext.get()));
        runner.enqueue(mContext);
        // FL_DBG("TaskCoroutineStub: Enqueued context " << fl::hex << reinterpret_cast<uintptr_t>(mContext.get()));

<<<<<<< Updated upstream
        // Launch std::thread with queue-based coordination
        // CRITICAL: Must capture mFunction (not parameter 'function' which was moved-from)
        // Capture shared_ptr by value to keep context alive for thread lifetime
        // C++11 workaround: Create local copy since init-captures require C++14
        TaskFunction func = mFunction;
        fl::shared_ptr<fl::detail::CoroutineContext> ctx_shared = mContext;
        std::thread t([ctx_shared, func]() {  // okay std namespace (stub platform only)
            // FL_DBG("TaskCoroutineStub: Thread for context " << fl::hex << reinterpret_cast<uintptr_t>(ctx_shared.get()) << " started, waiting...");
=======
        // Use a synchronization flag to ensure thread starts before we return
        fl::atomic<bool> thread_started(false);

        // Launch detached std::thread with queue-based coordination
        // Thread is detached (daemon-like) but context remains for cleanup
        std::thread([ctx_ptr, function, name, &thread_started]() {  // okay std namespace (stub platform only)
            FL_DBG("[TaskCoroutineStubImpl] Thread started for '" << name << "', about to wait");

            // Signal that thread has started, BEFORE we wait
            thread_started.store(true);

>>>>>>> Stashed changes
            // Wait for executor to signal us
            ctx_shared->wait();
            // FL_DBG("TaskCoroutineStub: Thread for context " << fl::hex << reinterpret_cast<uintptr_t>(ctx_shared.get()) << " woke up from wait");

            FL_DBG("[TaskCoroutineStubImpl] Thread woke up for '" << name << "', checking should_stop");

            // Check if we should stop before even starting
<<<<<<< Updated upstream
            if (ctx_shared->should_stop()) {
                // FL_DBG("TaskCoroutineStub: Thread for context " << fl::hex << reinterpret_cast<uintptr_t>(ctx_shared.get()) << " got stop signal, exiting");
                ctx_shared->set_completed(true);
=======
            if (ctx_ptr->should_stop()) {
                FL_DBG("[TaskCoroutineStubImpl] Thread stopping early for '" << name << "'");
                ctx_ptr->set_completed(true);
>>>>>>> Stashed changes
                fl::detail::CoroutineRunner::instance().signal_next();
                return;
            }

<<<<<<< Updated upstream
            // FL_DBG("TaskCoroutineStub: Thread for context " << fl::hex << reinterpret_cast<uintptr_t>(ctx_shared.get()) << " executing user function");
=======
            FL_DBG("[TaskCoroutineStubImpl] Executing function for '" << name << "'");
>>>>>>> Stashed changes
            // Execute the user's coroutine function
            // FL_DBG("TaskCoroutineStub: About to call user function");
            func();
            // FL_DBG("TaskCoroutineStub: User function returned");

<<<<<<< Updated upstream
            // FL_DBG("TaskCoroutineStub: Thread for context " << fl::hex << reinterpret_cast<uintptr_t>(ctx_shared.get()) << " completed, signaling next");
=======
            FL_DBG("[TaskCoroutineStubImpl] Function completed for '" << name << "', marking as completed");
>>>>>>> Stashed changes
            // Mark as completed and signal next coroutine
            ctx_shared->set_completed(true);
            fl::detail::CoroutineRunner::instance().signal_next();
<<<<<<< Updated upstream
        });

#ifdef TEST_DLL_MODE
        // In DLL mode: Store thread for cleanup (must join before DLL unload)
        register_coroutine_thread(std::move(t));  // okay std namespace (stub platform only)
#else
        // In normal mode: Detach thread (daemon-like, won't block exit)
        t.detach();
#endif
=======
        }).detach();  // Detach - daemon thread, won't block exit

        // Wait for thread to start and reach wait() before we return
        // This prevents a race where async_yield() signals before thread is ready
        while (!thread_started.load()) {
            std::this_thread::yield();  // okay std namespace (stub platform only)
        }

        FL_DBG("[TaskCoroutineStubImpl] Thread confirmed started, enqueueing context");

        // NOW enqueue the context, after thread is guaranteed to be waiting
        fl::detail::CoroutineRunner::instance().enqueue(ctx_ptr);
>>>>>>> Stashed changes
    }

    ~TaskCoroutineStubImpl() override {
        stop();
    }

    void stop() override {
        if (!mContext) return;

        // Get runner
        auto& runner = fl::detail::CoroutineRunner::instance();

        // Signal the context to stop and wake it up
        runner.stop(mContext);

        // Wait briefly for thread to acknowledge stop signal
        // This gives the thread time to set completed flag and exit cleanly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // okay std namespace

        // Remove from queue (prevents dangling weak_ptr)
        runner.remove(mContext);

        // Release our shared_ptr ownership
        // Thread may still hold a reference, so context won't be deleted until thread exits
        mContext.reset();
    }

    bool isRunning() const override {
        return mContext != nullptr;
    }

private:
    fl::shared_ptr<fl::detail::CoroutineContext> mContext;
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
