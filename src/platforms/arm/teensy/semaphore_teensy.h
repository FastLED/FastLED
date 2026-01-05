// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/teensy/semaphore_teensy.h
/// @brief Teensy interrupt-based semaphore implementation
///
/// This header provides Teensy-specific semaphore implementations using interrupt
/// disable/restore for critical sections. Since Teensy platforms are single-core
/// bare metal (no threading), the semaphore is ISR-safe but NOT thread-safe.
///
/// IMPORTANT: These are ISR-protection primitives, NOT threading primitives.
/// - acquire() on a depleted semaphore will ASSERT/PANIC (would deadlock on single-threaded)
/// - Use try_acquire() for ISR-safe non-blocking acquisition
/// - Protects against ISR preemption using CMSIS __disable_irq()/__enable_irq()
///
/// Supported platforms:
/// - Teensy LC (ARM Cortex-M0+, 48 MHz)
/// - Teensy 3.x (ARM Cortex-M4/M4F, 48-180 MHz)
/// - Teensy 4.x (ARM Cortex-M7, 600 MHz)

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
class CountingSemaphoreTeensy;

// Platform implementation aliases for Teensy
template<ptrdiff_t LeastMaxValue = 1>
using counting_semaphore = CountingSemaphoreTeensy<LeastMaxValue>;

using binary_semaphore = CountingSemaphoreTeensy<1>;

/// @brief Teensy interrupt-based counting semaphore
///
/// This class provides a counting semaphore implementation using interrupt
/// disable/restore for critical sections. Compatible with the C++20
/// std::counting_semaphore interface, but optimized for single-core bare metal.
///
/// CRITICAL LIMITATIONS:
/// - acquire() on depleted semaphore will ASSERT (would deadlock)
/// - Use try_acquire() for safe non-blocking operation
/// - No actual blocking - this is ISR protection, not thread synchronization
///
/// @tparam LeastMaxValue The maximum value the semaphore can hold
template<ptrdiff_t LeastMaxValue = 1>
class CountingSemaphoreTeensy {
private:
    ptrdiff_t mCount;

public:
    /// @brief Construct a counting semaphore with an initial count
    /// @param desired Initial count (must be >= 0 and <= LeastMaxValue)
    explicit CountingSemaphoreTeensy(ptrdiff_t desired);

    ~CountingSemaphoreTeensy() = default;

    // Non-copyable and non-movable
    CountingSemaphoreTeensy(const CountingSemaphoreTeensy&) = delete;
    CountingSemaphoreTeensy& operator=(const CountingSemaphoreTeensy&) = delete;
    CountingSemaphoreTeensy(CountingSemaphoreTeensy&&) = delete;
    CountingSemaphoreTeensy& operator=(CountingSemaphoreTeensy&&) = delete;

    /// @brief Increment the semaphore count by update (ISR-safe)
    /// @param update Number to add to the count (default 1)
    void release(ptrdiff_t update = 1);

    /// @brief Decrement the semaphore count (PANICS if count is 0)
    /// WARNING: On single-threaded platforms, blocking would deadlock.
    /// This function will ASSERT if count is 0. Use try_acquire() for safe operation.
    void acquire();

    /// @brief Try to decrement the semaphore count without blocking (ISR-safe)
    /// @return true if successful, false if count was 0
    bool try_acquire();

    /// @brief Try to acquire with a timeout (no-op on single-threaded, same as try_acquire)
    /// @tparam Rep Duration representation type
    /// @tparam Period Duration period type
    /// @param rel_time Maximum time to wait (ignored on single-threaded)
    /// @return true if acquired, false if count was 0
    template<class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time);  // okay std namespace

    /// @brief Try to acquire until an absolute time point (no-op on single-threaded)
    /// @tparam Clock Clock type
    /// @tparam Duration Duration type
    /// @param abs_time Absolute time point to wait until (ignored on single-threaded)
    /// @return true if acquired, false if count was 0
    template<class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time);  // okay std namespace

    /// @brief Get the maximum value the semaphore can hold
    /// @return LeastMaxValue
    static constexpr ptrdiff_t max() noexcept {
        return LeastMaxValue;
    }
};

// Define FASTLED_MULTITHREADED for Teensy (single-threaded, but ISR-safe)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

} // namespace platforms
} // namespace fl
