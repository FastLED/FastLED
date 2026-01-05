/// @file mutex_teensy.cpp
/// @brief Teensy interrupt-based mutex platform implementation

#ifdef FL_IS_TEENSY

#include "mutex_teensy.h"
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
// MutexTeensy Implementation
//=============================================================================

MutexTeensy::MutexTeensy() : mLocked(false) {
    // Constructor: mutex starts unlocked
}

void MutexTeensy::lock() {
    // Critical section: disable interrupts
    __disable_irq();

    // In single-threaded mode, attempting to lock an already locked mutex
    // would deadlock. ASSERT to catch programming errors.
    if (mLocked) {
        __enable_irq();
        FL_ASSERT(false,
                 "MutexTeensy: lock() on already locked mutex would deadlock "
                 "(single-threaded platform). Use try_lock() instead.");
        return;
    }

    mLocked = true;

    // Exit critical section: restore interrupts
    __enable_irq();
}

void MutexTeensy::unlock() {
    // Critical section: disable interrupts
    __disable_irq();

    // Unlock should only be called if we hold the lock
    FL_ASSERT(mLocked, "MutexTeensy: unlock() called on unlocked mutex");

    mLocked = false;

    // Exit critical section: restore interrupts
    __enable_irq();
}

bool MutexTeensy::try_lock() {
    // Critical section: disable interrupts
    __disable_irq();

    bool success = false;
    if (!mLocked) {
        mLocked = true;
        success = true;
    }

    // Exit critical section: restore interrupts
    __enable_irq();

    return success;
}

//=============================================================================
// RecursiveMutexTeensy Implementation
//=============================================================================

RecursiveMutexTeensy::RecursiveMutexTeensy() : mLockDepth(0) {
    // Constructor: recursive mutex starts unlocked
}

void RecursiveMutexTeensy::lock() {
    // Critical section: disable interrupts
    __disable_irq();

    // For recursive mutex on single-threaded platform, we can always increment depth
    // (there's only one execution context, so if we're here, we're the owner)
    // However, we still want to catch true deadlock scenarios

    // On a single-threaded platform, if depth > 0, we already hold the lock
    // and can safely increment. This allows recursive locking from the same context.
    ++mLockDepth;

    // Exit critical section: restore interrupts
    __enable_irq();
}

void RecursiveMutexTeensy::unlock() {
    // Critical section: disable interrupts
    __disable_irq();

    // Unlock should only be called if we hold the lock
    FL_ASSERT(mLockDepth > 0, "RecursiveMutexTeensy: unlock() called on unlocked mutex");

    --mLockDepth;

    // Exit critical section: restore interrupts
    __enable_irq();
}

bool RecursiveMutexTeensy::try_lock() {
    // Critical section: disable interrupts
    __disable_irq();

    // For recursive mutex on single-threaded platform, we can always lock
    // (there's only one execution context, so we're always the "owner")
    ++mLockDepth;

    // Exit critical section: restore interrupts
    __enable_irq();

    return true;  // Always succeeds for recursive mutex
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_TEENSY
