// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/await.h
/// @brief Platform-independent await() implementation for coroutines
///
/// Uses ICoroutineRuntime::suspendMainthread() to yield in a platform-agnostic way.
/// Each platform's runtime handles the appropriate yielding mechanism:
///   - ESP32: vTaskDelay to yield to FreeRTOS scheduler
///   - Teensy: Cooperative yield or pump coroutine runner
///   - Host/Stub: Thread sleep (safe from worker threads)
///   - Arduino: delayMicroseconds
///
/// This header is included from fl/task/executor.h. The public API fl::task::await()
/// delegates to fl::platforms::await().

#include "fl/task/promise.h"
#include "fl/task/promise_result.h"
#include "platforms/coroutine_runtime.h"
#include "fl/stl/chrono.h"
#include "fl/stl/noexcept.h"
#include "fl/system/timeout.h"

namespace fl {
namespace platforms {

/// @brief Default wall-clock bound for `await()`, in milliseconds.
///
/// `fl::task::await_top_level` caps the same kind of wait at 10000 pump
/// iterations, which at that loop's ~1 ms yield is about ten seconds. This is
/// the same budget stated as wall clock, which is what it is really bounding:
/// an iteration count buys a different amount of time on every platform,
/// depending on what `suspendMainthread()` does with its microsecond budget.
///
/// Generous on purpose. `await()` exists to block on genuinely slow work --
/// `fl::task::await(fl::fetch_get(...))` is the documented use -- so the
/// default has to cover a real network round trip. A caller that needs longer
/// passes a larger budget.
constexpr fl::u32 kAwaitDefaultTimeoutMs = 10000;

/// @brief Await promise completion using platform-agnostic polling
/// @tparam T The type of value the promise resolves to
/// @param promise The promise to await
/// @param timeout_ms Wall-clock budget before the wait gives up and returns an
///        error. There is deliberately no "wait forever" value: a caller with
///        slow work passes a larger budget, which keeps the failure mode a
///        reported error rather than a hang.
/// @return A PromiseResult<T> containing either the resolved value or an error
///
/// Polls the promise in a loop, yielding to the platform scheduler between
/// checks via ICoroutineRuntime::suspendMainthread(). This method is safe to call
/// from any execution context (main thread, coroutine, worker thread).
///
/// The wait is bounded. It used to have no cap of any kind, so a promise that
/// could not be resolved -- the easiest way in being a platform whose
/// coroutine backend is the null one, where the coroutine that would resolve
/// it never runs -- never returned to its caller. On a microcontroller that
/// stops the sketch until the watchdog resets the board. `PromiseResult<T>`
/// already carries an error and `fl::task::await_top_level` already returns
/// one on exhaustion; this is the same policy on the same kind of wait.
/// See FastLED#4369.
template<typename T>
fl::task::PromiseResult<T> await(
    fl::task::Promise<T> promise,
    fl::u32 timeout_ms = kAwaitDefaultTimeoutMs) FL_NO_EXCEPT {
    // Validate promise
    if (!promise.valid()) {
        return fl::task::PromiseResult<T>(fl::task::Error("Invalid promise"));
    }

    // If already completed, return immediately
    if (promise.is_completed()) {
        return promise.is_resolved()
            ? fl::task::PromiseResult<T>(promise.value())
            : fl::task::PromiseResult<T>(promise.error());
    }

    auto& runtime = ICoroutineRuntime::instance();

    // Poll until the promise completes or the budget runs out, yielding to the
    // platform scheduler in between. The deadline is checked after update()
    // and before the yield, so a zero budget still gets one poll rather than
    // failing a promise that was about to complete.
    const fl::Timeout deadline(fl::millis(), timeout_ms);
    while (!promise.is_completed()) {
        promise.update();
        if (promise.is_completed()) break;
        if (deadline.done(fl::millis())) {
            return fl::task::PromiseResult<T>(
                fl::task::Error("await timeout - promise did not complete"));
        }
        runtime.suspendMainthread(1000);  // Yield ~1ms
    }

    // Promise completed, return result
    return promise.is_resolved()
        ? fl::task::PromiseResult<T>(promise.value())
        : fl::task::PromiseResult<T>(promise.error());
}

} // namespace platforms
} // namespace fl
