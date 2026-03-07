/// @file coroutine_runner.h
/// @brief Queue-based coroutine runner for stub platform
///
/// **Architecture**:
/// - Coroutines register themselves in a global queue on creation
/// - Each coroutine waits on its own wakeup semaphore
/// - Main thread's run() wakes the next coroutine and waits for it to finish
/// - Uses a single counting_semaphore<2> for the two-phase handoff (started + done)
///
/// **Semaphore Protocol** (only 2 semaphores):
/// - Per-context wakeup_semaphore (binary): main wakes coroutine
/// - Global main_thread_semaphore (counting<2>): coroutine signals main twice
///   1. release() = "I started"  → main acquire() unblocks
///   2. release() = "I'm done"   → main acquire() unblocks

#pragma once

// IWYU pragma: private

#ifdef FASTLED_STUB_IMPL

#include "fl/stl/shared_ptr.h"
#include "fl/stl/semaphore.h"
#include "fl/export.h"

namespace fl {
namespace detail {

// Forward declarations for implementation types
class CoroutineContextImpl;
class CoroutineRunnerImpl;

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
} // namespace fl

#endif // FASTLED_STUB_IMPL
