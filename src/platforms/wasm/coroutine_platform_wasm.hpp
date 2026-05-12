#pragma once

// IWYU pragma: private

/// @file coroutine_platform_wasm.hpp
/// @brief WASM coroutine platform using pthreads + SharedArrayBuffer
///
/// ICoroutinePlatform implementation that uses OS-level threads (Emscripten
/// pthreads on top of Web Workers + SharedArrayBuffer) plus a
/// mutex/condition_variable baton to provide cooperative context switching.
///
/// Each coroutine context owns a real OS thread that is parked on its own
/// condition variable while another coroutine runs. contextSwitch hands a
/// "baton" from the current thread to the target thread, then blocks the
/// caller — only one thread runs user code at a time, so existing sketch
/// code stays single-owner.
///
/// Works in every cross-origin-isolated webview (Safari/WKWebView, Firefox,
/// Chrome, WebView2, WebKitGTK). The previous JSPI back-end was removed in
/// #2452 phase 7 — see that issue for the migration history.

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
#include "fl/stl/unique_ptr.h"

// Forward-declare pthread_exit (stable POSIX C ABI). Avoids pulling
// <pthread.h> into a header — that include is banned project-wide in favor
// of fl/stl/thread.h. We only need this one symbol to terminate the worker
// thread cleanly during destroyContext() teardown; see contextSwitch().
//
// ============================================================================
// Coroutine teardown contract — IMPORTANT for callers writing coroutines
// ============================================================================
//
// FastLED is built with `-fno-exceptions` (and so is the WASM target — see
// `[all].compiler_flags` in build_flags.toml). On Emscripten, `pthread_exit`
// under that configuration calls `_emscripten_thread_exit` followed by
// `emscripten_unwind_to_js_event_loop` — it does NOT walk the C++ stack and
// does NOT run destructors for objects living on it. With `-pthread`
// `-sPROXY_TO_PTHREAD` the same is true; `pthread_exit` is thread-local but
// still skips C++ unwinding.
//
// Implications for destroyContext():
//
//   * **Library frames are safe.** `pthread_coro_thread_main` and the
//     `contextSwitch` path explicitly drop their `fl::unique_lock` /
//     `fl::lock_guard` before reaching `pthread_exit(nullptr)`, so no
//     `fl::mutex` is leaked through the teardown path. The pthread itself
//     is `join()`-ed afterwards, so the OS thread is fully reclaimed.
//
//   * **User coroutine stack frames are NOT safe.** Any RAII-managed
//     resource live on the user coroutine's stack at the moment
//     destroyContext() asks the worker to exit — fl::unique_ptr slots,
//     heap allocations held by smart pointers, file handles, lock guards
//     in user code, etc. — will be abandoned without their destructors
//     running. That is a real leak: the underlying allocations / handles
//     are NOT released.
//
// Caller guidance:
//
//   1. Prefer cooperative shutdown. Have the coroutine body poll a "should
//      stop" condition (e.g. `CoroutineContext::should_stop()` or an
//      application-owned flag) and return through its normal exit path
//      *before* destroyContext() is invoked. That path runs full RAII.
//   2. If a hard teardown is unavoidable, keep ownership of long-lived
//      resources on the OWNER (the object that calls destroyContext()) and
//      hand only borrowed references / value copies into the coroutine.
//      Then destruction order is controlled by the owner, not the worker
//      stack.
//   3. Do not rely on stack-based locks / counters inside the coroutine to
//      release on exit — they won't.
extern "C" __attribute__((noreturn)) void pthread_exit(void* retval);

namespace fl {
namespace platforms {

/// Per-context state for the pthread-based coroutine platform.
///
/// Lifetime: the platform owns this struct directly. The worker thread holds
/// only a raw `self` pointer, but contextSwitch() observes `exit_requested`
/// after every wake and calls `pthread_exit()` so the worker is fully torn
/// down (with no further access to *this) before destroyContext() proceeds
/// with `join()`. See destroyContext() comments for the teardown protocol.
struct PthreadCoroCtx {
    fl::unique_ptr<fl::thread> thread;     ///< null for the runner context (no OS thread)
    fl::mutex mutex;                        ///< Guards `signaled` / `exit_requested`
    fl::condition_variable cv;              ///< Park spot for this thread
    bool signaled = false;                  ///< Set true to wake this context
    bool exit_requested = false;            ///< Set true to terminate the thread
    void (*entry_fn)() = nullptr;           ///< User-supplied entry trampoline
};

/// Thread body. Each non-runner context owns one of these.
/// Park until first signal, then run the entry trampoline. The trampoline
/// itself never returns (CoroutineContext::entry_trampoline busy-loops if
/// it falls through), so the *only* way this function returns is via the
/// initial-wait exit branch below. Mid-coroutine teardown is handled by
/// contextSwitch() calling pthread_exit() — the thread terminates without
/// returning here.
inline void pthread_coro_thread_main(PthreadCoroCtx* self) FL_NOEXCEPT {
    {
        fl::unique_lock<fl::mutex> lk(self->mutex);
        self->cv.wait(lk, [self] { return self->signaled; });
        self->signaled = false;
        if (self->exit_requested) {
            // Initial-wait exit — destroyContext() called before the
            // coroutine ever ran. Drop the lock and exit cleanly so
            // destroyContext()'s join() can return.
            return;
        }
    }

    if (self->entry_fn) {
        self->entry_fn();
    }
    // Unreachable: entry_fn (CoroutineContext::entry_trampoline) busy-loops
    // on its own after the final contextSwitch back to the runner.
    // Mid-coroutine teardown exits the thread via pthread_exit() inside
    // contextSwitch(), so control never returns here from entry_fn().
}

class CoroutinePlatformPthread : public ICoroutinePlatform {
public:
    void* createContext(void (*entry_fn)(), size_t /*stack_size*/) FL_NOEXCEPT override {
        auto* ctx = new PthreadCoroCtx();  // ok bare allocation - platform context lifetime
        ctx->entry_fn = entry_fn;
        ctx->thread.reset(new fl::thread(pthread_coro_thread_main, ctx));  // ok bare allocation
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
            // Teardown protocol — guarantees the worker has fully stopped
            // touching *ctx before we free it.
            //
            //   1. Set `exit_requested` and `signaled` under the lock.
            //   2. notify_all() — wakes the worker out of either its initial
            //      cv.wait() or any contextSwitch() cv.wait().
            //   3. The worker rechecks `exit_requested` after every cv.wait()
            //      and either falls off pthread_coro_thread_main (initial
            //      wait) or calls pthread_exit() (contextSwitch).
            //   4. join() blocks until that exit completes — only then is it
            //      safe to delete the thread object and *ctx.
            //
            // NB: under `-fno-exceptions`, `pthread_exit` skips C++ stack
            // unwinding. Library frames release their locks before exit (see
            // contextSwitch / pthread_coro_thread_main), but RAII objects
            // living on the *user* coroutine's stack are abandoned without
            // destructors. See the "Coroutine teardown contract" block at
            // the top of this file for the caller-facing implications.
            {
                fl::lock_guard<fl::mutex> lk(ctx->mutex);
                ctx->exit_requested = true;
                ctx->signaled = true;
            }
            ctx->cv.notify_all();

            if (ctx->thread->joinable()) {
                ctx->thread->join();
            }
            ctx->thread.reset();
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

        // Park until someone hands the baton back to us — or until
        // destroyContext() asks us to exit (`exit_requested`).
        bool exit_now = false;
        {
            fl::unique_lock<fl::mutex> lk(from->mutex);
            from->cv.wait(lk, [from] { return from->signaled; });
            from->signaled = false;
            exit_now = from->exit_requested;
        }
        if (exit_now) {
            // Mid-coroutine teardown. The trampoline's caller frame would
            // otherwise busy-loop (entry_trampoline's `while (true) {}`),
            // pinning the worker forever and blocking destroyContext()'s
            // join(). Terminate the thread cleanly here — no further access
            // to *from after this point, so destroyContext() may free *from
            // as soon as join() returns.
            pthread_exit(nullptr);
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
