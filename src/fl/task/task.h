#pragma once

/**
## Usage Example

```cpp
// IWYU pragma: begin_keep
#include <fl/system/fastled.h>
#include <fl/task/task.h>
// IWYU pragma: end_keep

void setup() {
    // Create a recurring task that runs every 100ms
    auto t = fl::task::every_ms(100, FL_TRACE)
        .then([]() {
            // Task logic here
        })
        .catch_([](const fl::task::Error& e) {
            // Error handling here
        });

    // Add task to scheduler
    fl::task::Scheduler::instance().add_task(t);
}

void loop() {
    // Execute ready tasks
    fl::task::Scheduler::instance().update();

    // Yield for other operations
    fl::task::run(1000);
}
```

## Coroutine (OS-Level Task Management)

This file also provides a LOW-LEVEL OS/RTOS task wrapper for hardware drivers
and platform-specific threading needs. This is separate from the high-level
task scheduler above.

### Coroutine Usage Example

```cpp
#include "fl/task/task.h"

void myTaskFunction() {
    while (true) {
        // Task work...
        if (shouldShutdown) {
            fl::task::exit_current();  // Self-delete
            // UNREACHABLE on ESP32
        }
    }
}

// Create and start OS-level task (RAII - auto-cleanup on destruction)
auto t = fl::task::coroutine({
    .func = myTaskFunction,
    .name = "MyTask"
});

// Task is automatically cleaned up when 't' goes out of scope
// Or manually: t.stop();
```

### Coroutine with Async/Await (ESP32 only)

On ESP32 platforms, coroutines can use `fl::task::await()` to efficiently block on
promises without busy-waiting. This provides zero-CPU-overhead asynchronous
operations perfect for network requests, timers, or sensor readings.

```cpp
#include "fl/task/task.h"
#include "fl/task/executor.h"

// Create a coroutine that performs sequential async operations
auto t = fl::task::coroutine({
    .func = []() {
        // Fetch data from an API (zero CPU usage while waiting)
        auto result = fl::task::await(fl::fetch_get("http://api.example.com/data"));

        if (result.ok()) {
            fl::string data = result.value().text();

            // Process the data
            process_data(data);
        } else {
            FL_WARN_F("Fetch failed: %s", result.error().message);
        }

        // Task completes and automatically cleans up
    },
    .name = "AsyncWorker"
});
```

**Platform support:**
- ESP32: FreeRTOS tasks via xTaskCreate/vTaskDelete + fl::task::await() support
- Host/Stub: std::thread + fl::task::await() support (for testing)
- Other platforms: Null implementation (no-op, fl::task::await() not available)
*/

// allow-include-after-namespace

#include "fl/stl/functional.h"  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "fl/system/trace.h"
#include "fl/task/promise.h"  // IWYU pragma: keep
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace task {

/// @brief Identifies how a scheduled task is triggered.
enum class TaskType {
    kEveryMs,      ///< Runs after a fixed time interval.
    kAtFramerate, ///< Runs at a fixed number of times per second.
    kBeforeFrame, ///< Runs immediately before each FastLED frame is rendered.
    kAfterFrame,  ///< Runs immediately after each FastLED frame is rendered.
    kCoroutine    ///< Runs as an operating-system or RTOS task.
};

// Forward declarations
class ITaskImpl;
class Coroutine;  // IWYU pragma: keep

/// @brief Function and platform tuning for an OS-level coroutine task.
/// @note Every field is copied when coroutine() is called, so the config object
/// may be destroyed as soon as that call returns.
struct CoroutineConfig {
    /// Optional location where the task was created, reported by
    /// Handle::trace_label(). Unset by default, in which case the label is empty.
    optional<TracePoint> trace;
    /// Function executed by the coroutine, copied at creation time. An empty
    /// function is legal: the coroutine starts, does nothing, and exits.
    function<void()> func;
    /// Diagnostic name reported by the platform. Defaults to "task".
    string name = "task";
    /// Requested stack size in bytes, default 4096. A platform may raise a
    /// smaller request to its own floor (ESP32 clamps up to 2048) and platforms
    /// that emulate coroutines may ignore the field entirely.
    size_t stack_size = 4096;
    /// Platform task priority relative to the platform idle priority, default 5.
    /// On ESP32 the FreeRTOS priority becomes `tskIDLE_PRIORITY + priority`.
    /// FastLED does not clamp this against the platform maximum.
    u8 priority = 5;
    /// Optional CPU core affinity, unset by default (no affinity). Only ESP32
    /// honors it; a value outside [0, FL_CPU_CORES) is silently downgraded to no
    /// affinity, and single-core or non-RTOS platforms ignore the field.
    optional<int> core_id;
};

/// @brief Task Handle with fluent API (was class fl::task, renamed to avoid namespace collision)
class Handle {
public:

    /// @brief Constructs an empty, invalid handle.
    Handle() FL_NO_EXCEPT = default;

    /// @brief Copies a handle, sharing ownership of the underlying task.
    Handle(const Handle&) FL_NO_EXCEPT = default;
    /// @brief Copies a handle, sharing ownership of the underlying task.
    Handle& operator=(const Handle&) FL_NO_EXCEPT = default;
    /// @brief Moves a handle without changing the underlying task.
    Handle(Handle&&) FL_NO_EXCEPT = default;
    /// @brief Moves a handle without changing the underlying task.
    Handle& operator=(Handle&&) FL_NO_EXCEPT = default;

    /// @brief Constructs a handle for an existing task implementation.
    /// @param impl Task implementation. Ownership is shared: this handle, every
    /// copy of it, and the entry Scheduler::add_task keeps all hold the
    /// implementation alive, and it is destroyed when the last of them goes away.
    /// Passing nullptr yields an invalid handle that behaves exactly like a
    /// default-constructed one: is_valid() returns false, every accessor returns
    /// a default value, every mutator is a no-op, and the scheduler refuses to
    /// register it.
    /// @note This is public only because ITaskImpl is forward-declared.
    explicit Handle(shared_ptr<ITaskImpl> impl) FL_NO_EXCEPT;

    /// @brief Sets the callback invoked when the task runs successfully.
    /// @param on_then Callback to invoke.
    /// @return This handle, for fluent configuration.
    Handle& then(function<void()> on_then) FL_NO_EXCEPT;
    /// @brief Sets the callback invoked when task execution reports an error.
    /// @param on_catch Callback to invoke with the reported error.
    /// @return This handle, for fluent configuration.
    Handle& catch_(function<void(const Error&)> on_catch) FL_NO_EXCEPT;
    /// @brief Prevents future scheduled executions of the task.
    /// @return This handle, for fluent configuration.
    Handle& cancel() FL_NO_EXCEPT;

    /// @brief Returns the task's unique identifier.
    int id() const FL_NO_EXCEPT;
    /// @brief Returns whether a success callback is configured.
    bool has_then() const FL_NO_EXCEPT;
    /// @brief Returns whether an error callback is configured.
    bool has_catch() const FL_NO_EXCEPT;
    /// @brief Returns the label of the task's creation trace, if available.
    string trace_label() const FL_NO_EXCEPT;
    /// @brief Returns the task's scheduling mode.
    TaskType type() const FL_NO_EXCEPT;
    /// @brief Returns the interval between runs, in milliseconds.
    int interval_ms() const FL_NO_EXCEPT;
    /// @brief Changes the interval between runs.
    /// @param interval_ms New interval in milliseconds.
    void set_interval_ms(int interval_ms) FL_NO_EXCEPT;
    /// @brief Returns the timestamp of the most recent run, in milliseconds.
    u32 last_run_time() const FL_NO_EXCEPT;
    /// @brief Records the timestamp of the most recent run.
    /// @param time Timestamp in milliseconds.
    void set_last_run_time(u32 time) FL_NO_EXCEPT;
    /// @brief Tests whether the task is ready at a given timestamp.
    /// @param current_time Current time in milliseconds.
    bool ready_to_run(u32 current_time) const FL_NO_EXCEPT;
    /// @brief Returns whether this handle refers to a task.
    bool is_valid() const FL_NO_EXCEPT;
    /// @brief Returns whether this handle refers to an OS-level coroutine.
    bool isCoroutine() const FL_NO_EXCEPT;

    /// @brief Stops the coroutine represented by this handle.
    /// @note This operation is only valid when isCoroutine() returns true.
    void stop() FL_NO_EXCEPT;
    /// @brief Returns whether the represented coroutine is currently running.
    /// @note This operation is only valid when isCoroutine() returns true.
    bool isRunning() const FL_NO_EXCEPT;

private:
    friend class Scheduler;

    // Internal methods for Scheduler (friend access only)
    void _set_id(int id) FL_NO_EXCEPT;
    int _id() const FL_NO_EXCEPT;
    bool _is_canceled() const FL_NO_EXCEPT;
    bool _ready_to_run(u32 current_time) const FL_NO_EXCEPT;
    bool _ready_to_run_frame_task(u32 current_time) const FL_NO_EXCEPT;
    void _set_last_run_time(u32 time) FL_NO_EXCEPT;
    bool _has_then() const FL_NO_EXCEPT;
    void _execute_then() FL_NO_EXCEPT;
    void _execute_catch(const Error& error) FL_NO_EXCEPT;
    TaskType _type() const FL_NO_EXCEPT;
    string _trace_label() const FL_NO_EXCEPT;

    shared_ptr<ITaskImpl> mImpl;
};

/// @brief Creates a task that runs at a fixed millisecond interval.
/// @param interval_ms Interval between runs.
/// @return A handle for configuring and controlling the task.
Handle every_ms(int interval_ms) FL_NO_EXCEPT;
/// @brief Creates a traced task that runs at a fixed millisecond interval.
/// @param interval_ms Interval between runs.
/// @param trace Location where the task was created.
/// @return A handle for configuring and controlling the task.
Handle every_ms(int interval_ms, const TracePoint& trace) FL_NO_EXCEPT;

/// @brief Creates a task that runs at a fixed frame rate.
/// @param fps Number of executions per second.
/// @return A handle for configuring and controlling the task.
Handle at_framerate(int fps) FL_NO_EXCEPT;
/// @brief Creates a traced task that runs at a fixed frame rate.
/// @param fps Number of executions per second.
/// @param trace Location where the task was created.
/// @return A handle for configuring and controlling the task.
Handle at_framerate(int fps, const TracePoint& trace) FL_NO_EXCEPT;

/// Frame tasks recur: every overload below schedules a task that runs once per
/// FastLED frame and keeps doing so until Handle::cancel() is called on it, so
/// keep the returned handle whenever the task will need to be canceled.

/// @brief Creates a task that runs before every FastLED frame until canceled.
/// @return A handle for configuring and controlling the task.
/// @note Prefer after_frame() unless work must happen immediately before rendering.
Handle before_frame() FL_NO_EXCEPT;
/// @brief Creates a traced task that runs before every FastLED frame until canceled.
/// @param trace Location where the task was created.
/// @return A handle for configuring and controlling the task.
Handle before_frame(const TracePoint& trace) FL_NO_EXCEPT;

/// @brief Creates a task that runs after every FastLED frame until canceled.
/// @return A handle for configuring and controlling the task.
/// @note UI work usually belongs after the frame so it is ready for the next loop.
Handle after_frame() FL_NO_EXCEPT;
/// @brief Creates a traced task that runs after every FastLED frame until canceled.
/// @param trace Location where the task was created.
/// @return A handle for configuring and controlling the task.
Handle after_frame(const TracePoint& trace) FL_NO_EXCEPT;
/// @brief Creates an after-frame task, running until canceled, with its success
/// callback already configured.
/// @param on_then Callback invoked after each FastLED frame.
/// @return A handle for controlling the task.
Handle after_frame(function<void()> on_then) FL_NO_EXCEPT;
/// @brief Creates a traced after-frame task, running until canceled, with its
/// success callback already configured.
/// @param on_then Callback invoked after each FastLED frame.
/// @param trace Location where the task was created.
/// @return A handle for controlling the task.
Handle after_frame(function<void()> on_then, const TracePoint& trace) FL_NO_EXCEPT;
/// @brief Creates and starts an OS-level coroutine task.
/// @param config Function and platform-specific task configuration.
/// @return A handle for controlling the coroutine.
Handle coroutine(const CoroutineConfig& config) FL_NO_EXCEPT;

/// @brief Terminates the currently executing coroutine.
/// @note This function does not return on platforms that support coroutines.
void exit_current() FL_NO_EXCEPT;

/// @brief Internal RAII wrapper for OS-level tasks (implementation detail)
/// @note Users should use fl::task::coroutine() instead of Coroutine directly
class Coroutine;  // IWYU pragma: keep

} // namespace task
} // namespace fl
