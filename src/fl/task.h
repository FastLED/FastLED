#pragma once

/**
## Usage Example

```cpp
#include <FastLED.h>
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
*/

#include "fl/functional.h"
#include "fl/string.h"
#include "fl/trace.h"
#include "fl/promise.h"
#include "fl/time.h"
#include "fl/ptr.h"

namespace fl {

enum class TaskType {
    kEveryMs,
    kAtFramerate,
    kBeforeFrame,
    kAfterFrame
};

// Forward declaration
class TaskImpl;

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
    bool is_valid() const { return mImpl != nullptr; }

private:
    friend class Scheduler;
    
    // Private constructor for internal use
    explicit task(shared_ptr<TaskImpl> impl);
    
    // Access to implementation for Scheduler
    shared_ptr<TaskImpl> get_impl() const { return mImpl; }

    shared_ptr<TaskImpl> mImpl;
};

// Private implementation class
class TaskImpl {
private:
    // Constructors
    TaskImpl(TaskType type, int interval_ms);
    TaskImpl(TaskType type, int interval_ms, const fl::TracePoint& trace);

    // Friend declaration to allow make_shared to access private constructors
    template<typename T, typename... Args>
    friend shared_ptr<T> make_shared(Args&&... args);

public:
    // Non-copyable but moveable
    TaskImpl(const TaskImpl&) = delete;
    TaskImpl& operator=(const TaskImpl&) = delete;
    TaskImpl(TaskImpl&&) = default;
    TaskImpl& operator=(TaskImpl&&) = default;

    // Static builders for internal use
    static shared_ptr<TaskImpl> create_every_ms(int interval_ms);
    static shared_ptr<TaskImpl> create_every_ms(int interval_ms, const fl::TracePoint& trace);
    static shared_ptr<TaskImpl> create_at_framerate(int fps);
    static shared_ptr<TaskImpl> create_at_framerate(int fps, const fl::TracePoint& trace);
    static shared_ptr<TaskImpl> create_before_frame();
    static shared_ptr<TaskImpl> create_before_frame(const fl::TracePoint& trace);
    static shared_ptr<TaskImpl> create_after_frame();
    static shared_ptr<TaskImpl> create_after_frame(const fl::TracePoint& trace);

    // Fluent API
    void set_then(function<void()> on_then);
    void set_catch(function<void(const Error&)> on_catch);
    void set_canceled();

    // Getters
    int id() const { return mTaskId; }
    bool has_then() const { return mHasThen; }
    bool has_catch() const { return mHasCatch; }
    string trace_label() const { return mTraceLabel ? *mTraceLabel : ""; }
    TaskType type() const { return mType; }
    int interval_ms() const { return mIntervalMs; }
    uint32_t last_run_time() const { return mLastRunTime; }
    void set_last_run_time(uint32_t time) { mLastRunTime = time; }
    bool ready_to_run(uint32_t current_time) const;
    bool ready_to_run_frame_task(uint32_t current_time) const;  // New method for frame tasks
    bool is_canceled() const { return mCanceled; }
    bool is_auto_registered() const { return mAutoRegistered; }

    // Execution
    void execute_then();
    void execute_catch(const Error& error);

private:
    friend class Scheduler;
    friend class task;

    // Auto-registration with scheduler
    void auto_register_with_scheduler();

    int mTaskId = 0;
    TaskType mType;
    int mIntervalMs;
    bool mCanceled = false;
    bool mAutoRegistered = false; // Track if task auto-registered with scheduler
    unique_ptr<string> mTraceLabel; // Optional trace label (default big so we put it in the heap)
    bool mHasThen = false;
    bool mHasCatch = false;
    uint32_t mLastRunTime = 0; // Last time the task was run

    function<void()> mThenCallback;
    function<void(const Error&)> mCatchCallback;
};

} // namespace fl
