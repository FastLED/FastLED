/// @file mutex_samd.cpp
/// @brief SAMD21/SAMD51 interrupt-based mutex platform implementation

#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)

#include "mutex_samd.h"
#include "fl/warn.h"

// Include CMSIS for interrupt control
// CMSIS provides __disable_irq() and __enable_irq()
FL_EXTERN_C_BEGIN
#include "sam.h"
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// MutexSAMD Implementation
//=============================================================================

MutexSAMD::MutexSAMD() : mLocked(false) {
    // Initialize mutex in unlocked state
}

void MutexSAMD::lock() {
    // Critical section: disable interrupts for atomic read-modify-write
    __disable_irq();

    // Check if already locked
    if (mLocked) {
        __enable_irq();
        FL_WARN("MutexSAMD::lock() called when already locked (would deadlock on threaded system)");
        return;
    }

    // Acquire lock
    mLocked = true;

    __enable_irq();
}

void MutexSAMD::unlock() {
    // Critical section: disable interrupts for atomic write
    __disable_irq();

    // Check if already unlocked
    if (!mLocked) {
        __enable_irq();
        FL_WARN("MutexSAMD::unlock() called when not locked");
        return;
    }

    // Release lock
    mLocked = false;

    __enable_irq();
}

bool MutexSAMD::try_lock() {
    // Critical section: disable interrupts for atomic read-modify-write
    __disable_irq();

    // Check if already locked
    if (mLocked) {
        __enable_irq();
        return false;
    }

    // Acquire lock
    mLocked = true;

    __enable_irq();
    return true;
}

//=============================================================================
// RecursiveMutexSAMD Implementation
//=============================================================================

RecursiveMutexSAMD::RecursiveMutexSAMD() : mLockCount(0) {
    // Initialize mutex with zero lock count
}

void RecursiveMutexSAMD::lock() {
    // Critical section: disable interrupts for atomic read-modify-write
    __disable_irq();

    // Increment lock count (recursive locking allowed)
    mLockCount++;

    __enable_irq();
}

void RecursiveMutexSAMD::unlock() {
    // Critical section: disable interrupts for atomic read-modify-write
    __disable_irq();

    // Check if already unlocked
    if (mLockCount == 0) {
        __enable_irq();
        FL_WARN("RecursiveMutexSAMD::unlock() called when not locked");
        return;
    }

    // Decrement lock count
    mLockCount--;

    __enable_irq();
}

bool RecursiveMutexSAMD::try_lock() {
    // Critical section: disable interrupts for atomic read-modify-write
    __disable_irq();

    // Increment lock count (recursive locking always succeeds in single-threaded)
    mLockCount++;

    __enable_irq();
    return true;
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_SAMD21 || FL_IS_SAMD51
