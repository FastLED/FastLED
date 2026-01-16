/// @file semaphore_rp.cpp
/// @brief RP2040/RP2350 Pico SDK semaphore platform implementation

// Include platform detection BEFORE the guard
#include "is_rp.h"

#ifdef FL_IS_RP2040

#include "semaphore_rp.h"
#include "fl/warn.h"

// Include Pico SDK headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "hardware/sync.h"
#include "pico/time.h"
FL_EXTERN_C_END

#include <chrono>  // ok include - for std::chrono

namespace fl {
namespace platforms {

//=============================================================================
// CountingSemaphoreRP Implementation
//=============================================================================

template<ptrdiff_t LeastMaxValue>
CountingSemaphoreRP<LeastMaxValue>::CountingSemaphoreRP(ptrdiff_t desired)
    : mSpinlock(nullptr), mCount(desired), mMaxValue(LeastMaxValue) {
    FL_ASSERT(desired >= 0 && desired <= LeastMaxValue,
             "CountingSemaphoreRP: initial count out of range");

    // Claim a hardware spinlock from the pool
    // spin_lock_claim_unused() returns the spinlock instance number
    int spinlock_num = spin_lock_claim_unused(true);  // true = required (panic if none available)

    if (spinlock_num < 0) {
        FL_WARN("CountingSemaphoreRP: Failed to claim hardware spinlock");
        return;
    }

    // Get the actual spinlock instance and store as opaque pointer
    spin_lock_t* spinlock = spin_lock_instance(spinlock_num);
    // spin_lock_t* may be volatile, so we need reinterpret_cast (fl::bit_cast cannot handle volatile)
    mSpinlock = reinterpret_cast<void*>(const_cast<uint32_t*>(reinterpret_cast<volatile uint32_t*>(spinlock))); // ok reinterpret cast
}

template<ptrdiff_t LeastMaxValue>
CountingSemaphoreRP<LeastMaxValue>::~CountingSemaphoreRP() {
    if (mSpinlock != nullptr) {
        spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

        // Get the spinlock number and unclaim it
        uint spinlock_num = spin_lock_get_num(spinlock);
        spin_lock_unclaim(spinlock_num);

        mSpinlock = nullptr;
    }
}

template<ptrdiff_t LeastMaxValue>
void CountingSemaphoreRP<LeastMaxValue>::release(ptrdiff_t update) {
    FL_ASSERT(update >= 0, "CountingSemaphoreRP: release update must be non-negative");
    FL_ASSERT(mSpinlock != nullptr, "CountingSemaphoreRP::release() called on null semaphore");

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

    // Acquire spinlock to protect count manipulation
    uint32_t save = spin_lock_blocking(spinlock);

    // Check if we would exceed max value
    if (mCount + update > mMaxValue) {
        spin_unlock(spinlock, save);
        FL_ASSERT(false, "CountingSemaphoreRP: release would exceed max value");
        return;
    }

    mCount += update;

    // Release spinlock
    spin_unlock(spinlock, save);
}

template<ptrdiff_t LeastMaxValue>
void CountingSemaphoreRP<LeastMaxValue>::acquire() {
    FL_ASSERT(mSpinlock != nullptr, "CountingSemaphoreRP::acquire() called on null semaphore");

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

    // Spin until we can decrement the count
    while (true) {
        uint32_t save = spin_lock_blocking(spinlock);

        if (mCount > 0) {
            mCount--;
            spin_unlock(spinlock, save);
            return;
        }

        spin_unlock(spinlock, save);

        // Yield to avoid hogging CPU (tight_loop_contents is a hint to the processor)
        tight_loop_contents();
    }
}

template<ptrdiff_t LeastMaxValue>
bool CountingSemaphoreRP<LeastMaxValue>::try_acquire() {
    if (mSpinlock == nullptr) {
        return false;
    }

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

    uint32_t save = spin_lock_blocking(spinlock);

    bool acquired = false;
    if (mCount > 0) {
        mCount--;
        acquired = true;
    }

    spin_unlock(spinlock, save);

    return acquired;
}

template<ptrdiff_t LeastMaxValue>
template<class Rep, class Period>
bool CountingSemaphoreRP<LeastMaxValue>::try_acquire_for(
    const std::chrono::duration<Rep, Period>& rel_time) {  // okay std namespace

    if (mSpinlock == nullptr) {
        return false;
    }

    // Convert duration to microseconds
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(rel_time);  // okay std namespace
    absolute_time_t timeout = make_timeout_time_us(us.count());

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

    // Spin until we can decrement the count or timeout
    while (true) {
        uint32_t save = spin_lock_blocking(spinlock);

        if (mCount > 0) {
            mCount--;
            spin_unlock(spinlock, save);
            return true;
        }

        spin_unlock(spinlock, save);

        // Check if we've timed out
        if (time_reached(timeout)) {
            return false;
        }

        // Yield to avoid hogging CPU
        tight_loop_contents();
    }
}

template<ptrdiff_t LeastMaxValue>
template<class Clock, class Duration>
bool CountingSemaphoreRP<LeastMaxValue>::try_acquire_until(
    const std::chrono::time_point<Clock, Duration>& abs_time) {  // okay std namespace

    // Convert absolute time to relative duration
    auto now = Clock::now();
    if (abs_time <= now) {
        // Already past the deadline, try immediate acquire
        return try_acquire();
    }

    auto rel_time = abs_time - now;
    return try_acquire_for(rel_time);
}

//=============================================================================
// Explicit template instantiations for common values
//=============================================================================

// Binary semaphore (max value = 1)
template class CountingSemaphoreRP<1>;

// Common counting semaphore values
template class CountingSemaphoreRP<2>;
template class CountingSemaphoreRP<5>;
template class CountingSemaphoreRP<10>;
template class CountingSemaphoreRP<100>;
template class CountingSemaphoreRP<1000>;

// Explicit instantiation of try_acquire_for for common duration types
template bool CountingSemaphoreRP<1>::try_acquire_for(const std::chrono::duration<long long, std::milli>&);  // okay std namespace
template bool CountingSemaphoreRP<1>::try_acquire_for(const std::chrono::duration<long long, std::micro>&);  // okay std namespace
template bool CountingSemaphoreRP<1>::try_acquire_for(const std::chrono::duration<long long, std::nano>&);  // okay std namespace
template bool CountingSemaphoreRP<1>::try_acquire_for(const std::chrono::duration<long long, std::ratio<1>>&);  // okay std namespace

// Explicit instantiation of try_acquire_until for common clock types
template bool CountingSemaphoreRP<1>::try_acquire_until(const std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace
template bool CountingSemaphoreRP<1>::try_acquire_until(const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace

} // namespace platforms
} // namespace fl

#endif // FL_IS_RP2040
