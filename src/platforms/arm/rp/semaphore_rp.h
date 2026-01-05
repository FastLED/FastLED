// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/rp/semaphore_rp.h
/// @brief RP2040/RP2350 Pico SDK semaphore implementation
///
/// This header provides RP2040/RP2350-specific semaphore implementations using Pico SDK spinlocks.
/// Provides both counting semaphores and binary semaphores for dual-core synchronization.

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
class CountingSemaphoreRP;

// Platform implementation aliases for RP2040/RP2350
template<ptrdiff_t LeastMaxValue = 1>
using counting_semaphore = CountingSemaphoreRP<LeastMaxValue>;

using binary_semaphore = CountingSemaphoreRP<1>;

/// @brief RP2040/RP2350 Pico SDK counting semaphore wrapper
///
/// This class provides a counting semaphore implementation using Pico SDK hardware spinlocks
/// and atomic operations. Compatible with the C++20 std::counting_semaphore interface.
/// Designed for dual-core synchronization on RP2040/RP2350 platforms.
///
/// @tparam LeastMaxValue The maximum value the semaphore can hold
template<ptrdiff_t LeastMaxValue = 1>
class CountingSemaphoreRP {
private:
    void* mSpinlock;     // spin_lock_t* (opaque pointer to avoid including Pico SDK headers)
    ptrdiff_t mCount;    // Current semaphore count (protected by spinlock)
    ptrdiff_t mMaxValue; // Maximum value (LeastMaxValue)

public:
    /// @brief Construct a counting semaphore with an initial count
    /// @param desired Initial count (must be >= 0 and <= LeastMaxValue)
    explicit CountingSemaphoreRP(ptrdiff_t desired);

    ~CountingSemaphoreRP();

    // Non-copyable and non-movable
    CountingSemaphoreRP(const CountingSemaphoreRP&) = delete;
    CountingSemaphoreRP& operator=(const CountingSemaphoreRP&) = delete;
    CountingSemaphoreRP(CountingSemaphoreRP&&) = delete;
    CountingSemaphoreRP& operator=(CountingSemaphoreRP&&) = delete;

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

// Define FASTLED_MULTITHREADED for RP2040/RP2350 (has dual-core support)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
