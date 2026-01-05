/// @file coroutine_runner.cpp
/// @brief Implementation of coroutine runner for stub platform

#ifdef FASTLED_STUB_IMPL

#include "coroutine_runner.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"  // Provides fl::mutex, fl::recursive_mutex, fl::unique_lock
#include "fl/stl/unique_ptr.h"
#include "fl/stl/queue.h"
#include <mutex>               // ok include (std::unique_lock required for std::condition_variable)
#include <condition_variable>  // ok include (no fl wrapper, works with fl::mutex via std::unique_lock)

namespace fl {
namespace detail {

// ===== CoroutineContextImpl (concrete implementation) =====

class CoroutineContextImpl : public CoroutineContext {
public:
    CoroutineContextImpl()
        : mReady(false), mShouldStop(false), mCompleted(false) {}

    void wait() override {
        std::unique_lock<std::mutex> lock(mMutex);
        mCv.wait(lock, [this]() { return mReady.load() || mShouldStop.load(); });
        mReady.store(false);  // Reset for next time
    }

    void signal() override {
        std::unique_lock<std::mutex> lock(mMutex);
        mReady.store(true);
        mCv.notify_one();
    }

    bool should_stop() const override {
        return mShouldStop.load();
    }

    void set_should_stop(bool value) override {
        mShouldStop.store(value);
    }

    bool is_completed() const override {
        return mCompleted.load();
    }

    void set_completed(bool value) override {
        mCompleted.store(value);
    }

private:
    std::mutex mMutex;                  // std::mutex required for std::condition_variable compatibility
    std::condition_variable mCv;
    fl::atomic<bool> mReady;            // Signaled when this coroutine can run
    fl::atomic<bool> mShouldStop;       // Signal for graceful shutdown
    fl::atomic<bool> mCompleted;        // Set when coroutine finishes
};

// CoroutineContext factory method
fl::unique_ptr<CoroutineContext> CoroutineContext::create() {
    return fl::make_unique<CoroutineContextImpl>();
}

// ===== CoroutineRunnerImpl (concrete implementation) =====

class CoroutineRunnerImpl : public CoroutineRunner {
public:
    CoroutineRunnerImpl() = default;

    void enqueue(CoroutineContext* ctx) override {
        fl::unique_lock<fl::recursive_mutex> lock(mQueueMutex);
        mQueue.push(ctx);
    }

    void signal_next() override {
        fl::unique_lock<fl::recursive_mutex> lock(mQueueMutex);

        // Remove completed coroutines from front of queue
        while (!mQueue.empty() && mQueue.front()->is_completed()) {
            mQueue.pop();
        }

        // Signal next waiting coroutine
        if (!mQueue.empty()) {
            CoroutineContext* ctx = mQueue.front();
            mQueue.pop();

            // Re-enqueue at back for next execution cycle
            mQueue.push(ctx);

            // Signal this coroutine to run
            lock.unlock();  // Unlock before signaling to avoid holding mutex
            ctx->signal();
        }
    }

    void stop(CoroutineContext* ctx) override {
        if (ctx) {
            ctx->set_should_stop(true);
            ctx->signal();  // Wake it up so it can exit
        }
    }

    void stop_all() override {
        fl::unique_lock<fl::recursive_mutex> lock(mQueueMutex);

        // Signal all coroutines to stop
        fl::queue<CoroutineContext*> temp = mQueue;
        while (!temp.empty()) {
            CoroutineContext* ctx = temp.front();
            temp.pop();
            ctx->set_should_stop(true);
            ctx->signal();
        }

        // Clear the queue
        while (!mQueue.empty()) {
            mQueue.pop();
        }
    }

private:
    fl::recursive_mutex mQueueMutex;
    fl::queue<CoroutineContext*> mQueue;
};

// CoroutineRunner singleton accessor
CoroutineRunner& CoroutineRunner::instance() {
    return fl::Singleton<CoroutineRunnerImpl>::instance();
}

// ===== Global synchronization primitives =====

class GlobalMutex {
public:
    void lock() { mMutex.lock(); }
    void unlock() { mMutex.unlock(); }
    bool try_lock() { return mMutex.try_lock(); }

private:
    fl::recursive_mutex mMutex;
};

class GlobalConditionVariable {
public:
    void notify_one() { mCv.notify_one(); }
    void notify_all() { mCv.notify_all(); }

    template<typename Lock, typename Predicate>
    void wait(Lock& lock, Predicate pred) {
        mCv.wait(lock, pred);
    }

    // Overload for std::mutex specifically (required by std::condition_variable)
    template<typename Predicate>
    void wait(std::unique_lock<std::mutex>& lock, Predicate pred) {
        mCv.wait(lock, pred);
    }

private:
    std::condition_variable mCv;
};

GlobalMutex& global_execution_mutex() {
    return fl::Singleton<GlobalMutex>::instance();
}

GlobalConditionVariable& global_execution_cv() {
    return fl::Singleton<GlobalConditionVariable>::instance();
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL
