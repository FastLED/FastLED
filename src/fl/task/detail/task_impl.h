#pragma once

// IWYU pragma: private

#include "fl/task/task.h"

namespace fl {
namespace task {

class ITaskImpl {
public:
    virtual ~ITaskImpl() FL_NO_EXCEPT = default;
    virtual void set_then(function<void()> on_then) = 0;
    virtual void set_catch(function<void(const Error&)> on_catch) = 0;
    virtual void set_canceled() = 0;
    virtual int id() const = 0;
    virtual void set_id(int id) = 0;
    virtual bool has_then() const = 0;
    virtual bool has_catch() const = 0;
    virtual string trace_label() const = 0;
    virtual TaskType type() const = 0;
    virtual int interval_ms() const = 0;
    virtual void set_interval_ms(int interval_ms) = 0;
    virtual u32 last_run_time() const = 0;
    virtual void set_last_run_time(u32 time) = 0;
    virtual bool ready_to_run(u32 current_time) const = 0;
    virtual bool ready_to_run_frame_task(u32 current_time) const = 0;
    virtual bool is_canceled() const = 0;
    virtual bool is_auto_registered() const = 0;
    virtual void execute_then() = 0;
    virtual void execute_catch(const Error& error) = 0;
    virtual void auto_register_with_scheduler() = 0;

    // Coroutine methods (no-op for non-coroutine tasks)
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

namespace detail {

// One definition in the timer core keeps task IDs shared across task kinds.
int next_task_id();
string make_trace_label(const TracePoint& trace);

} // namespace detail
} // namespace task
} // namespace fl
