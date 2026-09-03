#include "fl/task/scheduler.h"
#include "fl/stl/singleton.h"
#include "fl/stl/chrono.h"
#include "fl/log/log.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep - FL_UNUSED

namespace fl {
namespace task {

Scheduler::Scheduler() FL_NO_EXCEPT
    : mTasks(), mBeforeFrameSnapshot(), mAfterFrameSnapshot(), mNextTaskId(1) {}

Scheduler::~Scheduler() FL_NO_EXCEPT {
    if (mFrameListenerRegistered) {
        EngineEvents::setFrameTaskListener(nullptr);
    }
}

void Scheduler::onBeginFrame() FL_NO_EXCEPT {
    update_before_frame_tasks();
}

void Scheduler::onEndFrame() FL_NO_EXCEPT { update_after_frame_tasks(); }

// Scheduler implementation
Scheduler& Scheduler::instance() {
    return fl::Singleton<Scheduler>::instance();
}

int Scheduler::add_task(Handle t) {
    if (t.is_valid()) {
        for (const Handle& existing : mTasks) {
            if (existing.mImpl == t.mImpl) {
                return existing._id();
            }
        }
        int task_id = mNextTaskId.fetch_add(1);
        t._set_id(task_id);
        const bool is_frame_task = t._type() == TaskType::kBeforeFrame ||
                                   t._type() == TaskType::kAfterFrame;
        mTasks.push_back(fl::move(t));
        if (is_frame_task && !mFrameListenerRegistered) {
            // The dedicated final listener works even where the general
            // EngineEvents listener list is disabled for memory reasons.
            EngineEvents::setFrameTaskListener(this);
            mFrameListenerRegistered = true;
        }
        return task_id;
    }
    return 0; // Invalid task
}

void Scheduler::clear_all_tasks() FL_NO_EXCEPT {
    mTasks.clear();
    mNextTaskId.store(1);
    ++mClearGeneration;
    if (mFrameListenerRegistered) {
        EngineEvents::setFrameTaskListener(nullptr);
        mFrameListenerRegistered = false;
    }
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
    update_frame_listener_registration();
}

void Scheduler::update_before_frame_tasks() {
    if (mUpdatingBeforeFrameTasks) {
        return;
    }
    mUpdatingBeforeFrameTasks = true;
    update_tasks_of_type(TaskType::kBeforeFrame);
    mUpdatingBeforeFrameTasks = false;
}

void Scheduler::update_after_frame_tasks() {
    if (mUpdatingAfterFrameTasks) {
        return;
    }
    mUpdatingAfterFrameTasks = true;
    update_tasks_of_type(TaskType::kAfterFrame);
    mUpdatingAfterFrameTasks = false;
}

void Scheduler::update_tasks_of_type(TaskType task_type) {
    u32 current_time = fl::millis();

    remove_inactive_tasks();

    // Stable handles isolate dispatch from callbacks that mutate mTasks.
    // Separate reusable buffers preserve before/after nesting without a heap
    // allocation on every frame after each buffer reaches its high-water mark.
    fl::vector<Handle>& snapshot = task_type == TaskType::kBeforeFrame
                                       ? mBeforeFrameSnapshot
                                       : mAfterFrameSnapshot;
    snapshot.clear();
    for (const Handle& task : mTasks) {
        if (task._type() == task_type &&
            task._ready_to_run_frame_task(current_time)) {
            snapshot.push_back(task);
        }
    }

    const u32 clear_generation = mClearGeneration;
    for (Handle& task : snapshot) {
        if (mClearGeneration != clear_generation) {
            break;
        }
        if (!task.is_valid() || task._is_canceled()) {
            continue;
        }

        task._set_last_run_time(current_time);
        if (task._has_then()) {
            task._execute_then();
        } else {
            warn_no_then(task._id(), task._trace_label());
        }
    }
    snapshot.clear();
    remove_inactive_tasks();
    update_frame_listener_registration();
}

void Scheduler::remove_inactive_tasks() FL_NO_EXCEPT {
    for (fl::size i = 0; i < mTasks.size();) {
        if (!mTasks[i].is_valid() || mTasks[i]._is_canceled()) {
            mTasks.erase(mTasks.begin() + i);
        } else {
            ++i;
        }
    }
}

void Scheduler::update_frame_listener_registration() FL_NO_EXCEPT {
    if (!mFrameListenerRegistered) {
        return;
    }
    for (const Handle& task : mTasks) {
        const TaskType type = task._type();
        const bool is_frame_task = type == TaskType::kBeforeFrame ||
                                   type == TaskType::kAfterFrame;
        if (task.is_valid() && !task._is_canceled() && is_frame_task) {
            return;
        }
    }
    EngineEvents::setFrameTaskListener(nullptr);
    mFrameListenerRegistered = false;
}

void Scheduler::warn_no_then(int task_id, const fl::string& trace_label) {
    FL_UNUSED(task_id);  // only consumed by FL_WARN_F, a no-op on small platforms
    if (!trace_label.empty()) {
        FL_WARN_F("%s%s launched at %s", fl::string("[fl::task] Warning: no then() callback set for Task#"), task_id, trace_label);
    } else {
        FL_WARN_F("%s%s", fl::string("[fl::task] Warning: no then() callback set for Task#"), task_id);
    }
}

void Scheduler::warn_no_catch(int task_id, const fl::string& trace_label, const Error& error) {
    FL_UNUSED(task_id);  // only consumed by FL_WARN_F, a no-op on small platforms
    FL_UNUSED(error);
    if (!trace_label.empty()) {
        FL_WARN_F("%s%s launched at %s. Error: %s", fl::string("[fl::task] Warning: no catch_() callback set for Task#"), task_id, trace_label, error.message);
    } else {
        FL_WARN_F("%s%s. Error: %s", fl::string("[fl/task] Warning: no catch_() callback set for Task#"), task_id, error.message);
    }
}

} // namespace task
} // namespace fl
