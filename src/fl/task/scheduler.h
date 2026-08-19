#pragma once

/// @file fl/task/scheduler.h
/// @brief Task scheduler — manages timer and frame-based tasks

#include "fl/task/task.h"
#include "fl/system/engine_events.h"
#include "fl/stl/singleton.h"
#include "fl/stl/atomic.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace task {

class Scheduler : private EngineEvents::Listener {
public:
    static Scheduler& instance();

    int add_task(Handle t);
    void update();

    // Dispatch the callbacks registered for one frame phase.
    void update_before_frame_tasks();
    void update_after_frame_tasks();

    // For testing: clear all tasks
    void clear_all_tasks() FL_NO_EXCEPT;

private:
    friend class fl::Singleton<Scheduler>;
    Scheduler() FL_NO_EXCEPT;
    ~Scheduler() FL_NO_EXCEPT override;

    void onBeginFrame() FL_NO_EXCEPT override;
    void onEndFrame() FL_NO_EXCEPT override;

    void warn_no_then(int task_id, const fl::string& trace_label);
    void warn_no_catch(int task_id, const fl::string& trace_label, const Error& error);

    // Helper method for running specific task types
    void update_tasks_of_type(TaskType task_type);
    void remove_inactive_tasks() FL_NO_EXCEPT;
    void update_frame_listener_registration() FL_NO_EXCEPT;

    fl::vector<Handle> mTasks;
    fl::vector<Handle> mBeforeFrameSnapshot;
    fl::vector<Handle> mAfterFrameSnapshot;
    fl::atomic<int> mNextTaskId;
    u32 mClearGeneration = 0;
    bool mUpdatingBeforeFrameTasks = false;
    bool mUpdatingAfterFrameTasks = false;
    bool mFrameListenerRegistered = false;
};

} // namespace task
} // namespace fl
