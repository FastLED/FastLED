// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/esp/32/semaphore_esp32.h
/// @brief ESP32 FreeRTOS semaphore implementation
///
/// This header provides ESP32-specific semaphore implementations using FreeRTOS semaphores.
/// Provides both counting semaphores and binary semaphores for thread synchronization.

#include "fl/stl/assert.h"
#include "fl/stl/cstddef.h"

// CRITICAL: Include <chrono> BEFORE opening namespace to avoid polluting std::
#include <chrono>  // ok include

namespace fl {
namespace platforms {

// Forward declaration
template<ptrdiff_t LeastMaxValue>
class CountingSemaphoreESP32;

// Platform implementation aliases for ESP32
template<ptrdiff_t LeastMaxValue = 1>
using counting_semaphore = CountingSemaphoreESP32<LeastMaxValue>;

using binary_semaphore = CountingSemaphoreESP32<1>;

/// @brief ESP32 FreeRTOS counting semaphore wrapper
///
/// This class provides a counting semaphore implementation using FreeRTOS primitives.
/// Compatible with the C++20 std::counting_semaphore interface.
///
/// @tparam LeastMaxValue The maximum value the semaphore can hold
template<ptrdiff_t LeastMaxValue = 1>
class CountingSemaphoreESP32 {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer to avoid including FreeRTOS headers)

public:
    /// @brief Construct a counting semaphore with an initial count
    /// @param desired Initial count (must be >= 0 and <= LeastMaxValue)
    explicit CountingSemaphoreESP32(ptrdiff_t desired);

    ~CountingSemaphoreESP32();

    // Non-copyable and non-movable
    CountingSemaphoreESP32(const CountingSemaphoreESP32&) = delete;
    CountingSemaphoreESP32& operator=(const CountingSemaphoreESP32&) = delete;
    CountingSemaphoreESP32(CountingSemaphoreESP32&&) = delete;
    CountingSemaphoreESP32& operator=(CountingSemaphoreESP32&&) = delete;

    /// @brief Increment the semaphore count by update
    /// @param update Number to add to the count (default 1)
    void release(ptrdiff_t update = 1);

    /// @brief Decrement the semaphore count, blocking if count is 0
    void acquire();

    /// @brief Try to decrement the semaphore count without blocking
    /// @return true if successful, false if count was 0
    bool try_acquire();

    /// @brief Try to acquire with a timeout
    /// @tparam Rep Duration representation type
    /// @tparam Period Duration period type
    /// @param rel_time Maximum time to wait
    /// @return true if acquired within timeout, false otherwise
    template<class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time);  // okay std namespace

    /// @brief Try to acquire until an absolute time point
    /// @tparam Clock Clock type
    /// @tparam Duration Duration type
    /// @param abs_time Absolute time point to wait until
    /// @return true if acquired before timeout, false otherwise
    template<class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time);  // okay std namespace

    /// @brief Get the maximum value the semaphore can hold
    /// @return LeastMaxValue
    static constexpr ptrdiff_t max() noexcept {
        return LeastMaxValue;
    }
};

// Define FASTLED_MULTITHREADED for ESP32 (has FreeRTOS)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
