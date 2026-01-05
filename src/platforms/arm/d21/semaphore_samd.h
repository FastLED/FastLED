// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/d21/semaphore_samd.h
/// @brief SAMD21/SAMD51 interrupt-based semaphore implementation
///
/// This header provides SAMD-specific semaphore implementations using CMSIS
/// interrupt control for ISR-safe critical sections. Since SAMD21/SAMD51 are
/// bare metal platforms without threading, this provides basic synchronization
/// primitives for ISR/main thread coordination.
///
/// Architecture:
/// - SAMD21: ARM Cortex-M0+ (no true threading, ISR-safe critical sections)
/// - SAMD51: ARM Cortex-M4F (no true threading, ISR-safe critical sections)
///
/// Implementation approach:
/// - Uses CMSIS __disable_irq()/__enable_irq() for atomic operations
/// - Counter-based semaphore with critical section protection
/// - No blocking - single-threaded environment
/// - Warnings on acquire failures (counter is zero)

#include "fl/stl/assert.h"
#include "fl/stl/cstddef.h"
#include "fl/warn.h"

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
class CountingSemaphoreSAMD;

// Platform implementation aliases for SAMD
template<ptrdiff_t LeastMaxValue = 1>
using counting_semaphore = CountingSemaphoreSAMD<LeastMaxValue>;

using binary_semaphore = CountingSemaphoreSAMD<1>;

/// @brief SAMD interrupt-based counting semaphore
///
/// This class provides a counting semaphore implementation using CMSIS interrupt
/// control for atomic operations. Compatible with the C++20 std::counting_semaphore
/// interface, but adapted for bare metal single-threaded environment.
///
/// Since SAMD platforms are single-threaded, blocking operations would deadlock.
/// Instead, acquire() warns if the counter is zero and returns immediately.
///
/// @tparam LeastMaxValue The maximum value the semaphore can hold
template<ptrdiff_t LeastMaxValue = 1>
class CountingSemaphoreSAMD {
private:
    volatile ptrdiff_t mCounter;  // Current semaphore count (volatile for ISR safety)

public:
    /// @brief Construct a counting semaphore with an initial count
    /// @param desired Initial count (must be >= 0 and <= LeastMaxValue)
    explicit CountingSemaphoreSAMD(ptrdiff_t desired);

    ~CountingSemaphoreSAMD() = default;

    // Non-copyable and non-movable
    CountingSemaphoreSAMD(const CountingSemaphoreSAMD&) = delete;
    CountingSemaphoreSAMD& operator=(const CountingSemaphoreSAMD&) = delete;
    CountingSemaphoreSAMD(CountingSemaphoreSAMD&&) = delete;
    CountingSemaphoreSAMD& operator=(CountingSemaphoreSAMD&&) = delete;

    /// @brief Increment the semaphore count by update
    /// @param update Number to add to the count (default 1)
    void release(ptrdiff_t update = 1);

    /// @brief Decrement the semaphore count (warns if count is 0)
    /// @note In single-threaded environment, this cannot block
    void acquire();

    /// @brief Try to decrement the semaphore count without blocking
    /// @return true if successful, false if count was 0
    bool try_acquire();

    /// @brief Try to acquire with a timeout (immediate return on bare metal)
    /// @tparam Rep Duration representation type
    /// @tparam Period Duration period type
    /// @param rel_time Maximum time to wait (ignored - no blocking)
    /// @return true if acquired, false if count was 0
    template<class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time);  // okay std namespace

    /// @brief Try to acquire until an absolute time point (immediate return on bare metal)
    /// @tparam Clock Clock type
    /// @tparam Duration Duration type
    /// @param abs_time Absolute time point to wait until (ignored - no blocking)
    /// @return true if acquired, false if count was 0
    template<class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time);  // okay std namespace

    /// @brief Get the maximum value the semaphore can hold
    /// @return LeastMaxValue
    static constexpr ptrdiff_t max() noexcept {
        return LeastMaxValue;
    }
};

// Define FASTLED_MULTITHREADED=0 for SAMD (bare metal, no threading)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

} // namespace platforms
} // namespace fl
