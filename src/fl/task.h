#pragma once

/**
## Usage Example

```cpp
#include <fl/fastled.h>
#include <fl/task.h>

void setup() {
    // Create a recurring task that runs every 100ms
    auto task = fl::task::every_ms(100, FL_TRACE)
        .then([]() {
            // Task logic here
        })
        .catch_([](const fl::Error& e) {
            // Error handling here
        });

    // Add task to scheduler
    fl::Scheduler::instance().add_task(task);
}

void loop() {
    // Execute ready tasks
    fl::Scheduler::instance().update();

    // Yield for other operations
    fl::async_yield();
}
```

## TaskCoroutine (OS-Level Task Management)

This file also provides a LOW-LEVEL OS/RTOS task wrapper for hardware drivers
and platform-specific threading needs. This is separate from the high-level
task scheduler above.

### TaskCoroutine Usage Example

```cpp
#include "fl/task.h"

void myTaskFunction() {
    while (true) {
        // Task work...
        if (shouldShutdown) {
            fl::task::exitCurrent();  // Self-delete
            // UNREACHABLE on ESP32
        }
    }
}

// Create and start OS-level task (RAII - auto-cleanup on destruction)
auto task = fl::task::coroutine({
    .function = myTaskFunction,
    .name = "MyTask"
});

// Task is automatically cleaned up when 'task' goes out of scope
// Or manually: task.stop();
```

### TaskCoroutine with Async/Await (ESP32 only)

On ESP32 platforms, coroutines can use `fl::await()` to efficiently block on
promises without busy-waiting. This provides zero-CPU-overhead asynchronous
operations perfect for network requests, timers, or sensor readings.

```cpp
#include "fl/task.h"
#include "fl/async.h"

// Create a coroutine that performs sequential async operations
auto task = fl::task::coroutine({
    .function = []() {
        // Fetch data from an API (zero CPU usage while waiting)
        auto result = fl::await(fl::fetch_get("http://api.example.com/data"));

        if (result.ok()) {
            fl::string data = result.value().text();

            // Process the data
            process_data(data);

            // Post results back (another zero-CPU await)
            auto post_result = fl::await(fl::fetch_post("http://api.example.com/results", data));

            if (post_result.ok()) {
                FL_DBG("Successfully posted results");
            } else {
                FL_WARN("Failed to post: " << post_result.error().message);
            }
        } else {
            FL_WARN("Fetch failed: " << result.error().message);
        }

        // Task completes and automatically cleans up
    },
    .name = "AsyncWorker"
});
```

**Platform support:**
- ESP32: FreeRTOS tasks via xTaskCreate/vTaskDelete + fl::await() support
- Host/Stub: std::thread + fl::await() support (for testing)
- Other platforms: Null implementation (no-op, fl::await() not available)
*/

// allow-include-after-namespace

#include "fl/stl/functional.h"
#include "fl/stl/string.h"
#include "fl/trace.h"
#include "fl/promise.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cstddef.h"

namespace fl {

enum class TaskType {
    kEveryMs,
    kAtFramerate,
    kBeforeFrame,
    kAfterFrame,
    kCoroutine
};

// Forward declarations
class ITaskImpl;
class TaskCoroutine;

/// @brief Configuration for OS-level coroutine tasks
struct CoroutineConfig {
    fl::function<void()> function;
    fl::string name = "task";
    size_t stack_size = 4096;
    uint8_t priority = 5;
    fl::optional<TracePoint> trace;
};

/// @brief Task handle with fluent API
class task {
public:


    // Default constructor
    task() = default;

    // Copy and Move semantics (now possible with shared_ptr)
    task(const task&) = default;
    task& operator=(const task&) = default;
    task(task&&) = default;
    task& operator=(task&&) = default;

    // Static builders
    static task every_ms(int interval_ms);
    static task every_ms(int interval_ms, const fl::TracePoint& trace);

    static task at_framerate(int fps);
    static task at_framerate(int fps, const fl::TracePoint& trace);

    // For most cases you want after_frame() instead of before_frame(), unless you
    // are doing operations that need to happen right before the frame is rendered.
    // Most of the time for ui stuff (button clicks, etc) you want after_frame(), so it
    // can be available for the next iteration of loop().
    static task before_frame();
    static task before_frame(const fl::TracePoint& trace);

    // Example: auto task = fl::task::after_frame().then([]() {...}
    static task after_frame();
    static task after_frame(const fl::TracePoint& trace);
    // Example: auto task = fl::task::after_frame([]() {...}
    static task after_frame(function<void()> on_then);
    static task after_frame(function<void()> on_then, const fl::TracePoint& trace);
    static task coroutine(const CoroutineConfig& config);

    // Fluent API
    task& then(function<void()> on_then);
    task& catch_(function<void(const Error&)> on_catch);
    task& cancel();

    // Getters
    int id() const;
    bool has_then() const;
    bool has_catch() const;
    string trace_label() const;
    TaskType type() const;
    int interval_ms() const;
    uint32_t last_run_time() const;
    void set_last_run_time(uint32_t time);
    bool ready_to_run(uint32_t current_time) const;
    bool is_valid() const;
    bool isCoroutine() const;

    // Coroutine control (only valid if isCoroutine() == true)
    void stop();
    bool isRunning() const;

    // Static coroutine control
    static void exitCurrent();

private:
    friend class Scheduler;

    explicit task(shared_ptr<ITaskImpl> impl);

    // Internal methods for Scheduler (friend access only)
    void _set_id(int id);
    int _id() const;
    bool _is_canceled() const;
    bool _ready_to_run(uint32_t current_time) const;
    bool _ready_to_run_frame_task(uint32_t current_time) const;
    void _set_last_run_time(uint32_t time);
    bool _has_then() const;
    void _execute_then();
    void _execute_catch(const Error& error);
    TaskType _type() const;
    string _trace_label() const;

    shared_ptr<ITaskImpl> mImpl;
};

/// @brief Internal RAII wrapper for OS-level tasks (implementation detail)
/// @note Users should use task::coroutine() instead of TaskCoroutine directly
class TaskCoroutine;

} // namespace fl
