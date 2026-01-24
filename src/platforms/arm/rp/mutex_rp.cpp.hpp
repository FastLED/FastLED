/// @file mutex_rp.cpp
/// @brief RP2040/RP2350 Pico SDK mutex platform implementation

// Include platform detection BEFORE the guard
#include "is_rp.h"

#ifdef FL_IS_RP2040

#include "mutex_rp.h"
#include "fl/warn.h"

// Include Pico SDK headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "hardware/sync.h"
#include "pico/platform.h"
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// MutexRP Implementation
//=============================================================================

MutexRP::MutexRP() : mSpinlock(nullptr), mOwnerCore(0xFFFFFFFF), mLocked(false) {
    // Claim a hardware spinlock from the pool
    int spinlock_num = spin_lock_claim_unused(true);  // true = required (panic if none available)

    if (spinlock_num < 0) {
        FL_WARN("MutexRP: Failed to claim hardware spinlock");
        return;
    }

    // Get the actual spinlock instance and store as opaque pointer
    spin_lock_t* spinlock = spin_lock_instance(spinlock_num);
    // spin_lock_t* may be volatile, so we need reinterpret_cast
    mSpinlock = reinterpret_cast<void*>(const_cast<uint32_t*>(reinterpret_cast<volatile uint32_t*>(spinlock))); // ok reinterpret cast
}

MutexRP::~MutexRP() {
    if (mSpinlock != nullptr) {
        spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

        // Get the spinlock number and unclaim it
        uint spinlock_num = spin_lock_get_num(spinlock);
        spin_lock_unclaim(spinlock_num);

        mSpinlock = nullptr;
    }
}

void MutexRP::lock() {
    FL_ASSERT(mSpinlock != nullptr, "MutexRP::lock() called on null mutex");

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

    // Block until we acquire the spinlock
    uint32_t save = spin_lock_blocking(spinlock);

    // Mark as locked and record owner
    mLocked = true;
    mOwnerCore = get_core_num();

    // Keep the spinlock locked - it will be released in unlock()
    // Note: We store the interrupt state to restore it later
    // For now, we just use the spinlock directly as a mutex
    (void)save;  // Suppress unused variable warning
}

void MutexRP::unlock() {
    FL_ASSERT(mSpinlock != nullptr, "MutexRP::unlock() called on null mutex");
    FL_ASSERT(mLocked, "MutexRP::unlock() called on unlocked mutex");
    FL_ASSERT(mOwnerCore == get_core_num(), "MutexRP::unlock() called from different core than lock()");

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

    // Mark as unlocked
    mLocked = false;
    mOwnerCore = 0xFFFFFFFF;

    // Release the spinlock with interrupt state restoration (0 means enable interrupts)
    spin_unlock(spinlock, 0);
}

bool MutexRP::try_lock() {
    if (mSpinlock == nullptr) {
        return false;
    }

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

    // Try to acquire the spinlock without blocking
    // Use spin_try_lock_unsafe which returns true if lock acquired
    bool acquired = spin_try_lock_unsafe(spinlock);

    if (acquired) {
        mLocked = true;
        mOwnerCore = get_core_num();
    }

    return acquired;
}

//=============================================================================
// RecursiveMutexRP Implementation
//=============================================================================

RecursiveMutexRP::RecursiveMutexRP() : mSpinlock(nullptr), mOwnerCore(0xFFFFFFFF), mLockCount(0) {
    // Claim a hardware spinlock from the pool
    int spinlock_num = spin_lock_claim_unused(true);  // true = required (panic if none available)

    if (spinlock_num < 0) {
        FL_WARN("RecursiveMutexRP: Failed to claim hardware spinlock");
        return;
    }

    // Get the actual spinlock instance and store as opaque pointer
    spin_lock_t* spinlock = spin_lock_instance(spinlock_num);
    // spin_lock_t* may be volatile, so we need reinterpret_cast
    mSpinlock = reinterpret_cast<void*>(const_cast<uint32_t*>(reinterpret_cast<volatile uint32_t*>(spinlock))); // ok reinterpret cast
}

RecursiveMutexRP::~RecursiveMutexRP() {
    if (mSpinlock != nullptr) {
        spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

        // Get the spinlock number and unclaim it
        uint spinlock_num = spin_lock_get_num(spinlock);
        spin_lock_unclaim(spinlock_num);

        mSpinlock = nullptr;
    }
}

void RecursiveMutexRP::lock() {
    FL_ASSERT(mSpinlock != nullptr, "RecursiveMutexRP::lock() called on null mutex");

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);
    uint32_t current_core = get_core_num();

    // Check if we already own the lock (recursive case)
    if (mLockCount > 0 && mOwnerCore == current_core) {
        mLockCount++;
        return;
    }

    // Block until we acquire the spinlock
    uint32_t save = spin_lock_blocking(spinlock);

    // Mark as locked and record owner
    mLockCount = 1;
    mOwnerCore = current_core;

    (void)save;  // Suppress unused variable warning
}

void RecursiveMutexRP::unlock() {
    FL_ASSERT(mSpinlock != nullptr, "RecursiveMutexRP::unlock() called on null mutex");
    FL_ASSERT(mLockCount > 0, "RecursiveMutexRP::unlock() called on unlocked mutex");
    FL_ASSERT(mOwnerCore == get_core_num(), "RecursiveMutexRP::unlock() called from different core than lock()");

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);

    // Decrement lock count
    mLockCount--;

    if (mLockCount == 0) {
        // No more recursive locks, release the spinlock
        mOwnerCore = 0xFFFFFFFF;
        spin_unlock(spinlock, 0);
    }
}

bool RecursiveMutexRP::try_lock() {
    if (mSpinlock == nullptr) {
        return false;
    }

    spin_lock_t* spinlock = static_cast<spin_lock_t*>(mSpinlock);
    uint32_t current_core = get_core_num();

    // Check if we already own the lock (recursive case)
    if (mLockCount > 0 && mOwnerCore == current_core) {
        mLockCount++;
        return true;
    }

    // Try to acquire the spinlock without blocking
    // Use spin_try_lock_unsafe which returns true if lock acquired
    bool acquired = spin_try_lock_unsafe(spinlock);

    if (acquired) {
        mLockCount = 1;
        mOwnerCore = current_core;
    }

    return acquired;
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_RP2040
