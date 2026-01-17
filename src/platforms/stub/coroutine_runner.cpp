/// @file coroutine_runner.cpp
/// @brief Implementation of coroutine runner for stub platform

#ifdef FASTLED_STUB_IMPL

#include "coroutine_runner.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"
#include "fl/stl/mutex.h"  // Provides fl::mutex, fl::recursive_mutex, fl::unique_lock
#include "fl/stl/condition_variable.h"  // Provides fl::condition_variable
#include "fl/stl/shared_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/queue.h"
#include "fl/dbg.h"  // Debug output

namespace fl {
namespace detail {

// ===== CoroutineContextImpl (concrete implementation) =====

class CoroutineContextImpl : public CoroutineContext {
public:
    CoroutineContextImpl()
        : mReady(false), mShouldStop(false), mCompleted(false) {}

    void wait() override {
        // FL_DBG("CoroutineContext " << fl::hex << reinterpret_cast<uintptr_t>(this) << ": Entering wait()");
        fl::unique_lock<fl::mutex> lock(mMutex);
        // FL_DBG("CoroutineContext " << fl::hex << reinterpret_cast<uintptr_t>(this) << ": Acquired lock, waiting on condition...");

        // CRITICAL: Check if predicate is already true before waiting
        // This avoids waiting if the condition was signaled before we acquired the lock
        bool ready = mReady.load();
        bool should_stop = mShouldStop.load();
        if (!ready && !should_stop) {
            // Need to wait - predicate is false
            mCv.wait(lock, [this]() { return mReady.load() || mShouldStop.load(); });
        }

        // FL_DBG("CoroutineContext " << fl::hex << reinterpret_cast<uintptr_t>(this) << ": Woke from wait, ready=" << mReady.load() << " stop=" << mShouldStop.load());
        mReady.store(false);  // Reset for next time
    }

    void signal() override {
        // FL_DBG("CoroutineContext " << fl::hex << reinterpret_cast<uintptr_t>(this) << ": Signaling");
        fl::unique_lock<fl::mutex> lock(mMutex);
        mReady.store(true);
        mCv.notify_one();
        // FL_DBG("CoroutineContext " << fl::hex << reinterpret_cast<uintptr_t>(this) << ": Signal complete");
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
    fl::mutex mMutex;
    fl::condition_variable mCv;
    fl::atomic<bool> mReady;            // Signaled when this coroutine can run
    fl::atomic<bool> mShouldStop;       // Signal for graceful shutdown
    fl::atomic<bool> mCompleted;        // Set when coroutine finishes
};

// CoroutineContext factory method
fl::shared_ptr<CoroutineContext> CoroutineContext::create() {
    return fl::make_shared<CoroutineContextImpl>();
}

// ===== CoroutineRunnerImpl (concrete implementation) =====

class CoroutineRunnerImpl : public CoroutineRunner {
public:
    CoroutineRunnerImpl() = default;

    void enqueue(fl::shared_ptr<CoroutineContext> ctx) override {
        fl::unique_lock<fl::mutex> lock(mQueueMutex);
        // FL_DBG("CoroutineRunner: Enqueuing context " << fl::hex << reinterpret_cast<uintptr_t>(ctx.get()) << ", queue size before: " << mQueue.size());
        mQueue.push(fl::weak_ptr<CoroutineContext>(ctx));
        // FL_DBG("CoroutineRunner: Queue size after enqueue: " << mQueue.size());
    }

    void signal_next() override {
        fl::unique_lock<fl::mutex> lock(mQueueMutex);
        // FL_DBG("CoroutineRunner::signal_next: Called, queue size: " << mQueue.size());

        // Remove expired/completed coroutines from front of queue
        while (!mQueue.empty()) {
            fl::shared_ptr<CoroutineContext> ctx = mQueue.front().lock();
            // If expired (context deleted) or completed, remove from queue
            if (!ctx || ctx->is_completed()) {
                // if (!ctx) {
                //     FL_DBG("CoroutineRunner::signal_next: Removing expired context (deleted)");
                // } else {
                //     FL_DBG("CoroutineRunner::signal_next: Removing completed context " << fl::hex << reinterpret_cast<uintptr_t>(ctx.get()));
                // }
                mQueue.pop();
            } else {
                break;  // Found live context, stop cleaning
            }
        }

        // Signal next waiting coroutine
        if (!mQueue.empty()) {
            fl::shared_ptr<CoroutineContext> ctx = mQueue.front().lock();
            if (ctx) {  // Context still alive
                // FL_DBG("CoroutineRunner::signal_next: Signaling context " << fl::hex << reinterpret_cast<uintptr_t>(ctx.get()));
                mQueue.pop();

                // Re-enqueue at back for next execution cycle
                mQueue.push(fl::weak_ptr<CoroutineContext>(ctx));
                // FL_DBG("CoroutineRunner::signal_next: Re-enqueued context " << fl::hex << reinterpret_cast<uintptr_t>(ctx.get()));

                // Signal this coroutine to run
                // FIXED: Do NOT unlock early - keep lock held until signal() completes
                // ctx->signal() acquires its own mutex (mMutex), not our mQueueMutex,
                // so there's no deadlock risk. The lock will auto-release at scope exit.
                // FL_DBG("CoroutineRunner::signal_next: Calling ctx->signal()");
                ctx->signal();
                // FL_DBG("CoroutineRunner::signal_next: ctx->signal() returned");
            } else {
                // FL_DBG("CoroutineRunner::signal_next: Front context expired, removing");
                mQueue.pop();
            }
        } else {
            // FL_DBG("CoroutineRunner::signal_next: Queue is empty, no context to signal");
        }
    }

    void stop(fl::shared_ptr<CoroutineContext> ctx) override {
        if (ctx) {
            ctx->set_should_stop(true);
            ctx->signal();  // Wake it up so it can exit
        }
    }

    void remove(fl::shared_ptr<CoroutineContext> ctx) override {
        fl::unique_lock<fl::mutex> lock(mQueueMutex);
        // FL_DBG("CoroutineRunner::remove: Removing context " << fl::hex << reinterpret_cast<uintptr_t>(ctx.get()));

        // Create a temporary queue and copy all contexts except the one to remove
        fl::queue<fl::weak_ptr<CoroutineContext>> temp;
        while (!mQueue.empty()) {
            fl::weak_ptr<CoroutineContext> weak_front = mQueue.front();
            mQueue.pop();
            fl::shared_ptr<CoroutineContext> front = weak_front.lock();
            // Keep if: (1) still alive AND (2) not the one to remove
            if (front && front.get() != ctx.get()) {
                temp.push(weak_front);
            }
            // Expired contexts are silently dropped
        }

        // Swap back
        mQueue = temp;
    }

    void stop_all() override {
        fl::unique_lock<fl::mutex> lock(mQueueMutex);
        // FL_DBG("CoroutineRunner::stop_all: Called, queue size: " << mQueue.size());

        // Signal all coroutines to stop
        fl::queue<fl::weak_ptr<CoroutineContext>> temp = mQueue;
        while (!temp.empty()) {
            fl::weak_ptr<CoroutineContext> weak_ctx = temp.front();
            temp.pop();
            fl::shared_ptr<CoroutineContext> ctx = weak_ctx.lock();
            if (ctx) {
                // FL_DBG("CoroutineRunner::stop_all: Stopping context " << fl::hex << reinterpret_cast<uintptr_t>(ctx.get()));
                ctx->set_should_stop(true);
                ctx->signal();
            }
        }

        // Clear the queue
        while (!mQueue.empty()) {
            mQueue.pop();
        }
        // FL_DBG("CoroutineRunner::stop_all: Queue cleared");
    }

private:
    fl::mutex mQueueMutex;
    fl::queue<fl::weak_ptr<CoroutineContext>> mQueue;
};

// CRITICAL: File-scope static instance to ensure DLL boundary sharing on Windows.
// Function-local statics and template singletons create separate instances per DLL/EXE,
// causing coroutine synchronization failures. A file-scope static in the .cpp ensures
// the singleton lives in the DLL and is shared by all consumers (EXE + DLL).
static CoroutineRunnerImpl gCoroutineRunnerInstance;

// CoroutineRunner singleton accessor
CoroutineRunner& CoroutineRunner::instance() {
    return gCoroutineRunnerInstance;
}

// ===== Global execution lock for cooperative multitasking =====
//
// This lock ensures only one thread executes "user code" at a time.
// It provides a single-threaded execution model on top of real threads.

// File-scope static mutex for DLL boundary sharing (same rationale as CoroutineRunner)
static fl::mutex gGlobalExecutionMutex;

// Thread-local flag to track if current thread holds the global execution lock.
// Each thread has its own copy, so coroutines and main thread track independently.
// This prevents undefined behavior from unlocking a mutex we don't own.
thread_local bool gHoldingExecutionLock = false;

void global_execution_lock() {
    gGlobalExecutionMutex.lock();
    gHoldingExecutionLock = true;
}

void global_execution_unlock() {
    gHoldingExecutionLock = false;
    gGlobalExecutionMutex.unlock();
}

bool global_execution_try_lock() {
    bool acquired = gGlobalExecutionMutex.try_lock();
    if (acquired) {
        gHoldingExecutionLock = true;
    }
    return acquired;
}

bool global_execution_is_held() {
    return gHoldingExecutionLock;
}

void global_execution_set_held(bool held) {
    gHoldingExecutionLock = held;
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL
