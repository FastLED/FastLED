// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/stm32/semaphore_stm32.h
/// @brief STM32 FreeRTOS semaphore implementation
///
/// This header provides STM32-specific semaphore implementations using FreeRTOS semaphores.
/// Provides both counting semaphores and binary semaphores for thread synchronization.
/// Only available when FreeRTOS is detected on STM32 platforms.

#include "fl/stl/assert.h"
#include "fl/stl/cstddef.h"

// Forward declare std::chrono types to avoid including <chrono> in header
namespace std {
    namespace chrono {
        template<typename Rep, typename Period> class duration;
        template<typename Clock, typename Duration> class time_point;
    }
}

namespace fl {
namespace platforms {

// Forward declaration
template<ptrdiff_t LeastMaxValue>
class CountingSemaphoreSTM32;

// Platform implementation aliases for STM32
template<ptrdiff_t LeastMaxValue = 1>
using counting_semaphore = CountingSemaphoreSTM32<LeastMaxValue>;

using binary_semaphore = CountingSemaphoreSTM32<1>;

/// @brief STM32 FreeRTOS counting semaphore wrapper
///
/// This class provides a counting semaphore implementation using FreeRTOS primitives.
/// Compatible with the C++20 std::counting_semaphore interface.
///
/// @tparam LeastMaxValue The maximum value the semaphore can hold
template<ptrdiff_t LeastMaxValue = 1>
class CountingSemaphoreSTM32 {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer to avoid including FreeRTOS headers)

public:
    /// @brief Construct a counting semaphore with an initial count
    /// @param desired Initial count (must be >= 0 and <= LeastMaxValue)
    explicit CountingSemaphoreSTM32(ptrdiff_t desired);

    ~CountingSemaphoreSTM32();

    // Non-copyable and non-movable
    CountingSemaphoreSTM32(const CountingSemaphoreSTM32&) = delete;
    CountingSemaphoreSTM32& operator=(const CountingSemaphoreSTM32&) = delete;
    CountingSemaphoreSTM32(CountingSemaphoreSTM32&&) = delete;
    CountingSemaphoreSTM32& operator=(CountingSemaphoreSTM32&&) = delete;

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

// Define FASTLED_MULTITHREADED for STM32 with FreeRTOS
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
