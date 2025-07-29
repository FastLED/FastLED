#include "fl/async.h"
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

void AsyncManager::register_runner(AsyncRunner* runner) {
    if (runner && fl::find(mRunners.begin(), mRunners.end(), runner) == mRunners.end()) {
        mRunners.push_back(runner);
    }
}

void AsyncManager::unregister_runner(AsyncRunner* runner) {
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

void asyncrun() {
    fl::Scheduler::instance().update();
    AsyncManager::instance().update_all();
}

void async_yield() {
    // Always pump all async tasks first
    asyncrun();
    
    // Platform-specific yielding behavior
#ifdef __EMSCRIPTEN__
    // WASM: Use emscripten_sleep to yield control to browser event loop
    emscripten_sleep(1); // Sleep for 1ms to yield to browser
#endif
    for (int i = 0; i < 5; ++i) {
        asyncrun(); // Give other async tasks a chance
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

int Scheduler::add_task(fl::unique_ptr<task> t) {
    t->mTaskId = mNextTaskId++;
    mTasks.push_back(fl::move(t));
    return mTasks.back()->mTaskId;
}

void Scheduler::update() {
    uint32_t current_time = fl::time();
    
    // Use index-based iteration to avoid iterator invalidation issues
    for (fl::size i = 0; i < mTasks.size();) {
        fl::unique_ptr<task>& t_ptr = mTasks[i];
        task* t = t_ptr.get();

        if (t->mCanceled) {
            // erase() returns bool in HeapVector, not iterator
            mTasks.erase(mTasks.begin() + i);
            // Don't increment i since we just removed an element
        } else {
            // Check if task is ready to run
            bool should_run = t->ready_to_run(current_time);

            if (should_run) {
                // Update last run time for recurring tasks
                t->set_last_run_time(current_time);
                
                // Execute the task
                if (t->mThenCallback) {
                    t->mThenCallback();
                } else if (!t->has_then()) {
                    warn_no_then(t->id(), t->trace_label());
                }
                
                // Remove one-shot tasks, keep recurring ones
                bool is_recurring = (t->mType == TaskType::kEveryMs || t->mType == TaskType::kAtFramerate);
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
