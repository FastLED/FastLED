#include "fl/task/scheduler.h"
#include "fl/stl/singleton.h"
#include "fl/stl/chrono.h"
#include "fl/log/log.h"

namespace fl {
namespace task {

// Scheduler implementation
Scheduler& Scheduler::instance() {
    return fl::Singleton<Scheduler>::instance();
}

int Scheduler::add_task(Handle t) {
    if (t.is_valid()) {
        int task_id = mNextTaskId.fetch_add(1);
        t._set_id(task_id);
        mTasks.push_back(fl::move(t));
        return task_id;
    }
    return 0; // Invalid task
}

void Scheduler::update() {
    u32 current_time = fl::millis();

    // Use index-based iteration to avoid iterator invalidation issues
    for (fl::size i = 0; i < mTasks.size();) {
        Handle& t = mTasks[i];

        if (!t.is_valid() || t._is_canceled()) {
            // erase() returns bool in fl::vector, not iterator
            mTasks.erase(mTasks.begin() + i);
            // Don't increment i since we just removed an element
        } else {
            // Check if task is ready to run (frame tasks will return false here)
            bool should_run = t._ready_to_run(current_time);

            if (should_run) {
                // Update last run time for recurring tasks
                t._set_last_run_time(current_time);

                // Execute the task
                if (t._has_then()) {
                    t._execute_then();
                } else {
                    warn_no_then(t._id(), t._trace_label());
                }

                // Remove one-shot tasks, keep recurring ones
                bool is_recurring = (t._type() == TaskType::kEveryMs || t._type() == TaskType::kAtFramerate);
                if (is_recurring) {
                    ++i; // Keep recurring tasks
                } else {
                    // erase() returns bool in fl::vector, not iterator
                    mTasks.erase(mTasks.begin() + i);
                    // Don't increment i since we just removed an element
                }
            } else {
                ++i;
            }
        }
    }
}

void Scheduler::update_before_frame_tasks() {
    update_tasks_of_type(TaskType::kBeforeFrame);
}

void Scheduler::update_after_frame_tasks() {
    update_tasks_of_type(TaskType::kAfterFrame);
}

void Scheduler::update_tasks_of_type(TaskType task_type) {
    u32 current_time = fl::millis();

    // Use index-based iteration to avoid iterator invalidation issues
    for (fl::size i = 0; i < mTasks.size();) {
        Handle& t = mTasks[i];

        if (!t.is_valid() || t._is_canceled()) {
            // erase() returns bool in fl::vector, not iterator
            mTasks.erase(mTasks.begin() + i);
            // Don't increment i since we just removed an element
        } else if (t._type() == task_type) {
            // This is a frame task of the type we're looking for
            bool should_run = t._ready_to_run_frame_task(current_time);

            if (should_run) {
                // Update last run time for frame tasks (though they don't use it)
                t._set_last_run_time(current_time);

                // Execute the task
                if (t._has_then()) {
                    t._execute_then();
                } else {
                    warn_no_then(t._id(), t._trace_label());
                }

                // Frame tasks are always one-shot, so remove them after execution
                mTasks.erase(mTasks.begin() + i);
                // Don't increment i since we just removed an element
            } else {
                ++i;
            }
        } else {
            ++i; // Not the task type we're looking for
        }
    }
}

void Scheduler::warn_no_then(int task_id, const fl::string& trace_label) {
    if (!trace_label.empty()) {
        FL_WARN(fl::string("[fl::task] Warning: no then() callback set for Task#") << task_id << " launched at " << trace_label);
    } else {
        FL_WARN(fl::string("[fl::task] Warning: no then() callback set for Task#") << task_id);
    }
}

void Scheduler::warn_no_catch(int task_id, const fl::string& trace_label, const Error& error) {
        if (!trace_label.empty()) {
        FL_WARN(fl::string("[fl::task] Warning: no catch_() callback set for Task#") << task_id << " launched at " << trace_label << ". Error: " << error.message);
    } else {
        FL_WARN(fl::string("[fl/task] Warning: no catch_() callback set for Task#") << task_id << ". Error: " << error.message);
    }
}

} // namespace task
} // namespace fl
