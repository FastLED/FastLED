#pragma once

/// @file fl/task/scheduler.h
/// @brief Task scheduler — manages timer and frame-based tasks

#include "fl/task/task.h"
#include "fl/stl/singleton.h"
#include "fl/stl/atomic.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace task {

class Scheduler {
public:
    static Scheduler& instance();

    int add_task(Handle t);
    void update();

    // New methods for frame task handling
    void update_before_frame_tasks();
    void update_after_frame_tasks();

    // For testing: clear all tasks
    void clear_all_tasks() { mTasks.clear(); mNextTaskId.store(1); }

private:
    friend class fl::Singleton<Scheduler>;
    Scheduler() FL_NOEXCEPT : mTasks(), mNextTaskId(1) {}

    void warn_no_then(int task_id, const fl::string& trace_label);
    void warn_no_catch(int task_id, const fl::string& trace_label, const Error& error);

    // Helper method for running specific task types
    void update_tasks_of_type(TaskType task_type);

    fl::vector<Handle> mTasks;
    fl::atomic<int> mNextTaskId;
};

} // namespace task
} // namespace fl
