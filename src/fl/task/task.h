#pragma once

/**
## Usage Example

```cpp
// IWYU pragma: begin_keep
#include <fl/fastled.h>
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
            FL_WARN("Fetch failed: " << result.error().message);
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

enum class TaskType {
    kEveryMs,
    kAtFramerate,
    kBeforeFrame,
    kAfterFrame,
    kCoroutine
};

// Forward declarations
class ITaskImpl;
class Coroutine;  // IWYU pragma: keep

/// @brief Configuration for OS-level coroutine tasks
struct CoroutineConfig {
    optional<TracePoint> trace;  // where this object was created, optional.
    function<void()> func;
    string name = "task";
    size_t stack_size = 4096;
    u8 priority = 5;
    optional<int> core_id;  ///< Pin task to specific CPU core (ESP32 and other dual cores in the future)
};

/// @brief Task Handle with fluent API (was class fl::task, renamed to avoid namespace collision)
class Handle {
public:

    // Default constructor
    Handle() FL_NOEXCEPT = default;

    // Copy and Move semantics (now possible with shared_ptr)
    Handle(const Handle&) FL_NOEXCEPT = default;
    Handle& operator=(const Handle&) FL_NOEXCEPT = default;
    Handle(Handle&&) FL_NOEXCEPT = default;
    Handle& operator=(Handle&&) FL_NOEXCEPT = default;

    // Constructor from impl - public because ITaskImpl is only forward-declared
    // in the header, making this effectively private to external consumers
    explicit Handle(shared_ptr<ITaskImpl> impl) FL_NOEXCEPT;

    // Fluent API
    Handle& then(function<void()> on_then) FL_NOEXCEPT;
    Handle& catch_(function<void(const Error&)> on_catch) FL_NOEXCEPT;
    Handle& cancel() FL_NOEXCEPT;

    // Getters
    int id() const FL_NOEXCEPT;
    bool has_then() const FL_NOEXCEPT;
    bool has_catch() const FL_NOEXCEPT;
    string trace_label() const FL_NOEXCEPT;
    TaskType type() const FL_NOEXCEPT;
    int interval_ms() const FL_NOEXCEPT;
    void set_interval_ms(int interval_ms) FL_NOEXCEPT;
    u32 last_run_time() const FL_NOEXCEPT;
    void set_last_run_time(u32 time) FL_NOEXCEPT;
    bool ready_to_run(u32 current_time) const FL_NOEXCEPT;
    bool is_valid() const FL_NOEXCEPT;
    bool isCoroutine() const FL_NOEXCEPT;

    // Coroutine control (only valid if isCoroutine() == true)
    void stop() FL_NOEXCEPT;
    bool isRunning() const FL_NOEXCEPT;

private:
    friend class Scheduler;

    // Internal methods for Scheduler (friend access only)
    void _set_id(int id) FL_NOEXCEPT;
    int _id() const FL_NOEXCEPT;
    bool _is_canceled() const FL_NOEXCEPT;
    bool _ready_to_run(u32 current_time) const FL_NOEXCEPT;
    bool _ready_to_run_frame_task(u32 current_time) const FL_NOEXCEPT;
    void _set_last_run_time(u32 time) FL_NOEXCEPT;
    bool _has_then() const FL_NOEXCEPT;
    void _execute_then() FL_NOEXCEPT;
    void _execute_catch(const Error& error) FL_NOEXCEPT;
    TaskType _type() const FL_NOEXCEPT;
    string _trace_label() const FL_NOEXCEPT;

    shared_ptr<ITaskImpl> mImpl;
};

// Free function builders (were static methods on class fl::task)
Handle every_ms(int interval_ms) FL_NOEXCEPT;
Handle every_ms(int interval_ms, const TracePoint& trace) FL_NOEXCEPT;

Handle at_framerate(int fps) FL_NOEXCEPT;
Handle at_framerate(int fps, const TracePoint& trace) FL_NOEXCEPT;

// For most cases you want after_frame() instead of before_frame(), unless you
// are doing operations that need to happen right before the frame is rendered.
// Most of the time for ui stuff (button clicks, etc) you want after_frame(), so it
// can be available for the next iteration of loop().
Handle before_frame() FL_NOEXCEPT;
Handle before_frame(const TracePoint& trace) FL_NOEXCEPT;

// Example: auto t = fl::task::after_frame().then([]() {...}
Handle after_frame() FL_NOEXCEPT;
Handle after_frame(const TracePoint& trace) FL_NOEXCEPT;
// Example: auto t = fl::task::after_frame([]() {...}
Handle after_frame(function<void()> on_then) FL_NOEXCEPT;
Handle after_frame(function<void()> on_then, const TracePoint& trace) FL_NOEXCEPT;
Handle coroutine(const CoroutineConfig& config) FL_NOEXCEPT;

// Static coroutine control
void exit_current() FL_NOEXCEPT;

/// @brief Internal RAII wrapper for OS-level tasks (implementation detail)
/// @note Users should use fl::task::coroutine() instead of Coroutine directly
class Coroutine;  // IWYU pragma: keep

} // namespace task
} // namespace fl
