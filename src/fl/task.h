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
    fl::Scheduler::instance().add_task(fl::move(task));
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

namespace fl {

enum class TaskType {
    kEveryMs,
    kAtFramerate,
    kBeforeFrame,
    kAfterFrame
};

class task {
private:
    // Constructors
    task(TaskType type, int interval_ms);
    task(TaskType type, int interval_ms, const fl::TracePoint& trace);

    
    // Friend declaration to allow make_unique to access private constructors
    template<typename T, typename... Args>
    friend typename enable_if<!is_array<T>::value, unique_ptr<T>>::type 
    make_unique(Args&&... args);

public:
    // Copy and Move semantics
    task(const task&) = delete;
    task& operator=(const task&) = delete;

    task(task&& other) = default;
    task& operator=(task&& other) = default;

    // Static builders
    static unique_ptr<task> every_ms(int interval_ms);
    static unique_ptr<task> every_ms(int interval_ms, const fl::TracePoint& trace);

    static unique_ptr<task> at_framerate(int fps);
    static unique_ptr<task> at_framerate(int fps, const fl::TracePoint& trace);

    static unique_ptr<task> before_frame();
    static unique_ptr<task> before_frame(const fl::TracePoint& trace);

    static unique_ptr<task> after_frame();
    static unique_ptr<task> after_frame(const fl::TracePoint& trace);

    // Fluent API
    task& then(function<void()> on_then) &;
    task&& then(function<void()> on_then) &&;
    task& catch_(function<void(const Error&)> on_catch) &;
    task&& catch_(function<void(const Error&)> on_catch) &&;
    task& cancel() &;
    task&& cancel() &&;

    // Getters
    int id() const { return mTaskId; }
    bool has_then() const { return mHasThen; }
    bool has_catch() const { return mHasCatch; }
    string trace_label() const { return mTraceLabel? *mTraceLabel : ""; }
    TaskType type() const { return mType; }
    int interval_ms() const { return mIntervalMs; }
    uint32_t last_run_time() const { return mLastRunTime; }
    void set_last_run_time(uint32_t time) { mLastRunTime = time; }
    bool ready_to_run(uint32_t current_time) const;

private:
    friend class Scheduler;

    int mTaskId = 0;
    TaskType mType;
    int mIntervalMs;
    bool mCanceled = false;
    unique_ptr<string> mTraceLabel; // Optional trace label (default big so we put it in the heap)
    bool mHasThen = false;
    bool mHasCatch = false;
    uint32_t mLastRunTime = 0; // Last time the task was run

    function<void()> mThenCallback;
    function<void(const Error&)> mCatchCallback;
};

} // namespace fl
