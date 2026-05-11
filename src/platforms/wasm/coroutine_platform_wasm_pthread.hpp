#pragma once

// IWYU pragma: private

/// @file coroutine_platform_wasm_pthread.hpp
/// @brief WASM coroutine platform using pthreads + SharedArrayBuffer
///
/// Alternate ICoroutinePlatform implementation that uses OS-level threads
/// (Emscripten pthreads on top of Web Workers + SharedArrayBuffer) plus a
/// mutex/condition_variable baton to provide cooperative context switching.
///
/// Compared to the JSPI variant (coroutine_platform_wasm.hpp):
///   - No -sJSPI requirement; works in every cross-origin-isolated webview
///     (Safari/WKWebView, Firefox, Chrome, WebView2, WebKitGTK).
///   - Each coroutine context owns a real OS thread that is parked on its
///     own condition variable while another coroutine runs. contextSwitch
///     hands a "baton" from the current thread to the target thread, then
///     blocks the caller — only one thread runs user code at a time, so
///     existing sketch code stays single-owner.
///
/// Selected at compile time via FASTLED_WASM_PTHREADS=1.
///
/// See issue #2452 for the rationale and migration plan.

#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

// IWYU pragma: begin_keep
#include <emscripten.h>
// IWYU pragma: end_keep

#include "platforms/coroutine_runtime.h"
#include "fl/stl/condition_variable.h"
#include "fl/stl/int.h"
#include "fl/stl/mutex.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/thread.h"

namespace fl {
namespace platforms {

/// Per-context state for the pthread-based coroutine platform.
/// Holds the OS thread, its parking primitive (mutex + cv), and the
/// baton/exit flags used by contextSwitch().
struct PthreadCoroCtx {
    fl::thread* thread = nullptr;          ///< nullptr for the runner context
    fl::mutex mutex;                        ///< Guards `signaled` / `exit_requested`
    fl::condition_variable cv;              ///< Park spot for this thread
    bool signaled = false;                  ///< Set true to wake this context
    bool exit_requested = false;            ///< Set true to terminate the thread
    void (*entry_fn)() = nullptr;           ///< User-supplied entry trampoline
};

/// Thread body. Each non-runner context owns one of these.
/// Park until first signal, then run the entry trampoline; the trampoline
/// is responsible for ending with a contextSwitch back to the runner.
inline void pthread_coro_thread_main(PthreadCoroCtx* self) FL_NOEXCEPT {
    {
        fl::unique_lock<fl::mutex> lk(self->mutex);
        self->cv.wait(lk, [self] { return self->signaled; });
        self->signaled = false;
        if (self->exit_requested) {
            return;
        }
    }

    if (self->entry_fn) {
        self->entry_fn();
    }

    // The trampoline always finishes with contextSwitch(self, runner), which
    // re-parks this thread on `self->cv`. Reaching this point only happens
    // if exit_requested was set while parked there — in which case
    // contextSwitch() returned and we fall off the thread normally.
}

class CoroutinePlatformPthread : public ICoroutinePlatform {
public:
    void* createContext(void (*entry_fn)(), size_t /*stack_size*/) FL_NOEXCEPT override {
        auto* ctx = new PthreadCoroCtx();  // ok bare allocation - platform context lifetime
        ctx->entry_fn = entry_fn;
        ctx->thread = new fl::thread(pthread_coro_thread_main, ctx);  // ok bare allocation
        return ctx;
    }

    void* createRunnerContext() FL_NOEXCEPT override {
        // Runner is the calling thread (sketch thread). No OS thread to spawn —
        // we just need a parking spot for it.
        auto* ctx = new PthreadCoroCtx();  // ok bare allocation
        return ctx;
    }

    void destroyContext(void* ctx_p) FL_NOEXCEPT override {
        auto* ctx = static_cast<PthreadCoroCtx*>(ctx_p);
        if (!ctx) return;

        if (ctx->thread) {
            // Wake the thread out of its current park and ask it to exit.
            {
                fl::lock_guard<fl::mutex> lk(ctx->mutex);
                ctx->exit_requested = true;
                ctx->signaled = true;
            }
            ctx->cv.notify_all();

            // The thread may be parked deep inside contextSwitch (after the
            // coroutine completed) or inside its initial wait. We don't join
            // because the thread might still be holding user-stack state
            // that's safest to let the OS clean up at module exit.
            if (ctx->thread->joinable()) {
                ctx->thread->detach();
            }
            delete ctx->thread;  // ok bare allocation - platform context lifetime
            ctx->thread = nullptr;
        }
        delete ctx;  // ok bare allocation - platform context lifetime
    }

    void contextSwitch(void* from_ctx_p, void* to_ctx_p) FL_NOEXCEPT override {
        auto* from = static_cast<PthreadCoroCtx*>(from_ctx_p);
        auto* to = static_cast<PthreadCoroCtx*>(to_ctx_p);
        if (!from || !to) return;

        // Pass the baton to `to`.
        {
            fl::lock_guard<fl::mutex> lk(to->mutex);
            to->signaled = true;
        }
        to->cv.notify_one();

        // Park until someone hands the baton back to us.
        {
            fl::unique_lock<fl::mutex> lk(from->mutex);
            from->cv.wait(lk, [from] { return from->signaled; });
            from->signaled = false;
        }
    }

    fl::u32 micros() const FL_NOEXCEPT override {
        // emscripten_get_now() returns milliseconds as double
        return static_cast<fl::u32>(emscripten_get_now() * 1000.0);
    }
};

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_WASM
