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
/// class MyAsyncRunner : public fl::AsyncRunner {
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
///     MyAsyncRunner* runner = new MyAsyncRunner();
///     fl::AsyncManager::instance().register_runner(runner);
///     
///     // Now your async tasks will be automatically updated during:
///     // - FastLED.show() calls (via engine events)
///     // - delay() calls on WASM platforms
///     // - Manual fl::asyncrun() calls
/// }
/// @endcode

#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/function.h"
#include "fl/ptr.h"

namespace fl {

// Forward declaration
class AsyncManager;

/// @brief Generic asynchronous task runner interface
class AsyncRunner {
public:
    virtual ~AsyncRunner() = default;
    
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
    void register_runner(AsyncRunner* runner);
    
    /// Unregister an async runner
    void unregister_runner(AsyncRunner* runner);
    
    /// Update all registered async runners
    void update_all();
    
    /// Check if there are any active async tasks
    bool has_active_tasks() const;
    
    /// Get total number of active tasks across all runners
    size_t total_active_tasks() const;

private:
    fl::vector<AsyncRunner*> mRunners;
};

/// @brief Run all registered async tasks once
/// 
/// This function updates all registered async runners (fetch, timers, etc.)
/// and is automatically called during:
/// - FastLED engine events (onEndFrame)
/// - delay() calls on WASM platforms (every 1ms)
/// - Manual calls for custom async pumping
///
/// @note This replaces the old fetch_update() function with a generic approach
void asyncrun();

/// @brief Get the number of active async tasks across all systems
/// @return Total number of active async tasks
size_t async_active_tasks();

/// @brief Check if any async systems have active tasks
/// @return True if any async tasks are running
bool async_has_tasks();

} // namespace fl 
