#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/atomic.h"
#include "fl/stl/functional.h"
#include "fl/int.h"
#include "fl/singleton.h"
#include "fl/engine_events.h"

namespace fl {
namespace platforms {

/// @brief Platform-specific coroutine runtime interface
///
/// Provides coroutine pumping and shutdown coordination.
/// Registers as an EngineEvents::Listener to receive onExit() shutdown signal.
///
/// Platform implementations:
/// - ESP32: Yields to FreeRTOS scheduler via vTaskDelay()
/// - Teensy: Pumps cooperative coroutine runner or yields from coroutine
/// - Stub/Host: Pumps thread-based coroutine runner
/// - Arduino/WASM: No-op (no background coroutines)
class ICoroutineRuntime : public EngineEvents::Listener {
public:
    static ICoroutineRuntime& instance();

    ICoroutineRuntime() {
        EngineEvents::addListener(this);
    }

    ~ICoroutineRuntime() override {
        EngineEvents::removeListener(this);
    }

    /// Give CPU time to background coroutines/tasks for up to `us` microseconds.
    /// Called from main-thread contexts (async_run, delay).
    /// No-op on platforms without background coroutines (Arduino, WASM).
    /// @param us Microseconds budget for background work
    virtual void pumpCoroutines(fl::u32 us) = 0;

    /// Yield the current execution context for approximately `us` microseconds.
    /// Safe to call from ANY context (main thread, coroutine thread, worker thread).
    /// Used by fl::platforms::await() to yield while polling a promise.
    /// Default delegates to pumpCoroutines(). Override on platforms
    /// where pumpCoroutines() is not safe from worker threads (e.g. Stub).
    virtual void suspendMainthread(fl::u32 us) {
        pumpCoroutines(us);
    }

    /// Check if shutdown has been requested
    /// Background threads should poll this and exit early when true.
    bool isShutdownRequested() { return mShutdownRequested.load(); }

    /// Request shutdown — background threads will see this via isShutdownRequested()
    void requestShutdown() { mShutdownRequested.store(true); }

    /// Reset shutdown flag — call after cleanup to allow new threads
    void resetShutdown() { mShutdownRequested.store(false); }

    /// EngineEvents::Listener callback — sets shutdown flag on engine exit
    void onExit() override { requestShutdown(); }

private:
    fl::atomic<bool> mShutdownRequested{false};
};

/// @brief Platform abstraction for cooperative coroutine context switching
///
/// Implementations provide ONLY the hardware/OS-specific mechanics:
///   - Stack allocation + initial frame setup
///   - Register save/restore + stack pointer swap
///   - Interrupt detection (if applicable)
///   - Timer for scheduling time budgets
///
/// Everything else (scheduling, lifecycle, guards, await) is generic code
/// in CoroutineContext/CoroutineRunner (below) that calls this interface.
///
/// The void* context handle is opaque — each platform decides what it is:
///   - Teensy ARM:  struct with saved SP + stack base
///   - Windows:     LPVOID fiber handle
///   - Linux:       ucontext_t*
///
/// Platforms with cooperative coroutines (Teensy, test fibers) call
/// setInstance() to register their implementation. All other platforms
/// get a NullCoroutinePlatform (isAvailable() == false).
class ICoroutinePlatform {
public:
    virtual ~ICoroutinePlatform() = default;

    /// Returns true on platforms that provide cooperative coroutine support.
    /// NullCoroutinePlatform returns false — callers should check this
    /// before attempting to create or switch coroutine contexts.
    virtual bool isAvailable() const { return true; }

    /// One-time setup (e.g., Windows: ConvertThreadToFiber)
    virtual void init() {}

    /// Create a coroutine context. When first switched to,
    /// execution begins at entry_fn().
    /// Returns opaque handle, or nullptr on failure.
    virtual void* createContext(void (*entry_fn)(),
                                size_t stack_size) = 0;

    /// Create a runner context (main thread save slot, no stack allocation).
    virtual void* createRunnerContext() = 0;

    /// Destroy a context and free its resources.
    virtual void destroyContext(void* ctx) = 0;

    /// Save current state into from_ctx, restore state from to_ctx.
    /// Returns when another contextSwitch targets from_ctx.
    virtual void contextSwitch(void* from_ctx, void* to_ctx) = 0;

    /// Guard: is the caller in an interrupt/exception handler?
    /// Used by suspend()/resume() to prevent context switches from ISRs.
    ///
    /// On bare-metal ARM (Teensy), ISRs can preempt any code — including
    /// code paths that reach suspend()/resume() (e.g. fl::delay() →
    /// pumpCoroutines() → CoroutineRunner::run() → resume()). A context
    /// switch inside an ISR would corrupt CPU state because ISRs use a
    /// different stack and register save convention. The Teensy override
    /// checks __get_IPSR() != 0 to detect this; suspend()/resume() then
    /// silently return instead of crashing.
    ///
    /// Default returns false — correct for platforms without hardware ISRs
    /// (Windows fibers, Linux ucontext, Stub threads).
    virtual bool inInterruptContext() const { return false; }

    /// Check coroutine stack health (canary, guard page, etc.)
    /// Returns false if corruption detected.
    virtual bool checkStackHealth(void* ctx) const {
        (void)ctx;
        return true;
    }

    /// Platform time source in microseconds (for scheduling time budgets).
    virtual fl::u32 micros() const = 0;

    /// Get the active coroutine platform instance.
    /// Returns NullCoroutinePlatform if no platform has registered via setInstance().
    static ICoroutinePlatform& instance();

    /// Register a platform implementation. Platforms with cooperative
    /// coroutines (Teensy) call this during static initialization.
    /// Tests can call this to inject a test platform (e.g. Windows fibers).
    static void setInstance(ICoroutinePlatform* p);
};

/// @brief No-op coroutine platform for platforms without cooperative coroutines
///
/// All methods are safe to call but do nothing useful:
/// - isAvailable() returns false
/// - createContext() returns nullptr
/// - contextSwitch() is a no-op
class NullCoroutinePlatform : public ICoroutinePlatform {
public:
    bool isAvailable() const override { return false; }
    void* createContext(void (*)(), size_t) override { return nullptr; }
    void* createRunnerContext() override { return nullptr; }
    void destroyContext(void*) override {}
    void contextSwitch(void*, void*) override {}
    fl::u32 micros() const override { return 0; }
};

//=============================================================================
// Generic Cooperative Coroutine Context and Runner
//=============================================================================

/// @brief Per-coroutine execution context (generic)
///
/// Holds an opaque platform context handle and the user's function.
/// All platform-specific operations go through ICoroutinePlatform.
class CoroutineContext {
public:
    /// @brief Create a new coroutine context
    /// @param func The coroutine function to execute
    /// @param stack_size Stack size in bytes (platform may enforce minimum)
    /// @return Pointer to new context, or nullptr on failure
    static CoroutineContext* create(fl::function<void()> func,
                                    size_t stack_size = 4096);

    ~CoroutineContext();

    // Non-copyable
    CoroutineContext(const CoroutineContext&) = delete;
    CoroutineContext& operator=(const CoroutineContext&) = delete;

    /// @brief Resume this coroutine from the main thread
    void resume();

    /// @brief Suspend this coroutine — switch back to the runner (called from within coroutine)
    static void suspend();

    /// @brief Check if the caller is inside a coroutine (vs on the main thread)
    static bool isInsideCoroutine() { return runningCoroutine() != nullptr; }

    /// @brief Get the currently running coroutine (nullptr if on the main thread)
    static CoroutineContext* runningCoroutine();

    /// @brief Check if coroutine has completed (function returned)
    bool is_completed() const { return mCompleted; }

    /// @brief Request the coroutine to stop
    void set_should_stop(bool val) { mShouldStop = val; }

    /// @brief Check if stop has been requested
    bool should_stop() const { return mShouldStop; }

    /// @brief Force-stop: sets both should_stop and completed flags.
    /// Used by runner and task wrappers to immediately mark a coroutine as done.
    void stop_and_complete() {
        mShouldStop = true;
        mCompleted = true;
    }

    /// @brief Ready predicate function signature
    using ReadyFn = bool (*)(void*);

    /// @brief Set a ready predicate — runner skips this coroutine while it returns false
    void set_ready_fn(ReadyFn fn, void* arg) {
        mReadyFn = fn;
        mReadyArg = arg;
    }

    /// @brief Clear the ready predicate (coroutine always ready)
    void clear_ready_fn() {
        mReadyFn = nullptr;
        mReadyArg = nullptr;
    }

    /// @brief Check if this coroutine is ready to be resumed
    bool is_ready() const { return !mReadyFn || mReadyFn(mReadyArg); }

private:
    CoroutineContext() = default;

    /// Entry point trampoline (calls mFunction, marks completed, switches back)
    static void entry_trampoline();

    void* mPlatformCtx = nullptr;     ///< Opaque platform context handle
    fl::function<void()> mFunction;   ///< User function
    ReadyFn mReadyFn = nullptr;       ///< Optional ready predicate
    void* mReadyArg = nullptr;        ///< Argument passed to mReadyFn
    bool mCompleted = false;          ///< True when function has returned
    bool mShouldStop = false;         ///< Stop requested
    bool mStarted = false;            ///< True after first resume
};

/// @brief Global coroutine runner — manages work queue of coroutines
///
/// Singleton that maintains a circular queue of active coroutines.
/// The main thread calls run() to give time to coroutines.
class CoroutineRunner {
public:
    /// @brief Get singleton instance
    static CoroutineRunner& instance();

    /// @brief Add a coroutine to the work queue
    void enqueue(CoroutineContext* ctx);

    /// @brief Remove a coroutine from the work queue
    void remove(CoroutineContext* ctx);

    /// @brief Run coroutines for up to `us` microseconds
    void run(fl::u32 us);

    /// @brief Stop all queued coroutines
    void stop_all();

private:
    template <typename, int> friend class fl::Singleton;
    CoroutineRunner() = default;

    static constexpr size_t kMaxCoroutines = 16;
    CoroutineContext* mQueue[kMaxCoroutines] = {};
    size_t mCount = 0;
    size_t mNextIndex = 0;
};

}  // namespace platforms
}  // namespace fl
