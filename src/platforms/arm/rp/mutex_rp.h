// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/rp/mutex_rp.h
/// @brief RP2040/RP2350 Pico SDK mutex implementation
///
/// This header provides RP2040/RP2350-specific mutex implementations using Pico SDK spinlocks.
/// For RP platforms, we use std::unique_lock for full compatibility with condition variables.

#include "fl/stl/assert.h"
#include <mutex>  // ok include - needed for std::unique_lock

namespace fl {
namespace platforms {

// Forward declarations
class MutexRP;
class RecursiveMutexRP;

// Platform implementation aliases for RP2040/RP2350
using mutex = MutexRP;
using recursive_mutex = RecursiveMutexRP;

// Use std::unique_lock for full compatibility with std::condition_variable
template<typename Mutex>
using unique_lock = std::unique_lock<Mutex>;  // okay std namespace

// Lock constructor tag types (re-export from std)
using std::defer_lock_t;  // okay std namespace
using std::try_to_lock_t;  // okay std namespace
using std::adopt_lock_t;  // okay std namespace
using std::defer_lock;  // okay std namespace
using std::try_to_lock;  // okay std namespace
using std::adopt_lock;  // okay std namespace

// RP2040/RP2350 Pico SDK mutex wrapper
class MutexRP {
private:
    void* mSpinlock;      // spin_lock_t* (opaque pointer to avoid including Pico SDK headers)
    uint32_t mOwnerCore;  // Core ID of the owner (for debug/assert purposes)
    bool mLocked;         // Lock state

public:
    MutexRP();
    ~MutexRP();

    // Non-copyable and non-movable
    MutexRP(const MutexRP&) = delete;
    MutexRP& operator=(const MutexRP&) = delete;
    MutexRP(MutexRP&&) = delete;
    MutexRP& operator=(MutexRP&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// RP2040/RP2350 Pico SDK recursive mutex wrapper
class RecursiveMutexRP {
private:
    void* mSpinlock;      // spin_lock_t* (opaque pointer to avoid including Pico SDK headers)
    uint32_t mOwnerCore;  // Core ID of the owner
    uint32_t mLockCount;  // Recursion depth counter

public:
    RecursiveMutexRP();
    ~RecursiveMutexRP();

    // Non-copyable and non-movable
    RecursiveMutexRP(const RecursiveMutexRP&) = delete;
    RecursiveMutexRP& operator=(const RecursiveMutexRP&) = delete;
    RecursiveMutexRP(RecursiveMutexRP&&) = delete;
    RecursiveMutexRP& operator=(RecursiveMutexRP&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// Define FASTLED_MULTITHREADED for RP2040/RP2350 (has dual-core support)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
