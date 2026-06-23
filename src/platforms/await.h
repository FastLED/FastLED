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
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

/// @brief Await promise completion using platform-agnostic polling
/// @tparam T The type of value the promise resolves to
/// @param promise The promise to await
/// @return A PromiseResult<T> containing either the resolved value or an error
///
/// Polls the promise in a loop, yielding to the platform scheduler between
/// checks via ICoroutineRuntime::suspendMainthread(). This method is safe to call
/// from any execution context (main thread, coroutine, worker thread).
template<typename T>
fl::task::PromiseResult<T> await(fl::task::Promise<T> promise) FL_NOEXCEPT {
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

    // Poll until promise completes, yielding to platform scheduler
    while (!promise.is_completed()) {
        promise.update();
        if (promise.is_completed()) break;
        runtime.suspendMainthread(1000);  // Yield ~1ms
    }

    // Promise completed, return result
    return promise.is_resolved()
        ? fl::task::PromiseResult<T>(promise.value())
        : fl::task::PromiseResult<T>(promise.error());
}

} // namespace platforms
} // namespace fl
