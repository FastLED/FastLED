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
/// This header is included from fl/stl/async.h. The public API fl::await() in
/// fl/stl/async.h acts as a trampoline that delegates to fl::platforms::await().

#include "fl/promise.h"
#include "fl/promise_result.h"
#include "platforms/coroutine_runtime.h"

namespace fl {
namespace platforms {

/// @brief Await promise completion using platform-agnostic polling
/// @tparam T The type of value the promise resolves to
/// @param promise The promise to await
/// @return A result<T> containing either the resolved value or an error
///
/// Polls the promise in a loop, yielding to the platform scheduler between
/// checks via ICoroutineRuntime::suspendMainthread(). This method is safe to call
/// from any execution context (main thread, coroutine, worker thread).
template<typename T>
fl::result<T> await(fl::promise<T> promise) {
    // Validate promise
    if (!promise.valid()) {
        return fl::result<T>(Error("Invalid promise"));
    }

    // If already completed, return immediately
    if (promise.is_completed()) {
        return promise.is_resolved()
            ? fl::result<T>(promise.value())
            : fl::result<T>(promise.error());
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
        ? fl::result<T>(promise.value())
        : fl::result<T>(promise.error());
}

} // namespace platforms
} // namespace fl
