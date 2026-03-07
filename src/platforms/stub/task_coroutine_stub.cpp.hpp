// IWYU pragma: private

/// @file task_coroutine_stub.cpp
/// @brief Concrete implementation of TaskCoroutineStub using std::thread

#ifdef FASTLED_STUB_IMPL

#include "platforms/stub/task_coroutine_stub.h"
#include "platforms/coroutine_runtime.h"
#include "fl/stl/thread.h"  // stub platform only
#include "fl/stl/chrono.h"  // stub platform only
#include "fl/stl/unique_ptr.h"  // stub platform only
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "fl/singleton.h"
#include "platforms/stub/coroutine_runner.h"

namespace fl {
namespace platforms {

// Background thread registry (Singleton - survives static destruction)
// Separate from coroutine threads which have their own lifecycle.
struct BackgroundThreadRegistry {
    fl::vector<fl::thread> threads;
    fl::mutex mtx;

    static BackgroundThreadRegistry& instance() {
        return fl::Singleton<BackgroundThreadRegistry>::instance();
    }
};

// Coroutine thread registry (DLL mode only - need join before unload)
#ifdef TEST_DLL_MODE
struct CoroutineThreadRegistry {
    fl::vector<fl::thread> threads;
    fl::mutex mtx;

    static CoroutineThreadRegistry& instance() {
        return fl::Singleton<CoroutineThreadRegistry>::instance();
    }

    void add(fl::thread&& t) {
        fl::unique_lock<fl::mutex> lock(mtx);
        threads.push_back(fl::move(t));
    }
};
#endif

void cleanup_coroutine_threads() {
    fl::detail::CoroutineRunner::instance().stop_all();

    // Join coroutine threads (DLL mode only)
#ifdef TEST_DLL_MODE
    {
        auto& creg = CoroutineThreadRegistry::instance();
        fl::unique_lock<fl::mutex> lock(creg.mtx);
        for (auto& t : creg.threads) {
            if (t.joinable()) t.join();
        }
        creg.threads.clear();
    }
#endif

    // Join background threads (always)
    auto& bg = ICoroutineRuntime::instance();
    bg.requestShutdown();
    auto& reg = BackgroundThreadRegistry::instance();
    fl::unique_lock<fl::mutex> lock(reg.mtx);
    for (auto& t : reg.threads) {
        if (t.joinable()) t.join();
    }
    reg.threads.clear();
    bg.resetShutdown();
}

void register_background_thread(fl::thread&& t) {
    auto& reg = BackgroundThreadRegistry::instance();
    fl::unique_lock<fl::mutex> lock(reg.mtx);
    reg.threads.push_back(fl::move(t));
}

void cleanup_background_threads() {
    auto& bg = ICoroutineRuntime::instance();
    bg.requestShutdown();
    auto& reg = BackgroundThreadRegistry::instance();
    fl::unique_lock<fl::mutex> lock(reg.mtx);
    for (auto& t : reg.threads) {
        if (t.joinable()) t.join();
    }
    reg.threads.clear();
    bg.resetShutdown();
}

bool is_shutdown_requested() {
    return ICoroutineRuntime::instance().isShutdownRequested();
}

//=============================================================================
// TaskCoroutineStubImpl
//=============================================================================

class TaskCoroutineStubImpl : public TaskCoroutineStub {
public:
    TaskCoroutineStubImpl(fl::string name,
                          TaskFunction function,
                          size_t /*stack_size*/,
                          u8 /*priority*/)
        : mName(fl::move(name))
        , mFunction(fl::move(function)) {

        mContext = fl::detail::CoroutineContext::create();

        auto& runner = fl::detail::CoroutineRunner::instance();
        runner.enqueue(mContext);

        // Capture by value for thread safety
        TaskFunction func = mFunction;
        fl::shared_ptr<fl::detail::CoroutineContext> ctx_shared = mContext;

        fl::thread t([ctx_shared, func]() {
            // Mark ready and wait for wakeup
            ctx_shared->set_thread_ready(true);
            ctx_shared->wait();

            if (ctx_shared->should_stop()) {
                ctx_shared->set_completed(true);
                return;
            }

            auto& main_sema = fl::detail::CoroutineRunner::instance().get_main_thread_semaphore();

            // Phase 1: signal "started"
            main_sema.release();

            // Execute user function
            func();

            // Mark completed and signal "done"
            ctx_shared->set_completed(true);
            main_sema.release();
        });

#ifdef TEST_DLL_MODE
        CoroutineThreadRegistry::instance().add(fl::move(t));
#else
        t.detach();
#endif
    }

    ~TaskCoroutineStubImpl() override {
        stop();
    }

    void stop() override {
        if (!mContext) return;

        auto& runner = fl::detail::CoroutineRunner::instance();
        runner.stop(mContext);

        // Brief wait for thread to acknowledge stop
        fl::this_thread::sleep_for(fl::chrono::milliseconds(10)); // ok sleep for

        runner.remove(mContext);
        mContext.reset();
    }

    bool isRunning() const override {
        return mContext != nullptr;
    }

private:
    fl::shared_ptr<fl::detail::CoroutineContext> mContext;
    fl::string mName;
    TaskFunction mFunction;
};

//=============================================================================
// Factory methods
//=============================================================================

TaskCoroutineStub* TaskCoroutineStub::create(fl::string name,
                                              TaskFunction function,
                                              size_t stack_size,
                                              u8 priority) {
    return new TaskCoroutineStubImpl(fl::move(name), fl::move(function), stack_size, priority);  // ok bare allocation
}

ITaskCoroutine* createTaskCoroutine(fl::string name,
                                     ITaskCoroutine::TaskFunction function,
                                     size_t stack_size,
                                     u8 priority) {
    return TaskCoroutineStub::create(fl::move(name), fl::move(function), stack_size, priority);
}

void ITaskCoroutine::exitCurrent() {
    // On host, just return to end the thread function
}

} // namespace platforms
} // namespace fl

#endif // FASTLED_STUB_IMPL
