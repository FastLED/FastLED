#include "fl/async.h"
#include "fl/singleton.h"
#include "fl/algorithm.h"

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

} // namespace fl 
