#include "fl/async.h"
#include "fl/stl/functional.h"
#include "fl/singleton.h"
#include "fl/thread_local.h"
#include "fl/stl/algorithm.h"
#include "fl/task.h"
#include "fl/stl/time.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/thread_local.h"

// Platform-specific includes
#ifdef __EMSCRIPTEN__
extern "C" void emscripten_sleep(unsigned int ms);
#endif

#ifdef FASTLED_STUB_IMPL
#include "platforms/stub/coroutine_runner.h"  // Global coordination for stub platform
#include <thread>  // ok include (for std::this_thread::yield)
#endif

namespace fl {

namespace detail {


/// @brief Get reference to thread-local await recursion depth
/// @return Reference to the thread-local await depth counter
int& await_depth_tls() {
    static fl::ThreadLocal<int> s_await_depth(0);
    return s_await_depth.access();
}
} // namespace detail

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
    // FL_DBG("async_yield: Called from main thread");
    // Always pump all async tasks first
    async_run();

    // Platform-specific yielding behavior
#ifdef __EMSCRIPTEN__
    // WASM worker thread mode (PROXY_TO_PTHREAD): No explicit sleep needed.
    // The OS scheduler naturally handles thread yielding. Busy-waiting here
    // doesn't block the browser UI since we're on a background thread.
#endif

#ifdef FASTLED_STUB_IMPL
    // Only release lock if we actually hold it (prevents undefined behavior)
    // This handles the case where main thread calls async_yield() for the first
    // time before it has acquired the lock.
    if (fl::detail::global_execution_is_held()) {
        fl::detail::global_execution_unlock();
    }

    // Signal next coroutine in executor queue to run
    auto& runner = fl::detail::CoroutineRunner::instance();
    // FL_DBG("async_yield: Calling signal_next()");
    runner.signal_next();
    // FL_DBG("async_yield: signal_next() returned");

    // Yield CPU to allow coroutine threads to actually execute
    std::this_thread::yield();  // okay std namespace
    std::this_thread::sleep_for(std::chrono::milliseconds(1));  // okay std namespace

    // Always re-acquire the global execution lock
    // This ensures the caller holds the lock when we return
    fl::detail::global_execution_lock();
#endif

    // Standard async pumping (no global lock management here)
    for (int i = 0; i < 5; ++i) {
        async_run(); // Give other async tasks a chance
    }
    // FL_DBG("async_yield: Returning");
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
    if (t.is_valid()) {
        int task_id = mNextTaskId.fetch_add(1);
        t._set_id(task_id);
        mTasks.push_back(fl::move(t));
        return task_id;
    }
    return 0; // Invalid task
}

void Scheduler::update() {
    uint32_t current_time = fl::millis();
    
    // Use index-based iteration to avoid iterator invalidation issues
    for (fl::size i = 0; i < mTasks.size();) {
        task& t = mTasks[i];

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
    uint32_t current_time = fl::millis();

    // Use index-based iteration to avoid iterator invalidation issues
    for (fl::size i = 0; i < mTasks.size();) {
        task& t = mTasks[i];

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

} // namespace fl 
