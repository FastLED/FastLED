#pragma once

// IWYU pragma: private

/// @file coroutine_stub.hpp
/// @brief Host/Stub coroutine infrastructure — all implementations
///
/// Contains the semaphore-based coroutine runner, thread-based task
/// coroutine implementation, runtime singleton, and cleanup functions.

#ifdef FASTLED_STUB_IMPL

// IWYU pragma: begin_keep
#include "platforms/stub/coroutine_stub.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/queue.h"
#include "fl/stl/unique_ptr.h"
// IWYU pragma: end_keep

namespace fl {
namespace detail {

//=============================================================================
// CoroutineContextImpl
//=============================================================================

class CoroutineContextImpl : public CoroutineContext {
public:
    CoroutineContextImpl()
        : mWakeupSema(0), mShouldStop(false), mCompleted(false), mThreadReady(false) {}

    void wait() override {
        mWakeupSema.acquire();
    }

    fl::binary_semaphore& wakeup_semaphore() override {
        return mWakeupSema;
    }

    bool should_stop() const override { return mShouldStop.load(); }
    void set_should_stop(bool value) override { mShouldStop.store(value); }
    bool is_completed() const override { return mCompleted.load(); }
    void set_completed(bool value) override { mCompleted.store(value); }
    bool is_thread_ready() const override { return mThreadReady.load(); }
    void set_thread_ready(bool value) override { mThreadReady.store(value); }

private:
    fl::binary_semaphore mWakeupSema;
    fl::atomic<bool> mShouldStop;
    fl::atomic<bool> mCompleted;
    fl::atomic<bool> mThreadReady;
};

fl::shared_ptr<CoroutineContext> CoroutineContext::create() {
    return fl::make_shared<CoroutineContextImpl>();
}

//=============================================================================
// CoroutineRunnerImpl
//=============================================================================

class CoroutineRunnerImpl : public CoroutineRunner {
public:
    CoroutineRunnerImpl() : mMainThreadSema(0) {}

    fl::counting_semaphore<2>& get_main_thread_semaphore() override {
        return mMainThreadSema;
    }

    void enqueue(fl::shared_ptr<CoroutineContext> ctx) override {
        fl::unique_lock<fl::mutex> lock(mQueueMutex);
        mQueue.push(fl::weak_ptr<CoroutineContext>(ctx));
    }

    void stop(fl::shared_ptr<CoroutineContext> ctx) override {
        if (ctx) {
            ctx->set_should_stop(true);
            // Wake it so it can check should_stop and exit
            ctx->wakeup_semaphore().release();
        }
    }

    // Run the next coroutine using two-phase semaphore handoff.
    // Returns true if a job ran, false if queue is empty.
    bool run_next_job() {
        fl::shared_ptr<CoroutineContext> ctx;

        {
            fl::unique_lock<fl::mutex> lock(mQueueMutex);

            // Remove expired/completed coroutines from front
            while (!mQueue.empty()) {
                fl::shared_ptr<CoroutineContext> candidate = mQueue.front().lock();
                if (!candidate || candidate->is_completed()) {
                    mQueue.pop();
                } else {
                    break;
                }
            }

            if (mQueue.empty()) {
                return false;
            }

            ctx = mQueue.front().lock();
            if (!ctx) {
                mQueue.pop();
                return false;
            }
            mQueue.pop();
            // Re-enqueue at back for next cycle
            mQueue.push(fl::weak_ptr<CoroutineContext>(ctx));
        }

        // Two-phase handoff:
        // 1. Wake the coroutine
        ctx->wakeup_semaphore().release();

        // 2. Wait for "started" signal (first release from coroutine)
        mMainThreadSema.acquire();

        // 3. Wait for "done" signal (second release from coroutine)
        mMainThreadSema.acquire();

        return true;
    }

    void run(fl::u32 us) override {
        u32 begin = fl::millis();
        auto expired = [begin, us]() {
            u32 diff = fl::millis() - begin;
            return diff < us;
        };

        while(true) {
            bool more = run_next_job();
            if (!more || expired()) {
                break;
            }
        }
    }

    void remove(fl::shared_ptr<CoroutineContext> ctx) override {
        fl::queue<fl::weak_ptr<CoroutineContext>> temp;
        fl::unique_lock<fl::mutex> lock(mQueueMutex);
        while (!mQueue.empty()) {
            fl::weak_ptr<CoroutineContext> weak_front = mQueue.front();
            mQueue.pop();
            fl::shared_ptr<CoroutineContext> front = weak_front.lock();
            if (front && front.get() != ctx.get()) {
                temp.push(weak_front);
            }
        }
        mQueue = fl::move(temp);
    }

    void stop_all() override {
        fl::unique_lock<fl::mutex> lock(mQueueMutex);
        fl::queue<fl::weak_ptr<CoroutineContext>> temp = mQueue;
        while (!temp.empty()) {
            fl::weak_ptr<CoroutineContext> weak_ctx = temp.front();
            temp.pop();
            fl::shared_ptr<CoroutineContext> ctx = weak_ctx.lock();
            if (ctx) {
                ctx->set_should_stop(true);
                ctx->wakeup_semaphore().release();
            }
        }
        while (!mQueue.empty()) {
            mQueue.pop();
        }
    }

private:
    fl::mutex mQueueMutex;
    fl::queue<fl::weak_ptr<CoroutineContext>> mQueue;
    fl::counting_semaphore<2> mMainThreadSema;
};

CoroutineRunner& CoroutineRunner::instance() {
    return fl::Singleton<CoroutineRunnerImpl>::instance();
}

} // namespace detail

namespace platforms {

//=============================================================================
// Coroutine Runtime singleton
//=============================================================================

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeStub>::instance();
}

//=============================================================================
// Background thread registry
//=============================================================================

struct BackgroundThreadRegistry {
    fl::vector<fl::thread> threads;
    fl::mutex mtx;

    static BackgroundThreadRegistry& instance() {
        return fl::Singleton<BackgroundThreadRegistry>::instance();
    }
};

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

TaskCoroutinePtr TaskCoroutineStub::create(fl::string name,
                                            TaskFunction function,
                                            size_t stack_size,
                                            u8 priority) {
    return TaskCoroutinePtr(
        new TaskCoroutineStubImpl(fl::move(name), fl::move(function), stack_size, priority));  // ok bare allocation
}

TaskCoroutinePtr createTaskCoroutine(fl::string name,
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
