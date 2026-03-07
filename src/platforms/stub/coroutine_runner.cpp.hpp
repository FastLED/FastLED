// IWYU pragma: private

/// @file coroutine_runner.cpp
/// @brief Implementation of coroutine runner for stub platform

#ifdef FASTLED_STUB_IMPL

#include "platforms/stub/coroutine_runner.h"
#include "fl/stl/semaphore.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/queue.h"

namespace fl {
namespace detail {

// ===== CoroutineContextImpl =====

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

// ===== CoroutineRunnerImpl =====

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
} // namespace fl

#endif // FASTLED_STUB_IMPL
