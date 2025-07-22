#include "fl/async.h"
#include "fl/singleton.h"
#include "fl/algorithm.h"

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
    AsyncManager::instance().update_all();
}

size_t async_active_tasks() {
    return AsyncManager::instance().total_active_tasks();
}

bool async_has_tasks() {
    return AsyncManager::instance().has_active_tasks();
}

} // namespace fl 
