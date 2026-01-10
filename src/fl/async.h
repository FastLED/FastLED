#pragma once

/// @file async.h
/// @brief Generic asynchronous task management for FastLED
///
/// This module provides a unified system for managing asynchronous operations
/// across FastLED, including HTTP requests, timers, and other background tasks.
///
/// The async system integrates with FastLED's engine events and can be pumped
/// during delay() calls on WASM platforms for optimal responsiveness.
///
/// @section Usage
/// @code
/// #include "fl/async.h"
/// 
/// // Create a custom async runner
/// class Myasync_runner : public fl::async_runner {
/// public:
///     void update() override {
///         // Process your async tasks here
///         process_timers();
///         handle_network_events();
///     }
///     
///     bool has_active_tasks() const override {
///         return !mTimers.empty() || mNetworkActive;
///     }
///     
///     size_t active_task_count() const override {
///         return mTimers.size() + (mNetworkActive ? 1 : 0);
///     }
/// };
/// 
/// void setup() {
///     Myasync_runner* runner = new Myasync_runner();
///     fl::AsyncManager::instance().register_runner(runner);
///     
///     // Now your async tasks will be automatically updated during:
///     // - FastLED.show() calls (via engine events)
///     // - delay() calls on WASM platforms
///     // - Manual fl::async_run() calls
/// }
/// @endcode

#include "fl/promise.h"
#include "fl/promise_result.h"
#include "fl/singleton.h"
#include "fl/task.h"
#include "platforms/await.h"
#include "fl/stl/allocator.h"   // for allocator
#include "fl/stl/atomic.h"      // for atomic
#include "fl/stl/cstddef.h"     // for size_t
#include "fl/stl/string.h"      // for string
#include "fl/stl/vector.h"

namespace fl {

namespace detail {
/// @brief Get reference to thread-local await recursion depth
/// @return Reference to the thread-local await depth counter (internal implementation detail)
int& await_depth_tls();
} // namespace detail

/// @brief Generic asynchronous task runner interface
class async_runner {
public:
    virtual ~async_runner() = default;
    
    /// Update this async runner (called during async pumping)
    virtual void update() = 0;
    
    /// Check if this runner has active tasks
    virtual bool has_active_tasks() const = 0;
    
    /// Get number of active tasks (for debugging/monitoring)
    virtual size_t active_task_count() const = 0;
};

/// @brief Async task manager (singleton)
class AsyncManager {
public:
    static AsyncManager& instance();
    
    /// Register an async runner
    void register_runner(async_runner* runner);
    
    /// Unregister an async runner
    void unregister_runner(async_runner* runner);
    
    /// Update all registered async runners
    void update_all();
    
    /// Check if there are any active async tasks
    bool has_active_tasks() const;
    
    /// Get total number of active tasks across all runners
    size_t total_active_tasks() const;

private:
    fl::vector<async_runner*> mRunners;
};

/// @brief Platform-specific async yield function
/// 
/// This function pumps all async tasks and yields control appropriately for the platform:
/// - WASM: calls async_run() then emscripten_sleep(1) to yield to browser
/// - Other platforms: calls async_run() multiple times with simple yielding
/// 
/// This centralizes platform-specific async behavior instead of having #ifdef in generic code.
void async_yield();


/// @brief Run all registered async tasks once
/// 
/// This function updates all registered async runners (fetch, timers, etc.)
/// and is automatically called during:
/// - FastLED engine events (onEndFrame)
/// - delay() calls on WASM platforms (every 1ms)
/// - Manual calls for custom async pumping
///
/// @note This replaces the old fetch_update() function with a generic approach
void async_run();



/// @brief Get the number of active async tasks across all systems
/// @return Total number of active async tasks
size_t async_active_tasks();

/// @brief Check if any async systems have active tasks
/// @return True if any async tasks are running
bool async_has_tasks();

/// @brief Synchronously wait for a promise to complete (ONLY safe in top-level contexts)
/// @tparam T The type of value the promise resolves to (automatically deduced)
/// @param promise The promise to wait for
/// @return A PromiseResult containing either the resolved value T or an Error
///
/// This function blocks until the promise is either resolved or rejected,
/// then returns a PromiseResult that can be checked with ok() for success/failure.
/// While waiting, it continuously calls async_yield() to pump async tasks and yield appropriately.
///
/// **PERFORMANCE NOTE**: This function busy-waits (high CPU usage) while the promise
/// is pending. For coroutines on ESP32 or Host/Stub platforms, prefer fl::await()
/// which truly blocks the coroutine with zero CPU overhead.
///
/// **SAFETY WARNING**: This function should ONLY be called from top-level contexts
/// like Arduino loop() function. Never call this from:
/// - Promise callbacks (.then, .catch_)
/// - Nested async operations
/// - Interrupt handlers
/// - Library initialization code
/// - Coroutine contexts (use fl::await() instead)
///
/// The "_top_level" suffix emphasizes this safety requirement.
///
/// **Type Deduction**: The template parameter T is automatically deduced from the 
/// promise parameter, so you don't need to specify it explicitly.
///
/// @section Usage
/// @code
/// auto promise = fl::fetch_get("http://example.com");
/// auto result = fl::await_top_level(promise);  // Type automatically deduced!
/// 
/// if (result.ok()) {
///     const Response& resp = result.value();
///     FL_WARN("Success: " << resp.text());
/// } else {
///     FL_WARN("Error: " << result.error().message);
/// }
/// 
/// // Or use operator bool
/// if (result) {
///     FL_WARN("Got response: " << result.value().status());
/// }
/// 
/// // You can still specify the type explicitly if needed:
/// auto explicit_result = fl::await_top_level<response>(promise);
/// @endcode
template<typename T>
fl::result<T> await_top_level(fl::promise<T> promise) {
    // Handle invalid promises
    if (!promise.valid()) {
        return fl::result<T>(Error("Invalid promise"));
    }

    // If already completed, return immediately
    if (promise.is_completed()) {
        if (promise.is_resolved()) {
            return fl::result<T>(promise.value());
        } else {
            return fl::result<T>(promise.error());
        }
    }

    // Track recursion depth to prevent infinite loops
    int& await_depth = detail::await_depth_tls();
    if (await_depth > 10) {
        return fl::result<T>(Error("await_top_level recursion limit exceeded - possible infinite loop"));
    }

    ++await_depth;

    // Wait for promise to complete while pumping async tasks
    int pump_count = 0;
    const int max_pump_iterations = 10000; // Safety limit

    while (!promise.is_completed() && pump_count < max_pump_iterations) {
        // Update the promise first (in case it's not managed by async system)
        promise.update();

        // Check if completed after update
        if (promise.is_completed()) {
            break;
        }

        // Platform-agnostic async pump and yield
        async_yield();

        ++pump_count;
    }

    --await_depth;

    // Check for timeout
    if (pump_count >= max_pump_iterations) {
        return fl::result<T>(Error("await_top_level timeout - promise did not complete"));
    }

    // Return the result
    if (promise.is_resolved()) {
        return fl::result<T>(promise.value());
    } else {
        return fl::result<T>(promise.error());
    }
}

} // namespace fl

// ============================================================================
// Await API Comparison: await() vs await_top_level()
// ============================================================================
//
// FastLED provides two await functions for different contexts:
//
// 1. fl::await() - For coroutines (ESP32/Host only)
//    - Zero CPU overhead (true OS-level blocking)
//    - Must be called from fl::task::coroutine() context
//    - Suspends coroutine thread until promise completes
//    - Platform support: ESP32 (FreeRTOS), Host/Stub (std::thread)
//    - Example: See ESP32 and FASTLED_STUB_IMPL sections below
//
// 2. fl::await_top_level() - For main loop/top-level code (All platforms)
//    - High CPU usage (busy-wait with async_yield())
//    - Safe to call from Arduino loop() or main()
//    - NOT safe in promise callbacks or nested async operations
//    - Platform support: All platforms
//    - Example: See documentation above
//
// **When to use which?**
// - In fl::task::coroutine(): Use fl::await() for zero CPU waste
// - In Arduino loop() or main(): Use fl::await_top_level()
// - Other platforms without coroutine support: Use fl::await_top_level()
//
// ============================================================================
// Platform-Specific Await Implementations
// ============================================================================
//
// The platform-specific await() implementations are in platforms/await.h
// The public API fl::await() acts as a trampoline that delegates to
// fl::platforms::await() without polluting fl/ namespace with platform headers.
//
// Supported platforms (true OS-level blocking):
// - ESP32: FreeRTOS task notifications
// - Host/Stub: fl::condition_variable
//
// Unsupported platforms will get a static_assert error if await() is used.
// Use fl::await_top_level() instead on platforms without coroutine support.



namespace fl {

/// @brief Await promise completion in a coroutine (Trampoline to platform implementation)
/// @tparam T The type of value the promise resolves to
/// @param promise The promise to await
/// @return A result<T> containing either the resolved value or an error
///
/// This is the public API for awaiting promises in coroutines. It delegates to
/// the platform-specific implementation in fl::platforms::await().
///
/// **REQUIREMENTS**:
/// - MUST be called from within a fl::task::coroutine() context
/// - Only available on ESP32 (FreeRTOS) and Host/Stub (std::thread) platforms
/// - Promise MUST eventually complete (or timeout)
///
/// **SAFETY**: Unlike await_top_level(), this does NOT busy-wait. The
/// coroutine is suspended by the OS scheduler and wakes when the promise
/// completes, with zero CPU overhead.
///
/// @section Usage
/// @code
/// fl::task::coroutine({
///     .function = []() {
///         auto result = fl::await(fl::fetch_get("http://example.com"));
///         if (result.ok()) {
///             process(result.value());
///         }
///     }
/// });
/// @endcode
template<typename T>
inline fl::result<T> await(fl::promise<T> promise) {
    return fl::platforms::await(promise);
}

} // namespace fl

namespace fl {

class Scheduler {
public:
    static Scheduler& instance();

    int add_task(task t);
    void update();
    
    // New methods for frame task handling
    void update_before_frame_tasks();
    void update_after_frame_tasks();
    
    // For testing: clear all tasks
    void clear_all_tasks() { mTasks.clear(); mNextTaskId.store(1); }

private:
    friend class fl::Singleton<Scheduler>;
    Scheduler() : mTasks(), mNextTaskId(1) {}

    void warn_no_then(int task_id, const fl::string& trace_label);
    void warn_no_catch(int task_id, const fl::string& trace_label, const Error& error);
    
    // Helper method for running specific task types
    void update_tasks_of_type(TaskType task_type);

    fl::vector<task> mTasks;
    fl::atomic<int> mNextTaskId;
};

} // namespace fl 
