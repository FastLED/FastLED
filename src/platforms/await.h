// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/await.h
/// @brief Platform dispatch for coroutine-based await() implementations
///
/// This header provides platform-specific implementations in fl::platforms::await()
/// for platforms that support true OS-level blocking (ESP32, Host/Stub).
///
/// Supported platforms:
/// - ESP32: FreeRTOS task notifications
/// - Host/Stub: fl::condition_variable
/// - Other: No await() support (use fl::await_top_level() instead)
///
/// This header is included from fl/async.h. The public API fl::await() in
/// fl/async.h acts as a trampoline that delegates to fl::platforms::await().

#include "fl/promise.h"
#include "fl/promise_result.h"

namespace fl {
namespace platforms {

// ============================================================================
// ESP32: True Blocking Await in Coroutines (FreeRTOS Task Notifications)
// ============================================================================

#ifdef ESP32

} // namespace platforms
} // namespace fl

// FreeRTOS includes must be included OUTSIDE namespaces
FL_EXTERN_C_BEGIN
#include "freertos/FreeRTOS.h"  // ok include
#include "freertos/task.h"       // ok include
FL_EXTERN_C_END

namespace fl {
namespace platforms {

/// @brief Await promise completion in a coroutine (ESP32 only, true blocking)
/// @tparam T The type of value the promise resolves to
/// @param promise The promise to await
/// @return A result<T> containing either the resolved value or an error
///
/// Implementation uses FreeRTOS task notifications for efficient suspension.
/// This is called by fl::await() as a trampoline. See fl/async.h for full documentation.
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

    // Get current FreeRTOS task handle
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    // Register completion callbacks to wake this task
    promise.then([current_task](const T&) {
        xTaskNotifyGive(current_task);  // Wake the blocked task
    }).catch_([current_task](const Error&) {
        xTaskNotifyGive(current_task);  // Wake on error too
    });

    // Block this coroutine until promise completes
    // The OS scheduler will run other tasks (zero CPU waste)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Promise completed, return result
    return promise.is_resolved()
        ? fl::result<T>(promise.value())
        : fl::result<T>(promise.error());
}

#endif // ESP32

// ============================================================================
// Host/Stub: True Blocking Await using fl::condition_variable
// ============================================================================

#ifdef FASTLED_STUB_IMPL

} // namespace platforms
} // namespace fl

// Host-platform includes must be OUTSIDE namespaces
#include "fl/stl/condition_variable.h"  // fl::condition_variable
#include "fl/stl/mutex.h"  // fl::mutex and fl::unique_lock (aliases std::mutex/std::unique_lock in multithreaded mode)
#include "platforms/stub/coroutine_runner.h"  // Global coordination

namespace fl {
namespace platforms {

/// @brief Await promise completion in a coroutine (Host platform using std::thread)
/// @tparam T The type of value the promise resolves to
/// @param promise The promise to await
/// @return A result<T> containing either the resolved value or an error
///
/// Implementation uses fl::condition_variable for efficient suspension.
/// This is called by fl::await() as a trampoline. See fl/async.h for full documentation.
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

    // Create synchronization primitives for local coordination
    // Note: fl::mutex is MutexReal (inherits std::mutex) in multithreaded mode
    // fl::unique_lock is aliased to std::unique_lock in multithreaded mode for
    // compatibility with fl::condition_variable
    fl::mutex mtx;
    fl::condition_variable cv;
    fl::atomic<bool> completed(false);

    // Register completion callbacks
    promise.then([&](const T&) {
        completed.store(true);
        cv.notify_one();
    }).catch_([&](const Error&) {
        completed.store(true);
        cv.notify_one();
    });

    // Release global execution lock before waiting
    // This allows other coroutines and main thread to run while we wait
    fl::detail::global_execution_unlock();

    // Signal next coroutine in executor queue to run while we wait
    fl::detail::CoroutineRunner::instance().signal_next();

    // Wait on local condition variable for promise completion
    // fl::unique_lock<fl::mutex> is fully compatible with fl::condition_variable because:
    // - In multithreaded mode: fl::unique_lock = std::unique_lock, fl::mutex = std::mutex
    fl::unique_lock<fl::mutex> local_lock(mtx);
    cv.wait(local_lock, [&]() { return completed.load(); });
    local_lock.unlock();  // Release local lock

    // Re-acquire global execution lock before returning to user code
    // This ensures only one thread executes "user code" at a time
    fl::detail::global_execution_lock();

    // Promise completed and we now hold global lock again, return result
    return promise.is_resolved()
        ? fl::result<T>(promise.value())
        : fl::result<T>(promise.error());
}

#endif // FASTLED_STUB_IMPL

// ============================================================================
// Unsupported Platforms: Static Assert on Instantiation
// ============================================================================

#if !defined(ESP32) && !defined(FASTLED_STUB_IMPL)

/// @brief No-op await() for platforms without coroutine support
/// @tparam T The type of value the promise resolves to
/// @param promise The promise to await (unused)
/// @return Never returns - triggers static_assert on instantiation
///
/// This version provides a clear compile-time error when await() is used
/// on platforms that don't support true OS-level blocking.
template<typename T>
fl::result<T> await(fl::promise<T> promise) {
    static_assert(sizeof(T) == 0,
        "fl::await() is not supported on this platform. "
        "Use fl::await_top_level() instead, or enable coroutine support "
        "(available on ESP32 with FreeRTOS, or Host/Stub platforms).");
    (void)promise;  // Suppress unused parameter warning
    return fl::result<T>(Error("Unsupported platform"));
}

#endif // !ESP32 && !FASTLED_STUB_IMPL

} // namespace platforms
} // namespace fl
