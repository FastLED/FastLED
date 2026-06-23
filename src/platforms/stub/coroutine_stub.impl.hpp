#pragma once

// IWYU pragma: private

/// @file coroutine_stub.impl.hpp
/// @brief Host/Stub coroutine infrastructure — all implementations
///
/// Contains the semaphore-based coroutine runner, thread-based task
/// coroutine implementation, runtime singleton, and cleanup functions.

#ifdef FASTLED_STUB_IMPL

// IWYU pragma: begin_keep
#include "platforms/stub/coroutine_stub.h"
#include "fl/stl/singleton.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/queue.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep

namespace fl {
namespace detail {

//=============================================================================
// CoroutineContextImpl
//=============================================================================

class CoroutineContextImpl : public CoroutineContext {
public:
    CoroutineContextImpl()
 FL_NOEXCEPT : mWakeupSema(0), mShouldStop(false), mCompleted(false), mThreadReady(false) {}

    void wait() FL_NOEXCEPT override {
        mWakeupSema.acquire();
    }

    fl::binary_semaphore& wakeup_semaphore() FL_NOEXCEPT override {
        return mWakeupSema;
    }

    bool should_stop() const FL_NOEXCEPT override { return mShouldStop.load(); }
    void set_should_stop(bool value) FL_NOEXCEPT override { mShouldStop.store(value); }
    bool is_completed() const FL_NOEXCEPT override { return mCompleted.load(); }
    void set_completed(bool value) FL_NOEXCEPT override { mCompleted.store(value); }
    bool is_thread_ready() const FL_NOEXCEPT override { return mThreadReady.load(); }
    void set_thread_ready(bool value) FL_NOEXCEPT override { mThreadReady.store(value); }

private:
    fl::binary_semaphore mWakeupSema;
    fl::atomic<bool> mShouldStop;
    fl::atomic<bool> mCompleted;
    fl::atomic<bool> mThreadReady;
};

fl::shared_ptr<CoroutineContext> CoroutineContext::create() FL_NOEXCEPT {
    return fl::make_shared<CoroutineContextImpl>();
}

//=============================================================================
// CoroutineRunnerImpl
//=============================================================================

class CoroutineRunnerImpl : public CoroutineRunner {
public:
    CoroutineRunnerImpl() FL_NOEXCEPT : mMainThreadSema(0) {}

    fl::counting_semaphore<2>& get_main_thread_semaphore() FL_NOEXCEPT override {
        return mMainThreadSema;
    }

    void enqueue(fl::shared_ptr<CoroutineContext> ctx) FL_NOEXCEPT override {
        fl::unique_lock<fl::mutex> lock(mQueueMutex) FL_NOEXCEPT;
        mQueue.push(fl::weak_ptr<CoroutineContext>(ctx));
    }

    void stop(fl::shared_ptr<CoroutineContext> ctx) FL_NOEXCEPT override {
        if (ctx) {
            ctx->set_should_stop(true);
            // Wake it so it can check should_stop and exit
            ctx->wakeup_semaphore().release();
        }
    }

    // Run the next coroutine using two-phase semaphore handoff.
    // Returns true if a job ran, false if queue is empty.
    bool run_next_job() FL_NOEXCEPT {
        fl::shared_ptr<CoroutineContext> ctx;

        {
            fl::unique_lock<fl::mutex> lock(mQueueMutex) FL_NOEXCEPT;

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

    void run(fl::u32 us) FL_NOEXCEPT override {
        u32 begin = fl::millis();
        auto expired = [begin, us]() FL_NOEXCEPT {
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

    void remove(fl::shared_ptr<CoroutineContext> ctx) FL_NOEXCEPT override {
        fl::queue<fl::weak_ptr<CoroutineContext>> temp;
        fl::unique_lock<fl::mutex> lock(mQueueMutex) FL_NOEXCEPT;
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

    void stop_all() FL_NOEXCEPT override {
        fl::unique_lock<fl::mutex> lock(mQueueMutex) FL_NOEXCEPT;
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

CoroutineRunner& CoroutineRunner::instance() FL_NOEXCEPT {
    return fl::Singleton<CoroutineRunnerImpl>::instance();
}

} // namespace detail

namespace platforms {

//=============================================================================
// Coroutine Runtime singleton
//=============================================================================

ICoroutineRuntime& ICoroutineRuntime::instance() FL_NOEXCEPT {
    return fl::Singleton<CoroutineRuntimeStub>::instance();
}

//=============================================================================
// Background thread registry
//=============================================================================

struct BackgroundThreadRegistry {
    fl::vector<fl::thread> threads;
    fl::mutex mtx;

    static BackgroundThreadRegistry& instance() FL_NOEXCEPT {
        return fl::Singleton<BackgroundThreadRegistry>::instance();
    }
};

#ifdef TEST_DLL_MODE
struct CoroutineThreadRegistry {
    fl::vector<fl::thread> threads;
    fl::mutex mtx;

    static CoroutineThreadRegistry& instance() FL_NOEXCEPT {
        return fl::Singleton<CoroutineThreadRegistry>::instance();
    }

    void add(fl::thread&& t) FL_NOEXCEPT {
        fl::unique_lock<fl::mutex> lock(mtx) FL_NOEXCEPT;
        threads.push_back(fl::move(t));
    }
};
#endif

void cleanup_coroutine_threads() FL_NOEXCEPT {
    fl::detail::CoroutineRunner::instance().stop_all();

    // Join coroutine threads (DLL mode only)
#ifdef TEST_DLL_MODE
    {
        auto& creg = CoroutineThreadRegistry::instance();
        fl::unique_lock<fl::mutex> lock(creg.mtx) FL_NOEXCEPT;
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
    fl::unique_lock<fl::mutex> lock(reg.mtx) FL_NOEXCEPT;
    for (auto& t : reg.threads) {
        if (t.joinable()) t.join();
    }
    reg.threads.clear();
    bg.resetShutdown();
}

void register_background_thread(fl::thread&& t) FL_NOEXCEPT {
    auto& reg = BackgroundThreadRegistry::instance();
    fl::unique_lock<fl::mutex> lock(reg.mtx) FL_NOEXCEPT;
    reg.threads.push_back(fl::move(t));
}

void cleanup_background_threads() FL_NOEXCEPT {
    auto& bg = ICoroutineRuntime::instance();
    bg.requestShutdown();
    auto& reg = BackgroundThreadRegistry::instance();
    fl::unique_lock<fl::mutex> lock(reg.mtx) FL_NOEXCEPT;
    for (auto& t : reg.threads) {
        if (t.joinable()) t.join();
    }
    reg.threads.clear();
    bg.resetShutdown();
}

bool is_shutdown_requested() FL_NOEXCEPT {
    return ICoroutineRuntime::instance().isShutdownRequested();
}

//=============================================================================
// Thread-local coroutine context — lets exitCurrent() find its context
//=============================================================================

fl::detail::CoroutineContext*& runningStubCoroutineContext() FL_NOEXCEPT {
    return SingletonThreadLocal<fl::detail::CoroutineContext*>::instance();
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
 FL_NOEXCEPT : mName(fl::move(name))
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

            // Set thread-local so exitCurrent() can find this context
            runningStubCoroutineContext() = ctx_shared.get();

            // Execute user function
            func();

            // Clear thread-local
            runningStubCoroutineContext() = nullptr;

            // Mark completed and signal "done"
            // (skipped if exitCurrent() already did this)
            if (!ctx_shared->is_completed()) {
                ctx_shared->set_completed(true);
                main_sema.release();
            }
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

    void stop() FL_NOEXCEPT override {
        if (!mContext) return;

        auto& runner = fl::detail::CoroutineRunner::instance();
        runner.stop(mContext);

        // Brief wait for thread to acknowledge stop
        fl::this_thread::sleep_for(fl::chrono::milliseconds(10)); // ok sleep for

        runner.remove(mContext);
        mContext.reset();
    }

    bool isRunning() const FL_NOEXCEPT override {
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
                                            u8 priority) FL_NOEXCEPT {
    return TaskCoroutinePtr(
        new TaskCoroutineStubImpl(fl::move(name), fl::move(function), stack_size, priority));  // ok bare allocation
}

TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ICoroutineTask::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int /*core_id*/) FL_NOEXCEPT {
    return TaskCoroutineStub::create(fl::move(name), fl::move(function), stack_size, priority);
}

void ICoroutineTask::exitCurrent() FL_NOEXCEPT {
    auto* ctx = runningStubCoroutineContext();
    if (ctx) {
        ctx->set_completed(true);
        // Signal "done" to unblock the main thread's second acquire()
        fl::detail::CoroutineRunner::instance().get_main_thread_semaphore().release();
    }
    // Block forever — thread will be joined during cleanup
    while (true) {
        fl::this_thread::sleep_for(fl::chrono::seconds(1)); // ok sleep for
    }
}

} // namespace platforms
} // namespace fl

#endif // FASTLED_STUB_IMPL
