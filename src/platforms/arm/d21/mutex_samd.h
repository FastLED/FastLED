// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/d21/mutex_samd.h
/// @brief SAMD21/SAMD51 interrupt-based mutex implementation
///
/// This header provides SAMD-specific mutex implementations using CMSIS
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
/// - Lock flag with critical section protection
/// - No blocking - single-threaded environment
/// - Warnings on lock failures (already locked)

#include "fl/stl/assert.h"
#include "fl/warn.h"
#include <mutex>  // ok include - needed for std::unique_lock compatibility

namespace fl {
namespace platforms {

// Forward declarations
class MutexSAMD;
class RecursiveMutexSAMD;

// Platform implementation aliases for SAMD
using mutex = MutexSAMD;
using recursive_mutex = RecursiveMutexSAMD;

// Use std::unique_lock for compatibility (works with our mutex interface)
template<typename Mutex>
using unique_lock = std::unique_lock<Mutex>;  // okay std namespace

// Lock constructor tag types (re-export from std)
using std::defer_lock_t;  // okay std namespace
using std::try_to_lock_t;  // okay std namespace
using std::adopt_lock_t;  // okay std namespace
using std::defer_lock;  // okay std namespace
using std::try_to_lock;  // okay std namespace
using std::adopt_lock;  // okay std namespace

/// @brief SAMD interrupt-based mutex
///
/// This class provides a mutex implementation using CMSIS interrupt control
/// for atomic operations. Compatible with std::mutex interface, but adapted
/// for bare metal single-threaded environment.
///
/// Since SAMD platforms are single-threaded, blocking would deadlock.
/// Instead, lock() warns if the mutex is already locked and returns immediately.
class MutexSAMD {
private:
    volatile bool mLocked;  // Lock flag (volatile for ISR safety)

public:
    MutexSAMD();
    ~MutexSAMD() = default;

    // Non-copyable and non-movable
    MutexSAMD(const MutexSAMD&) = delete;
    MutexSAMD& operator=(const MutexSAMD&) = delete;
    MutexSAMD(MutexSAMD&&) = delete;
    MutexSAMD& operator=(MutexSAMD&&) = delete;

    /// @brief Lock the mutex (warns if already locked)
    void lock();

    /// @brief Unlock the mutex
    void unlock();

    /// @brief Try to lock the mutex without blocking
    /// @return true if lock acquired, false if already locked
    bool try_lock();
};

/// @brief SAMD interrupt-based recursive mutex
///
/// This class provides a recursive mutex implementation using CMSIS interrupt
/// control. Allows the same "thread" (in single-threaded context, this means
/// tracking lock count) to acquire the lock multiple times.
///
/// In bare metal single-threaded environment, we track the lock count and
/// allow multiple lock() calls from the same context.
class RecursiveMutexSAMD {
private:
    volatile uint32_t mLockCount;  // Number of times locked (0 = unlocked)

public:
    RecursiveMutexSAMD();
    ~RecursiveMutexSAMD() = default;

    // Non-copyable and non-movable
    RecursiveMutexSAMD(const RecursiveMutexSAMD&) = delete;
    RecursiveMutexSAMD& operator=(const RecursiveMutexSAMD&) = delete;
    RecursiveMutexSAMD(RecursiveMutexSAMD&&) = delete;
    RecursiveMutexSAMD& operator=(RecursiveMutexSAMD&&) = delete;

    /// @brief Lock the mutex (increments lock count)
    void lock();

    /// @brief Unlock the mutex (decrements lock count)
    void unlock();

    /// @brief Try to lock the mutex without blocking
    /// @return Always true in single-threaded environment
    bool try_lock();
};

// Define FASTLED_MULTITHREADED=0 for SAMD (bare metal, no threading)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

} // namespace platforms
} // namespace fl
