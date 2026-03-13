/// @file coroutine_stub.h
/// @brief Host/Stub coroutine infrastructure — all declarations
///
/// Consolidates the thread-based coroutine runtime, semaphore-based
/// coroutine runner, and task coroutine declarations into a single header.
///
/// ## Architecture
/// - Coroutines register themselves in a global queue on creation
/// - Each coroutine waits on its own wakeup semaphore
/// - Main thread's run() wakes the next coroutine and waits for it to finish
/// - Uses a single counting_semaphore<2> for the two-phase handoff (started + done)
///
/// ## Semaphore Protocol (only 2 semaphores):
/// - Per-context wakeup_semaphore (binary): main wakes coroutine
/// - Global main_thread_semaphore (counting<2>): coroutine signals main twice
///   1. release() = "I started"  -> main acquire() unblocks
///   2. release() = "I'm done"   -> main acquire() unblocks

#pragma once

// IWYU pragma: private

#ifdef FASTLED_STUB_IMPL

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "platforms/coroutine.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/semaphore.h"
#include "fl/stl/thread.h"
#include "fl/stl/chrono.h"
#include "fl/stl/string.h"
#include "fl/system/export.h"
// IWYU pragma: end_keep

namespace fl {
namespace detail {

//=============================================================================
// Coroutine Context — per-coroutine state
//=============================================================================

/// @brief Coroutine execution context interface
///
/// Each coroutine has its own wakeup semaphore.
/// The runner queue manages when each coroutine is allowed to run.
class CoroutineContext {
public:
    static fl::shared_ptr<CoroutineContext> create();

    virtual ~CoroutineContext() = default;

    /// @brief Wait until this coroutine is signaled to run
    virtual void wait() = 0;

    /// @brief Get this coroutine's wakeup semaphore
    virtual fl::binary_semaphore& wakeup_semaphore() = 0;

    /// @brief Check if coroutine should stop
    virtual bool should_stop() const = 0;

    /// @brief Set should_stop flag
    virtual void set_should_stop(bool value) = 0;

    /// @brief Check if coroutine is completed
    virtual bool is_completed() const = 0;

    /// @brief Set completed flag
    virtual void set_completed(bool value) = 0;

    /// @brief Check if coroutine thread is ready (reached the wait point)
    virtual bool is_thread_ready() const = 0;

    /// @brief Set thread ready flag (internal use by thread)
    virtual void set_thread_ready(bool value) = 0;
};

//=============================================================================
// Coroutine Runner — global queue-based scheduler
//=============================================================================

/// @brief Global coroutine runner queue interface
///
/// Manages FIFO queue of waiting coroutines and signals them in order.
/// Uses weak_ptr to avoid keeping contexts alive.
class CoroutineRunner {
public:
    virtual ~CoroutineRunner() = default;

    /// @brief Register a coroutine context in the queue
    virtual void enqueue(fl::shared_ptr<CoroutineContext> ctx) = 0;

    /// @brief Run coroutines for the specified duration
    virtual void run(fl::u32 us) = 0;

    /// @brief Stop a specific coroutine context
    virtual void stop(fl::shared_ptr<CoroutineContext> ctx) = 0;

    /// @brief Remove a context from the queue (for cleanup)
    virtual void remove(fl::shared_ptr<CoroutineContext> ctx) = 0;

    /// @brief Stop all coroutines
    virtual void stop_all() = 0;

    /// @brief Get singleton instance
    static CoroutineRunner& instance();

    /// @brief Get the main thread semaphore for two-phase handoff
    /// Coroutine releases twice (started + done), main acquires twice.
    virtual fl::counting_semaphore<2>& get_main_thread_semaphore() = 0;
};

} // namespace detail

namespace platforms {

//=============================================================================
// Coroutine Runtime — thread-based with sleep override
//=============================================================================

class CoroutineRuntimeStub : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) override {
        fl::detail::CoroutineRunner::instance().run(us);
    }

    void suspendMainthread(fl::u32 us) override {
        // Stub coroutines are std::threads — calling CoroutineRunner::run()
        // from a worker thread would deadlock on the two-phase semaphore handoff.
        // Instead, just sleep for the requested duration (safe from any thread).
        fl::this_thread::sleep_for(fl::chrono::microseconds(us));  // ok sleep for
    }
};

//=============================================================================
// Task Coroutine — std::thread-based
//=============================================================================

/// @brief Host-platform task coroutine interface using std::thread
///
/// This is an abstract interface - use create() factory method or
/// createTaskCoroutine() to instantiate.
///
/// The stub implementation uses std::thread for coroutine execution, providing
/// real concurrency for host-based unit tests without requiring FreeRTOS.
///
/// **Embedded Behavior**: Like embedded systems (ESP32/Arduino), coroutine threads
/// are "daemon" threads that don't block process exit. Threads are detached but
/// contexts are tracked via shared_ptr for optional cleanup.
class TaskCoroutineStub : public ICoroutineTask {
public:
    static TaskCoroutinePtr create(fl::string name,
                                    TaskFunction function,
                                    size_t stack_size = 4096,
                                    u8 priority = 5);

    ~TaskCoroutineStub() override = default;

    void stop() override = 0;
    bool isRunning() const override = 0;
};

//=============================================================================
// Global cleanup functions
//=============================================================================

/// @brief Clean up all coroutine and background threads before DLL unload
///
/// Joins all registered background threads and coroutine threads.
/// Must be called before DLL unload to prevent access violations.
void cleanup_coroutine_threads();

/// @brief Register a background thread for cleanup on exit
///
/// Threads registered here will be joined by cleanup_background_threads()
/// or cleanup_coroutine_threads().
void register_background_thread(fl::thread&& t);

/// @brief Join all registered background threads (not coroutine threads)
///
/// Safe to call between tests — does NOT touch coroutine state.
void cleanup_background_threads();

/// @brief Check if shutdown has been requested (stub platform)
///
/// Background threads should poll this and exit early when true.
/// Backed by atomic flag in BackgroundThreadRegistry.
bool is_shutdown_requested();

} // namespace platforms
} // namespace fl

#endif // FASTLED_STUB_IMPL
