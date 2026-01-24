/// @file semaphore_samd.cpp
/// @brief SAMD21/SAMD51 interrupt-based semaphore platform implementation

#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)

#include "semaphore_samd.h"
#include "fl/warn.h"

#include <chrono>  // ok include - for std::chrono

// Include CMSIS for interrupt control
// CMSIS provides __disable_irq() and __enable_irq()
FL_EXTERN_C_BEGIN
#include "sam.h"
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// CountingSemaphoreSAMD Implementation
//=============================================================================

template<ptrdiff_t LeastMaxValue>
CountingSemaphoreSAMD<LeastMaxValue>::CountingSemaphoreSAMD(ptrdiff_t desired)
    : mCounter(desired) {
    FL_ASSERT(desired >= 0 && desired <= LeastMaxValue,
             "CountingSemaphoreSAMD: initial count out of range");
}

template<ptrdiff_t LeastMaxValue>
void CountingSemaphoreSAMD<LeastMaxValue>::release(ptrdiff_t update) {
    FL_ASSERT(update >= 0, "CountingSemaphoreSAMD: release update must be non-negative");

    // Critical section: disable interrupts for atomic operation
    __disable_irq();

    // Check if release would exceed max value
    if (mCounter + update > LeastMaxValue) {
        __enable_irq();
        FL_ASSERT(false, "CountingSemaphoreSAMD: release would exceed max value");
        return;
    }

    // Increment counter
    mCounter += update;

    __enable_irq();
}

template<ptrdiff_t LeastMaxValue>
void CountingSemaphoreSAMD<LeastMaxValue>::acquire() {
    // Critical section: disable interrupts for atomic read-modify-write
    __disable_irq();

    // Check if counter is zero
    if (mCounter == 0) {
        __enable_irq();
        FL_WARN("CountingSemaphoreSAMD::acquire() called when counter is 0 (would deadlock on threaded system)");
        return;
    }

    // Decrement counter
    mCounter--;

    __enable_irq();
}

template<ptrdiff_t LeastMaxValue>
bool CountingSemaphoreSAMD<LeastMaxValue>::try_acquire() {
    // Critical section: disable interrupts for atomic read-modify-write
    __disable_irq();

    // Check if counter is zero
    if (mCounter == 0) {
        __enable_irq();
        return false;
    }

    // Decrement counter
    mCounter--;

    __enable_irq();
    return true;
}

template<ptrdiff_t LeastMaxValue>
template<class Rep, class Period>
bool CountingSemaphoreSAMD<LeastMaxValue>::try_acquire_for(
    const std::chrono::duration<Rep, Period>& rel_time) {  // okay std namespace

    // On bare metal single-threaded platform, we cannot block
    // Just attempt immediate acquisition
    (void)rel_time;  // Unused - no blocking possible
    return try_acquire();
}

template<ptrdiff_t LeastMaxValue>
template<class Clock, class Duration>
bool CountingSemaphoreSAMD<LeastMaxValue>::try_acquire_until(
    const std::chrono::time_point<Clock, Duration>& abs_time) {  // okay std namespace

    // On bare metal single-threaded platform, we cannot block
    // Just attempt immediate acquisition
    (void)abs_time;  // Unused - no blocking possible
    return try_acquire();
}

//=============================================================================
// Explicit template instantiations for common values
//=============================================================================

// Binary semaphore (max value = 1)
template class CountingSemaphoreSAMD<1>;

// Common counting semaphore values
template class CountingSemaphoreSAMD<2>;
template class CountingSemaphoreSAMD<5>;
template class CountingSemaphoreSAMD<10>;
template class CountingSemaphoreSAMD<100>;
template class CountingSemaphoreSAMD<1000>;

// Explicit instantiation of try_acquire_for for common duration types
template bool CountingSemaphoreSAMD<1>::try_acquire_for(const std::chrono::duration<long long, std::milli>&);  // okay std namespace
template bool CountingSemaphoreSAMD<1>::try_acquire_for(const std::chrono::duration<long long, std::micro>&);  // okay std namespace
template bool CountingSemaphoreSAMD<1>::try_acquire_for(const std::chrono::duration<long long, std::nano>&);  // okay std namespace
template bool CountingSemaphoreSAMD<1>::try_acquire_for(const std::chrono::duration<long long, std::ratio<1>>&);  // okay std namespace

// Explicit instantiation of try_acquire_until for common clock types
template bool CountingSemaphoreSAMD<1>::try_acquire_until(const std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace
template bool CountingSemaphoreSAMD<1>::try_acquire_until(const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace

} // namespace platforms
} // namespace fl

#endif // FL_IS_SAMD21 || FL_IS_SAMD51
