// IWYU pragma: private

/// @file coroutine_context.cpp.hpp
/// @brief Generic coroutine context and runner implementation
///
/// Always compiled. On platforms without cooperative coroutines,
/// ICoroutinePlatform::instance() returns NullCoroutinePlatform
/// (isAvailable() == false) and create() returns nullptr.
/// Platforms register via ICoroutinePlatform::setInstance().

#include "platforms/coroutine_runtime.h"
#include "fl/stl/singleton.h"
#include "fl/warn.h"

namespace fl {
namespace platforms {

//=============================================================================
// ICoroutinePlatform singleton — settable, defaults to NullCoroutinePlatform
//=============================================================================

static ICoroutinePlatform* sCoroutinePlatformInstance = nullptr;

ICoroutinePlatform& ICoroutinePlatform::instance() {
    if (sCoroutinePlatformInstance) {
        return *sCoroutinePlatformInstance;
    }
    return fl::Singleton<NullCoroutinePlatform>::instance();
}

void ICoroutinePlatform::setInstance(ICoroutinePlatform* p) {
    sCoroutinePlatformInstance = p;
}

//=============================================================================
// Global coroutine state
//=============================================================================

/// @brief Global coroutine state — runner context and current coroutine pointer.
struct CoroutineGlobals {
    void* runner_ctx = nullptr;        ///< Platform handle for runner (main thread)
    CoroutineContext* current = nullptr; ///< Currently executing coroutine
};

static CoroutineGlobals& globals() {
    return fl::Singleton<CoroutineGlobals>::instance();
}

/// @brief Lazy-init the runner's platform context (created on first use)
static void* get_runner_ctx() {
    auto& g = globals();
    if (!g.runner_ctx) {
        g.runner_ctx = ICoroutinePlatform::instance().createRunnerContext();
    }
    return g.runner_ctx;
}

//=============================================================================
// CoroutineContext Implementation
//=============================================================================

void CoroutineContext::entry_trampoline() {
    CoroutineContext* self = globals().current;
    if (self && self->mFunction) {
        self->mFunction();
    }

    // Mark completed and switch back to runner
    if (self) {
        self->mCompleted = true;
    }

    void* runner = get_runner_ctx();
    if (runner && self) {
        ICoroutinePlatform::instance().contextSwitch(
            self->mPlatformCtx, runner);
    }

    // Should never reach here
    while (true) {}
}

CoroutineContext* CoroutineContext::create(fl::function<void()> func,
                                           size_t stack_size) {
    auto& platform = ICoroutinePlatform::instance();

    void* pctx = platform.createContext(entry_trampoline, stack_size);
    if (!pctx) {
        return nullptr;
    }

    auto* ctx = new CoroutineContext();  // ok bare allocation
    ctx->mPlatformCtx = pctx;
    ctx->mFunction = fl::move(func);

    return ctx;
}

CoroutineContext::~CoroutineContext() {
    // Defensive: remove from runner queue to prevent dangling pointer
    CoroutineRunner::instance().remove(this);

    if (mPlatformCtx) {
        ICoroutinePlatform::instance().destroyContext(mPlatformCtx);
        mPlatformCtx = nullptr;
    }
}

void CoroutineContext::resume() {
    if (mCompleted) {
        return;
    }

    auto& platform = ICoroutinePlatform::instance();

    // ISR guard — context switching from interrupt handler corrupts state
    if (platform.inInterruptContext()) {
        return;
    }

    // Re-entrancy guard: if another coroutine is already running (e.g. a
    // coroutine called suspend() which re-entered run()), skip. There's
    // only one runner context save slot — a nested contextSwitch would
    // overwrite it.
    if (globals().current != nullptr) {
        return;
    }

    globals().current = this;
    mStarted = true;

    void* runner = get_runner_ctx();
    platform.contextSwitch(runner, mPlatformCtx);

    // We're back — coroutine suspended or completed
    globals().current = nullptr;
}

CoroutineContext* CoroutineContext::runningCoroutine() {
    return globals().current;
}

void CoroutineContext::suspend() {
    auto& platform = ICoroutinePlatform::instance();

    // ISR guard
    if (platform.inInterruptContext()) {
        return;
    }

    CoroutineContext* self = globals().current;
    if (!self) {
        return;  // Not inside a coroutine
    }

    void* runner = get_runner_ctx();
    if (!runner) {
        return;
    }

    // Check stack health before switching — catch overflow early
    if (!platform.checkStackHealth(self->mPlatformCtx)) {
        FL_WARN("FATAL: Stack overflow detected in coroutine");
        while (true) {}  // Halt — memory is already corrupted
    }

    // Switch back to runner
    platform.contextSwitch(self->mPlatformCtx, runner);

    // Resumed — we're back in the coroutine
}

//=============================================================================
// CoroutineRunner Implementation
//=============================================================================

CoroutineRunner& CoroutineRunner::instance() {
    return fl::Singleton<CoroutineRunner>::instance();
}

void CoroutineRunner::enqueue(CoroutineContext* ctx) {
    if (!ctx) return;

    // Check for duplicates
    for (size_t i = 0; i < mCount; ++i) {
        if (mQueue[i] == ctx) return;
    }

    if (mCount >= kMaxCoroutines) {
        FL_WARN("CoroutineRunner: Queue full, cannot enqueue");
        return;
    }

    mQueue[mCount++] = ctx;
}

void CoroutineRunner::remove(CoroutineContext* ctx) {
    for (size_t i = 0; i < mCount; ++i) {
        if (mQueue[i] == ctx) {
            // Shift remaining entries down
            for (size_t j = i; j + 1 < mCount; ++j) {
                mQueue[j] = mQueue[j + 1];
            }
            --mCount;
            mQueue[mCount] = nullptr;

            // Adjust next index if needed
            if (mNextIndex > i && mNextIndex > 0) {
                --mNextIndex;
            }
            if (mNextIndex >= mCount && mCount > 0) {
                mNextIndex = 0;
            }
            return;
        }
    }
}

void CoroutineRunner::run(fl::u32 us) {
    if (mCount == 0) return;

    auto& platform = ICoroutinePlatform::instance();
    fl::u32 start = platform.micros();

    // Run at least one coroutine, then keep going until time budget exhausted
    bool first = true;
    while (first || (platform.micros() - start) < us) {
        first = false;

        if (mCount == 0) break;

        // Pick next coroutine (round-robin)
        if (mNextIndex >= mCount) {
            mNextIndex = 0;
        }

        CoroutineContext* ctx = mQueue[mNextIndex];
        ++mNextIndex;

        if (!ctx) continue;

        // Remove completed or stopped coroutines
        if (ctx->is_completed()) {
            remove(ctx);
            continue;
        }

        if (ctx->should_stop()) {
            ctx->stop_and_complete();
            remove(ctx);
            continue;
        }

        // Skip coroutines that aren't ready (e.g. awaiting a promise)
        if (!ctx->is_ready()) {
            continue;
        }

        // Resume this coroutine — it will run until it suspends
        ctx->resume();
    }
}

void CoroutineRunner::stop_all() {
    for (size_t i = 0; i < mCount; ++i) {
        if (mQueue[i]) {
            mQueue[i]->stop_and_complete();
            mQueue[i] = nullptr;
        }
    }
    mCount = 0;
    mNextIndex = 0;
}

} // namespace platforms
} // namespace fl
