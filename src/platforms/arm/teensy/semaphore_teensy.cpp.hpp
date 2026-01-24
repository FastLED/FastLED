/// @file semaphore_teensy.cpp
/// @brief Teensy interrupt-based semaphore platform implementation

#ifdef FL_IS_TEENSY

#include "semaphore_teensy.h"
#include "fl/warn.h"

// ARM Cortex CMSIS intrinsics for interrupt control
// __disable_irq() / __enable_irq() are available on all Teensy platforms
// (Cortex-M0+, M4, M4F, M7)
extern "C" {
    void __disable_irq(void);
    void __enable_irq(void);
}

namespace fl {
namespace platforms {

//=============================================================================
// CountingSemaphoreTeensy Implementation
//=============================================================================

template<ptrdiff_t LeastMaxValue>
CountingSemaphoreTeensy<LeastMaxValue>::CountingSemaphoreTeensy(ptrdiff_t desired)
    : mCount(desired) {
    FL_ASSERT(desired >= 0 && desired <= LeastMaxValue,
             "CountingSemaphoreTeensy: initial count out of range");
}

template<ptrdiff_t LeastMaxValue>
void CountingSemaphoreTeensy<LeastMaxValue>::release(ptrdiff_t update) {
    FL_ASSERT(update >= 0, "CountingSemaphoreTeensy: release update must be non-negative");

    // Critical section: disable interrupts
    __disable_irq();

    // Check for overflow before updating
    if (mCount + update > LeastMaxValue) {
        __enable_irq();
        FL_ASSERT(false, "CountingSemaphoreTeensy: release would exceed max value");
        return;
    }

    mCount += update;

    // Exit critical section: restore interrupts
    __enable_irq();
}

template<ptrdiff_t LeastMaxValue>
void CountingSemaphoreTeensy<LeastMaxValue>::acquire() {
    // Critical section: disable interrupts
    __disable_irq();

    // In single-threaded mode, waiting would deadlock
    // ASSERT if count is 0 to catch programming errors
    if (mCount <= 0) {
        __enable_irq();
        FL_ASSERT(false,
                 "CountingSemaphoreTeensy: acquire() with count=0 would deadlock "
                 "(single-threaded platform). Use try_acquire() instead.");
        return;
    }

    --mCount;

    // Exit critical section: restore interrupts
    __enable_irq();
}

template<ptrdiff_t LeastMaxValue>
bool CountingSemaphoreTeensy<LeastMaxValue>::try_acquire() {
    // Critical section: disable interrupts
    __disable_irq();

    bool success = false;
    if (mCount > 0) {
        --mCount;
        success = true;
    }

    // Exit critical section: restore interrupts
    __enable_irq();

    return success;
}

template<ptrdiff_t LeastMaxValue>
template<class Rep, class Period>
bool CountingSemaphoreTeensy<LeastMaxValue>::try_acquire_for(
    const std::chrono::duration<Rep, Period>& rel_time) {  // okay std namespace

    // On single-threaded platforms, timed acquire is same as try_acquire
    // (no waiting possible - would deadlock)
    (void)rel_time;
    return try_acquire();
}

template<ptrdiff_t LeastMaxValue>
template<class Clock, class Duration>
bool CountingSemaphoreTeensy<LeastMaxValue>::try_acquire_until(
    const std::chrono::time_point<Clock, Duration>& abs_time) {  // okay std namespace

    // On single-threaded platforms, timed acquire is same as try_acquire
    // (no waiting possible - would deadlock)
    (void)abs_time;
    return try_acquire();
}

//=============================================================================
// Explicit template instantiations for common values
//=============================================================================

// Binary semaphore (max value = 1)
template class CountingSemaphoreTeensy<1>;

// Common counting semaphore values
template class CountingSemaphoreTeensy<2>;
template class CountingSemaphoreTeensy<5>;
template class CountingSemaphoreTeensy<10>;
template class CountingSemaphoreTeensy<100>;
template class CountingSemaphoreTeensy<1000>;

// Explicit instantiation of try_acquire_for for common duration types
template bool CountingSemaphoreTeensy<1>::try_acquire_for(const std::chrono::duration<long long, std::milli>&);  // okay std namespace
template bool CountingSemaphoreTeensy<1>::try_acquire_for(const std::chrono::duration<long long, std::micro>&);  // okay std namespace
template bool CountingSemaphoreTeensy<1>::try_acquire_for(const std::chrono::duration<long long, std::nano>&);  // okay std namespace
template bool CountingSemaphoreTeensy<1>::try_acquire_for(const std::chrono::duration<long long, std::ratio<1>>&);  // okay std namespace

// Explicit instantiation of try_acquire_until for common clock types
template bool CountingSemaphoreTeensy<1>::try_acquire_until(const std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace
template bool CountingSemaphoreTeensy<1>::try_acquire_until(const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace

} // namespace platforms
} // namespace fl

#endif // FL_IS_TEENSY
