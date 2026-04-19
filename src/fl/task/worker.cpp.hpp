#pragma once

/// @file fl/task/worker.cpp.hpp
/// @brief Platform-dispatching implementation of fl::task::Worker.
///
/// On dual-core ESP32 (FL_HAS_MULTICORE_AFFINITY) a persistent FreeRTOS
/// task pinned to the requested core sits on a binary semaphore waiting
/// for a functor pointer, runs it, signals a second semaphore, loops. Kept
/// intentionally simple — one functor in flight at a time, caller
/// serialised through a mutex.
///
/// On single-core ESP32 / non-ESP32, run() just calls fn() synchronously.

#include "fl/task/worker.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"

#if defined(FL_IS_ESP32)
// Deep platform include is intentional — Worker needs FL_HAS_MULTICORE_AFFINITY
// and FL_CPU_CORES to decide whether to spawn a real pinned task.
// IWYU pragma: begin_keep
#include "platforms/esp/32/feature_flags/enabled.h"  // ok platform headers
// IWYU pragma: end_keep
#endif

#if defined(FL_IS_ESP32) && defined(FL_HAS_MULTICORE_AFFINITY) && FL_HAS_MULTICORE_AFFINITY
#define FL_WORKER_REAL_IMPL 1
#else
#define FL_WORKER_REAL_IMPL 0
#endif

#if FL_WORKER_REAL_IMPL
#include "fl/stl/mutex.h"
#include "fl/stl/atomic.h"
#include "fl/stl/move.h"
extern "C" {
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
// IWYU pragma: end_keep
}
#endif

namespace fl {
namespace task {

#if FL_WORKER_REAL_IMPL

class WorkerImpl {
public:
    WorkerImpl(int core, const char* name, size_t stack_size) FL_NOEXCEPT
        : mShouldStop(false), mWorkFn(nullptr) {
        mWorkAvailable = xSemaphoreCreateBinary();
        mWorkDone      = xSemaphoreCreateBinary();
        if (!mWorkAvailable || !mWorkDone) {
            mValid = false;
            return;
        }
        BaseType_t affinity = (core >= 0 && core < FL_CPU_CORES)
            ? static_cast<BaseType_t>(core)
            : tskNO_AFFINITY;
        // Slightly elevated priority so the worker preempts idle work but
        // doesn't starve user tasks on the same core.
        BaseType_t rc = xTaskCreatePinnedToCore(
            &WorkerImpl::taskEntry,
            name,
            static_cast<configSTACK_DEPTH_TYPE>(stack_size),
            this,
            tskIDLE_PRIORITY + 5,
            &mTask,
            affinity);
        mValid = (rc == pdPASS);
    }

    ~WorkerImpl() FL_NOEXCEPT {
        mShouldStop.store(true);
        if (mWorkAvailable) {
            // Wake the worker so it can observe mShouldStop and exit.
            xSemaphoreGive(mWorkAvailable);
        }
        if (mTask) {
            // Let the worker finish its loop and self-delete via vTaskDelete.
            // We poll briefly; if the worker never observes shutdown,
            // force-delete to avoid leaking the FreeRTOS task.
            for (int i = 0; i < 10 && mTask; ++i) {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            if (mTask) {
                vTaskDelete(mTask);
                mTask = nullptr;
            }
        }
        if (mWorkAvailable) {
            vSemaphoreDelete(mWorkAvailable);
            mWorkAvailable = nullptr;
        }
        if (mWorkDone) {
            vSemaphoreDelete(mWorkDone);
            mWorkDone = nullptr;
        }
    }

    bool valid() const FL_NOEXCEPT { return mValid; }

    void run(fl::function<void()> fn) FL_NOEXCEPT {
        if (!mValid) {
            // Fall back to synchronous on allocation failure rather than
            // silently dropping the work.
            fn();
            return;
        }
        fl::lock_guard<fl::mutex> lock(mPostMutex);
        mWorkFn = &fn;
        xSemaphoreGive(mWorkAvailable);
        xSemaphoreTake(mWorkDone, portMAX_DELAY);
        mWorkFn = nullptr;
    }

private:
    static void taskEntry(void* param) FL_NOEXCEPT {
        auto* self = static_cast<WorkerImpl*>(param);
        while (true) {
            xSemaphoreTake(self->mWorkAvailable, portMAX_DELAY);
            if (self->mShouldStop.load()) {
                break;
            }
            auto* fn = self->mWorkFn;
            if (fn && *fn) {
                (*fn)();
            }
            xSemaphoreGive(self->mWorkDone);
        }
        self->mTask = nullptr;
        vTaskDelete(nullptr);
    }

    fl::mutex mPostMutex;
    fl::atomic<bool> mShouldStop;
    fl::function<void()>* mWorkFn;
    SemaphoreHandle_t mWorkAvailable = nullptr;
    SemaphoreHandle_t mWorkDone      = nullptr;
    TaskHandle_t mTask = nullptr;
    bool mValid = false;
};

#endif // FL_WORKER_REAL_IMPL

Worker::Worker(int core, const char* name, size_t stack_size) FL_NOEXCEPT
    : mCoreId(core), mImpl(nullptr) {
#if FL_WORKER_REAL_IMPL
    mImpl = new WorkerImpl(core, name, stack_size);  // ok bare allocation
    if (!mImpl->valid()) {
        delete mImpl;  // ok bare allocation
        mImpl = nullptr;
    }
#else
    (void)core;
    (void)name;
    (void)stack_size;
#endif
}

Worker::~Worker() FL_NOEXCEPT {
#if FL_WORKER_REAL_IMPL
    if (mImpl) {
        delete mImpl;  // ok bare allocation
        mImpl = nullptr;
    }
#endif
}

void Worker::run(fl::function<void()> fn) FL_NOEXCEPT {
#if FL_WORKER_REAL_IMPL
    if (mImpl) {
        mImpl->run(fl::move(fn));
        return;
    }
#endif
    // Synchronous fallback: single-core ESP32, non-ESP32, or allocation
    // failure on dual-core ESP32.
    if (fn) {
        fn();
    }
}

bool Worker::isPinned() const FL_NOEXCEPT {
    // Unified read across real-impl and synchronous-fallback paths —
    // mImpl is always nullptr on the fallback, always valid (or nullptr on
    // allocation failure) on the real path. Keeping this branch un-#if'd
    // also keeps clang's -Wunused-private-field happy on non-ESP32 TUs.
    return mImpl != nullptr;
}

} // namespace task
} // namespace fl

#undef FL_WORKER_REAL_IMPL
