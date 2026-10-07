// ok no header - public declarations remain in fl/task/executor.h

#include "fl/task/executor.h"
#include "fl/task/task_pump.h"
#include "fl/stl/atomic.h"
#include "fl/stl/functional.h"
#include "fl/stl/singleton.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/algorithm.h"
#include "fl/task/task.h"
#include "fl/stl/chrono.h"
#include "fl/log/log.h"

#include "fl/stl/new.h"
#include "fl/system/yield.h"
#include "platforms/coroutine_runtime.h"

namespace fl {
namespace task {

namespace detail {

/// @brief Get reference to thread-local await recursion depth
/// @return Reference to the thread-local await depth counter
int& await_depth_tls() {
    return SingletonThreadLocal<int>::instance();
}
} // namespace detail

Executor& Executor::instance() {
    Executor& executor = fl::Singleton<Executor>::instance();
    detail::set_executor_pump([]() {
        fl::Singleton<Executor>::instance().update_all();
    });
    return executor;
}

void Executor::register_runner(Runner* r) {
    if (r && fl::find(mRunners.begin(), mRunners.end(), r) == mRunners.end()) {
        mRunners.push_back(r);
    }
}

void Executor::unregister_runner(Runner* r) {
    auto it = fl::find(mRunners.begin(), mRunners.end(), r);
    if (it != mRunners.end()) {
        mRunners.erase(it);
    }
}

void Executor::update_all() {
    // Update all registered runners
    for (auto* r : mRunners) {
        if (r) {
            r->update();
        }
    }
}

bool Executor::has_active_tasks() const {
    for (const auto* r : mRunners) {
        if (r && r->has_active_tasks()) {
            return true;
        }
    }
    return false;
}

size_t Executor::total_active_tasks() const {
    size_t total = 0;
    for (const auto* r : mRunners) {
        if (r) {
            total += r->active_task_count();
        }
    }
    return total;
}

size_t active_tasks() {
    return Executor::instance().total_active_tasks();
}

bool has_tasks() {
    return Executor::instance().has_active_tasks();
}


} // namespace task
} // namespace fl
