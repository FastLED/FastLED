#pragma once

/// @file fl/task/executor.h
/// @brief Task executor — runs registered task runners and manages the run loop
///
/// This module provides a unified system for managing background operations
/// across FastLED, including HTTP requests, timers, and other background tasks.
///
/// The executor integrates with FastLED's engine events and can be pumped
/// during delay() calls on WASM platforms for optimal responsiveness.
///
/// @section Usage
/// @code
/// #include "fl/task/executor.h"
///
/// // Create a custom runner
/// class MyRunner : public fl::task::Runner {
/// public:
///     void update() override {
///         // Process your tasks here
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
///     MyRunner* runner = new MyRunner();
///     fl::task::Executor::instance().register_runner(runner);
///
///     // Now your tasks will be automatically updated during:
///     // - FastLED.show() calls (via engine events)
///     // - delay() calls on WASM platforms
///     // - Manual fl::task::run() calls
/// }
/// @endcode

#include "fl/task/promise.h"
#include "fl/task/promise_result.h"
#include "fl/task/scheduler.h"
#include "fl/stl/singleton.h"
#include "fl/task/task.h"
#include "platforms/await.h"
#include "fl/stl/cstddef.h"     // for size_t
#include "fl/stl/stdint.h"      // for u8
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace task {

/// @brief Flags controlling which subsystems run() pumps
///
/// - TASKS:      Scheduler (fl::task timers) + Executor (fetch, HTTP server)
/// - COROUTINES: Platform coroutines (pumpCoroutines — ESP32: vTaskDelay,
///               Teensy: cooperative context switch, Stub: thread run)
/// - SYSTEM:     Pure OS-level yield (vTaskDelay(0) on ESP32, thread yield on host)
/// - ALL:        All subsystems (default)
enum class ExecFlags : u8 {
    TASKS      = 1 << 0,
    COROUTINES = 1 << 1,
    SYSTEM     = 1 << 2,
    ALL        = (1 << 0) | (1 << 1) | (1 << 2)
};

inline ExecFlags operator|(ExecFlags a, ExecFlags b) {
    return static_cast<ExecFlags>(static_cast<u8>(a) | static_cast<u8>(b));
}

inline bool operator&(ExecFlags a, ExecFlags b) {
    return (static_cast<u8>(a) & static_cast<u8>(b)) != 0;
}

namespace detail {
/// @brief Get reference to thread-local await recursion depth
/// @return Reference to the thread-local await depth counter (internal implementation detail)
int& await_depth_tls();
} // namespace detail

/// @brief Generic task runner interface
class Runner {
public:
    virtual ~Runner() FL_NOEXCEPT = default;

    /// Update this runner (called during task pumping)
    virtual void update() = 0;

    /// Check if this runner has active tasks
    virtual bool has_active_tasks() const = 0;

    /// Get number of active tasks (for debugging/monitoring)
    virtual size_t active_task_count() const = 0;
};

/// @brief Task executor (singleton) — manages registered runners
class Executor {
public:
    static Executor& instance();

    /// Register a runner
    void register_runner(Runner* r);

    /// Unregister a runner
    void unregister_runner(Runner* r);

    /// Update all registered runners
    void update_all();

    /// Check if there are any active tasks
    bool has_active_tasks() const;

    /// Get total number of active tasks across all runners
    size_t total_active_tasks() const;

private:
    fl::vector<Runner*> mRunners;
};

/// @brief Run selected task subsystems
///
/// Pumps the requested subsystems and yields for up to `microseconds`.
///
/// Usage:
/// - `run()` — pump ALL + yield for 1ms (default)
/// - `run(0)` — pump ALL, no yield
/// - `run(250, ExecFlags::SYSTEM)` — OS yield only (for DMA wait loops)
/// - `run(1000, ExecFlags::TASKS | ExecFlags::COROUTINES)` — skip OS yield
///
/// @param microseconds  Budget in microseconds (default 1000 = 1ms)
/// @param flags         Which subsystems to pump (default ALL)
void run(fl::u32 microseconds = 1000, ExecFlags flags = ExecFlags::ALL);



/// @brief Get the number of active tasks across all systems
/// @return Total number of active tasks
size_t active_tasks();

/// @brief Check if any systems have active tasks
/// @return True if any tasks are running
bool has_tasks();

/// @brief Synchronously wait for a promise to complete (ONLY safe in top-level contexts)
/// @tparam T The type of value the promise resolves to (automatically deduced)
/// @param p The promise to wait for
/// @return A promise_result containing either the resolved value T or an Error
///
/// This function blocks until the promise is either resolved or rejected,
/// then returns a promise_result that can be checked with ok() for success/failure.
/// While waiting, it continuously calls run(1000) to pump async tasks and yield appropriately.
///
/// **PERFORMANCE NOTE**: This function busy-waits (high CPU usage) while the promise
/// is pending. For coroutines on ESP32 or Host/Stub platforms, prefer fl::task::await()
/// which truly blocks the coroutine with zero CPU overhead.
///
/// **SAFETY WARNING**: This function should ONLY be called from top-level contexts
/// like Arduino loop() function. Never call this from:
/// - Promise callbacks (.then, .catch_)
/// - Nested async operations
/// - Interrupt handlers
/// - Library initialization code
/// - Coroutine contexts (use fl::task::await() instead)
///
/// The "_top_level" suffix emphasizes this safety requirement.
///
/// @section Usage
/// @code
/// auto p = fl::fetch_get("http://example.com");
/// auto result = fl::task::await_top_level(p);  // Type automatically deduced!
///
/// if (result.ok()) {
///     const Response& resp = result.value();
///     FL_WARN("Success: " << resp.text());
/// } else {
///     FL_WARN("Error: " << result.error().message);
/// }
/// @endcode
template<typename T>
PromiseResult<T> await_top_level(Promise<T> p) {
    // Handle invalid promises
    if (!p.valid()) {
        return PromiseResult<T>(Error("Invalid promise"));
    }

    // If already completed, return immediately
    if (p.is_completed()) {
        if (p.is_resolved()) {
            return PromiseResult<T>(p.value());
        } else {
            return PromiseResult<T>(p.error());
        }
    }

    // Track recursion depth to prevent infinite loops
    int& await_depth = detail::await_depth_tls();
    if (await_depth > 10) {
        return PromiseResult<T>(Error("await_top_level recursion limit exceeded - possible infinite loop"));
    }

    ++await_depth;

    // Wait for promise to complete while pumping async tasks
    int pump_count = 0;
    const int max_pump_iterations = 10000; // Safety limit

    while (!p.is_completed() && pump_count < max_pump_iterations) {
        // Update the promise first (in case it's not managed by async system)
        p.update();

        // Check if completed after update
        if (p.is_completed()) {
            break;
        }

        // Platform-agnostic async pump and yield
        run(1000);

        ++pump_count;
    }

    --await_depth;

    // Check for timeout
    if (pump_count >= max_pump_iterations) {
        return PromiseResult<T>(Error("await_top_level timeout - promise did not complete"));
    }

    // Return the result
    if (p.is_resolved()) {
        return PromiseResult<T>(p.value());
    } else {
        return PromiseResult<T>(p.error());
    }
}

} // namespace task
} // namespace fl

// ============================================================================
// Await API Comparison: await() vs await_top_level()
// ============================================================================
//
// FastLED provides two await functions for different contexts:
//
// 1. fl::task::await() - For coroutines (ESP32/Host only)
//    - Zero CPU overhead (true OS-level blocking)
//    - Must be called from fl::task::coroutine() context
//    - Suspends coroutine thread until promise completes
//    - Platform support: ESP32 (FreeRTOS), Host/Stub (std::thread)
//
// 2. fl::task::await_top_level() - For main loop/top-level code (All platforms)
//    - High CPU usage (busy-wait with run(1000))
//    - Safe to call from Arduino loop() or main()
//    - NOT safe in promise callbacks or nested async operations
//    - Platform support: All platforms
//
// **When to use which?**
// - In fl::task::coroutine(): Use fl::task::await() for zero CPU waste
// - In Arduino loop() or main(): Use fl::task::await_top_level()
// - Other platforms without coroutine support: Use fl::task::await_top_level()
//
// ============================================================================



namespace fl {
namespace task {

/// @brief Await promise completion in a coroutine (Trampoline to platform implementation)
/// @tparam T The type of value the promise resolves to
/// @param p The promise to await
/// @return A PromiseResult<T> containing either the resolved value or an error
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
///     .func = []() {
///         auto result = fl::task::await(fl::fetch_get("http://example.com"));
///         if (result.ok()) {
///             process(result.value());
///         }
///     }
/// });
/// @endcode
template<typename T>
inline PromiseResult<T> await(Promise<T> p) {
    return fl::platforms::await(p);
}

} // namespace task
} // namespace fl

