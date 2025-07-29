#include "fl/async.h"
#include "fl/functional.h"
#include "fl/singleton.h"
#include "fl/algorithm.h"
#include "fl/task.h"
#include "fl/time.h"
#include "fl/warn.h"

// Platform-specific includes
#ifdef __EMSCRIPTEN__
extern "C" void emscripten_sleep(unsigned int ms);
#endif

namespace fl {

AsyncManager& AsyncManager::instance() {
    return fl::Singleton<AsyncManager>::instance();
}

void AsyncManager::register_runner(async_runner* runner) {
    if (runner && fl::find(mRunners.begin(), mRunners.end(), runner) == mRunners.end()) {
        mRunners.push_back(runner);
    }
}

void AsyncManager::unregister_runner(async_runner* runner) {
    auto it = fl::find(mRunners.begin(), mRunners.end(), runner);
    if (it != mRunners.end()) {
        mRunners.erase(it);
    }
}

void AsyncManager::update_all() {
    // Update all registered runners
    for (auto* runner : mRunners) {
        if (runner) {
            runner->update();
        }
    }
}

bool AsyncManager::has_active_tasks() const {
    for (const auto* runner : mRunners) {
        if (runner && runner->has_active_tasks()) {
            return true;
        }
    }
    return false;
}

size_t AsyncManager::total_active_tasks() const {
    size_t total = 0;
    for (const auto* runner : mRunners) {
        if (runner) {
            total += runner->active_task_count();
        }
    }
    return total;
}

// Public API functions

void async_run() {
    fl::Scheduler::instance().update();
    AsyncManager::instance().update_all();
}

void async_yield() {
    // Always pump all async tasks first
    async_run();
    
    // Platform-specific yielding behavior
#ifdef __EMSCRIPTEN__
    // WASM: Use emscripten_sleep to yield control to browser event loop
    emscripten_sleep(1); // Sleep for 1ms to yield to browser
#endif
    for (int i = 0; i < 5; ++i) {
        async_run(); // Give other async tasks a chance
    }
}

size_t async_active_tasks() {
    return AsyncManager::instance().total_active_tasks();
}

bool async_has_tasks() {
    return AsyncManager::instance().has_active_tasks();
}

// Scheduler implementation
Scheduler& Scheduler::instance() {
    return fl::Singleton<Scheduler>::instance();
}

int Scheduler::add_task(task t) {
    if (t.get_impl()) {
        t.get_impl()->mTaskId = mNextTaskId++;
        int task_id = t.get_impl()->mTaskId;
        mTasks.push_back(fl::move(t));
        return task_id;
    }
    return 0; // Invalid task
}

void Scheduler::update() {
    uint32_t current_time = fl::time();
    
    // Use index-based iteration to avoid iterator invalidation issues
    for (fl::size i = 0; i < mTasks.size();) {
        task& t = mTasks[i];
        auto impl = t.get_impl();
        
        if (!impl || impl->is_canceled()) {
            // erase() returns bool in HeapVector, not iterator
            mTasks.erase(mTasks.begin() + i);
            // Don't increment i since we just removed an element
        } else {
            // Check if task is ready to run (frame tasks will return false here)
            bool should_run = impl->ready_to_run(current_time);

            if (should_run) {
                // Update last run time for recurring tasks
                impl->set_last_run_time(current_time);
                
                // Execute the task
                if (impl->has_then()) {
                    impl->execute_then();
                } else {
                    warn_no_then(impl->id(), impl->trace_label());
                }
                
                // Remove one-shot tasks, keep recurring ones
                bool is_recurring = (impl->type() == TaskType::kEveryMs || impl->type() == TaskType::kAtFramerate);
                if (is_recurring) {
                    ++i; // Keep recurring tasks
                } else {
                    // erase() returns bool in HeapVector, not iterator
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
    uint32_t current_time = fl::time();
    
    // Use index-based iteration to avoid iterator invalidation issues
    for (fl::size i = 0; i < mTasks.size();) {
        task& t = mTasks[i];
        auto impl = t.get_impl();
        
        if (!impl || impl->is_canceled()) {
            // erase() returns bool in HeapVector, not iterator
            mTasks.erase(mTasks.begin() + i);
            // Don't increment i since we just removed an element
        } else if (impl->type() == task_type) {
            // This is a frame task of the type we're looking for
            bool should_run = impl->ready_to_run_frame_task(current_time);

            if (should_run) {
                // Update last run time for frame tasks (though they don't use it)
                impl->set_last_run_time(current_time);
                
                // Execute the task
                if (impl->has_then()) {
                    impl->execute_then();
                } else {
                    warn_no_then(impl->id(), impl->trace_label());
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

} // namespace fl 
