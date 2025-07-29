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

#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/function.h"
#include "fl/ptr.h"
#include "fl/variant.h"
#include "fl/promise.h"
#include "fl/promise_result.h"
#include "fl/singleton.h"
#include "fl/thread_local.h"

#include "fl/task.h"
#include "fl/time.h"

namespace fl {

// Forward declarations
class AsyncManager;

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
/// **SAFETY WARNING**: This function should ONLY be called from top-level contexts
/// like Arduino loop() function. Never call this from:
/// - Promise callbacks (.then, .catch_)
/// - Nested async operations  
/// - Interrupt handlers
/// - Library initialization code
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
    static fl::ThreadLocal<int> await_depth(0);
    if (await_depth.access() > 10) {
        return fl::result<T>(Error("await_top_level recursion limit exceeded - possible infinite loop"));
    }
    
    ++await_depth.access();
    
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
    
    --await_depth.access();
    
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

class Scheduler {
public:
    static Scheduler& instance();

    int add_task(task t);
    void update();
    
    // New methods for frame task handling
    void update_before_frame_tasks();
    void update_after_frame_tasks();
    
    // For testing: clear all tasks
    void clear_all_tasks() { mTasks.clear(); mNextTaskId = 1; }

private:
    friend class fl::Singleton<Scheduler>;
    Scheduler() : mTasks() {}

    void warn_no_then(int task_id, const fl::string& trace_label);
    void warn_no_catch(int task_id, const fl::string& trace_label, const Error& error);
    
    // Helper method for running specific task types
    void update_tasks_of_type(TaskType task_type);

    fl::vector<task> mTasks;
    int mNextTaskId = 1;
};

} // namespace fl 
