/// @file coroutine_runner.h
/// @brief Queue-based coroutine runner for stub platform
///
/// This provides a FIFO queue of coroutines that execute one at a time.
/// Each coroutine has its own condition variable and waits to be signaled.
/// When main thread yields, it signals the next coroutine in the queue.
///
/// **Architecture**:
/// - Coroutines register themselves in a global queue on creation
/// - Each coroutine waits on its own condition variable
/// - Main thread's async_yield() signals the next waiting coroutine
/// - When coroutine completes or awaits, it signals the next in queue
/// - Coroutines can be stopped/cleaned up by removing from queue
///
/// **Interface Design**:
/// - Uses virtual dispatch to hide concurrency primitives from header
/// - All implementation details (mutexes, condition variables) in .cpp file
/// - Header declares only abstract interfaces and factory functions

#pragma once

#ifdef FASTLED_STUB_IMPL

#include "fl/stl/shared_ptr.h"
#include "fl/export.h"

namespace fl {
namespace detail {

// Forward declarations for implementation types
class CoroutineContextImpl;
class CoroutineRunnerImpl;

/// @brief Coroutine execution context interface
///
/// Each coroutine has its own synchronization state.
/// The runner queue manages when each coroutine is allowed to run.
///
/// Note: No FASTLED_EXPORT - this is internal to stub platform, not part of public API
class CoroutineContext {
public:
    /// @brief Factory method to create a new coroutine context
    /// @return shared_ptr to allow shared ownership between queue and owner
    static fl::shared_ptr<CoroutineContext> create();

    virtual ~CoroutineContext() = default;
    /// @brief Wait until this coroutine is signaled to run
    virtual void wait() = 0;

    /// @brief Signal this coroutine to run
    virtual void signal() = 0;

    /// @brief Check if coroutine should stop
    virtual bool should_stop() const = 0;

    /// @brief Set should_stop flag
    virtual void set_should_stop(bool value) = 0;

    /// @brief Check if coroutine is completed
    virtual bool is_completed() const = 0;

    /// @brief Set completed flag
    virtual void set_completed(bool value) = 0;
};

/// @brief Global coroutine runner queue interface
///
/// Manages FIFO queue of waiting coroutines and signals them in order.
/// Uses weak_ptr to avoid keeping contexts alive - contexts are owned by their creators.
class CoroutineRunner {
public:
    virtual ~CoroutineRunner() = default;

    /// @brief Register a coroutine context in the queue
    /// @param ctx shared_ptr to the context (queue stores weak_ptr)
    virtual void enqueue(fl::shared_ptr<CoroutineContext> ctx) = 0;

    /// @brief Signal the next waiting coroutine to run
    virtual void signal_next() = 0;

    /// @brief Stop a specific coroutine context
    /// @param ctx shared_ptr to the context to stop
    virtual void stop(fl::shared_ptr<CoroutineContext> ctx) = 0;

    /// @brief Remove a context from the queue (for cleanup)
    /// @param ctx shared_ptr to the context to remove
    virtual void remove(fl::shared_ptr<CoroutineContext> ctx) = 0;

    /// @brief Stop all coroutines
    virtual void stop_all() = 0;

    /// @brief Get singleton instance
    static CoroutineRunner& instance();
};

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL
