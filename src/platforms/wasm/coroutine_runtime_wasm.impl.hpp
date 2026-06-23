#pragma once

// IWYU pragma: private

/// @file coroutine_runtime_wasm.impl.hpp
/// @brief WASM coroutine runtime — pumps cooperative coroutine runner
///
/// Provides cooperative coroutines in WASM via pthreads + SharedArrayBuffer
/// (CoroutinePlatformPthread). Works in every cross-origin isolated webview
/// (WebKit/Safari, Firefox, WebView2, WebKitGTK).
///
/// The generic CoroutineRunner handles scheduling; this file just wires
/// the platform implementation and runtime together. See issue #2452 for
/// the JSPI -> pthread migration history.

#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

// IWYU pragma: begin_keep
#include "platforms/wasm/coroutine_platform_wasm.hpp"
#include <emscripten/atomic.h>
#include "fl/stl/atomic.h"
#include "platforms/coroutine_runtime.h"
#include "platforms/coroutine.h"
#include "fl/stl/singleton.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/log/log.h"
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

//=============================================================================
// Coroutine Runtime — pumps cooperative coroutine runner
//=============================================================================

class CoroutineRuntimeWasm : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) FL_NOEXCEPT override {
        if (CoroutineContext::isInsideCoroutine()) {
            // Called from within a coroutine — yield back to runner
            CoroutineContext::suspend();
        } else {
            // Called from main thread — give time to background coroutines
            CoroutineRunner::instance().run(us);
        }
    }

    /// Park on a real futex instead of busy-yielding.
    ///
    /// fl::platforms::await() polls a Promise with this method between
    /// re-checks. The calling pthread parks via WASM `Atomics.wait` until
    /// either (a) the timeout elapses or (b) wakeWaiters() notifies on the
    /// shared counter from a JS callback.
    ///
    /// emscripten_atomic_wait_u32 is only valid off the browser main UI
    /// thread, but with -sPROXY_TO_PTHREAD both main() and every coroutine
    /// run on worker pthreads, so any caller that reaches this path is on
    /// a parkable thread.
    void suspendMainthread(fl::u32 us) FL_NOEXCEPT override {
        // Inside a coroutine: hand the baton back to the runner so other
        // coroutines (and the main loop) keep progressing — same as default.
        if (CoroutineContext::isInsideCoroutine()) {
            CoroutineContext::suspend();
            return;
        }
        // Outside a coroutine (e.g. await called from setup/loop):
        // snapshot the futex counter then park on it for up to `us`. Any
        // wakeWaiters() call between the snapshot and the wait will be
        // observed and short-circuit the timeout.
        fl::u32 snapshot = mWakeupCounter.load();
        fl::i64 timeout_ns = static_cast<fl::i64>(us) * 1000;
        // emscripten_atomic_wait_u32 takes plain void* — fl::atomic<u32> is
        // a standard-layout wrapper around a single u32, so its address
        // coincides with the underlying word.
        emscripten_atomic_wait_u32(
            static_cast<void*>(&mWakeupCounter),  // ok reinterpret_cast — atomic wrapper is layout-compat with its u32
            snapshot,
            timeout_ns);
    }

    /// Wake every pthread parked in suspendMainthread().
    /// Called from async callbacks (currently js_fetch_success_callback /
    /// js_fetch_error_callback) after they have committed Promise state, so
    /// the awaiting pthread re-checks promptly.
    void wakeWaiters() FL_NOEXCEPT override {
        mWakeupCounter.fetch_add(1);
        // emscripten_atomic_notify expects a 64-bit count; pass a sentinel
        // that wakes all parked threads.
        emscripten_atomic_notify(
            static_cast<void*>(&mWakeupCounter),
            static_cast<fl::i64>(0x7fffffff));
    }

private:
    /// 32-bit shared-memory word used as a generic Promise-wake futex.
    /// The exact value carries no information — `Atomics.wait` parks while
    /// the loaded value equals the snapshot, so any change at all is a wake.
    fl::atomic<fl::u32> mWakeupCounter{0};
};

//=============================================================================
// Platform Registration — register fiber platform at startup
//=============================================================================

namespace {
struct WasmPlatformRegistrar {
    WasmPlatformRegistrar() FL_NOEXCEPT {
        ICoroutinePlatform::setInstance(
            &fl::Singleton<CoroutinePlatformPthread>::instance());
    }
};
static WasmPlatformRegistrar sWasmPlatformRegistrar;
}  // namespace

ICoroutineRuntime& ICoroutineRuntime::instance() FL_NOEXCEPT {
    return fl::Singleton<CoroutineRuntimeWasm>::instance();
}

//=============================================================================
// Task Coroutine — cooperative fiber-based
//=============================================================================

class TaskCoroutineWasm : public ICoroutineTask {
public:
    static TaskCoroutinePtr create(fl::string name,
                                    TaskFunction function,
                                    size_t stack_size = 4096,
                                    u8 priority = 5) FL_NOEXCEPT {
        auto* ctx = CoroutineContext::create(fl::move(function), stack_size);
        if (!ctx) {
            FL_WARN("TaskCoroutineWasm: Failed to create context for '"
                    << name << "'");
            return nullptr;
        }

        TaskCoroutinePtr task(new TaskCoroutineWasm()) FL_NOEXCEPT;  // ok bare allocation
        auto* impl = static_cast<TaskCoroutineWasm*>(task.get());
        impl->mName = fl::move(name);
        impl->mContext.reset(ctx);

        // Register with the global runner
        CoroutineRunner::instance().enqueue(ctx);

        return task;
    }

    ~TaskCoroutineWasm() override { stop(); }

    void stop() FL_NOEXCEPT override {
        if (!mContext) return;
        mContext->stop_and_complete();
        CoroutineRunner::instance().remove(mContext.get());
    }

    bool isRunning() const FL_NOEXCEPT override {
        if (!mContext) return false;
        return !mContext->is_completed();
    }

private:
    TaskCoroutineWasm() = default;

    fl::string mName;
    fl::unique_ptr<CoroutineContext> mContext;
};

//=============================================================================
// Factory function
//=============================================================================

TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ICoroutineTask::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int /*core_id*/) FL_NOEXCEPT {
    return TaskCoroutineWasm::create(fl::move(name), fl::move(function),
                                     stack_size, priority);
}

//=============================================================================
// Static exitCurrent — suspend back to runner and mark completed
//=============================================================================

void ICoroutineTask::exitCurrent() FL_NOEXCEPT {
    CoroutineContext* ctx = CoroutineContext::runningCoroutine();
    if (ctx) {
        ctx->set_should_stop(true);
    }
    CoroutineContext::suspend();
    // Should never resume — runner marks completed and removes on should_stop.
    while (true) {}
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_WASM
