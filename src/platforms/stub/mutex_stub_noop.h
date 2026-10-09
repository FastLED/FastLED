// ok no namespace fl
// allow-include-after-namespace
#pragma once

// IWYU pragma: private

/// @file platforms/stub/mutex_stub_noop.h
/// @brief Stub platform mutex implementation for single-threaded environments
///
/// This header provides fake mutex implementations for single-threaded platforms.
/// The mutex operations are no-ops or assertions, as there's no actual concurrency.

#include "fl/stl/assert.h"
#include "fl/stl/utility.h"  // for fl::swap
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

// Tag types for lock constructors
struct defer_lock_t { explicit defer_lock_t() = default; };
struct try_to_lock_t { explicit try_to_lock_t() = default; };
struct adopt_lock_t { explicit adopt_lock_t() = default; };

constexpr defer_lock_t defer_lock{};
constexpr try_to_lock_t try_to_lock{};
constexpr adopt_lock_t adopt_lock{};

// Fake mutex (non-recursive) for single-threaded mode
class MutexFake {
private:
    bool mLocked = false;

public:
    MutexFake() = default;

    // Non-copyable and non-movable
    MutexFake(const MutexFake&) = delete;
    MutexFake& operator=(const MutexFake&) = delete;
    MutexFake(MutexFake&&) = delete;
    MutexFake& operator=(MutexFake&&) = delete;

    // Non-recursive mutex operations
    void lock() FL_NO_EXCEPT {
        FL_ASSERT(!mLocked, "MutexFake: attempting to lock already locked mutex (non-recursive)");
        mLocked = true;
    }

    void unlock() FL_NO_EXCEPT {
        FL_ASSERT(mLocked, "MutexFake: unlock called on unlocked mutex");
        mLocked = false;
    }

    bool try_lock() FL_NO_EXCEPT {
        if (mLocked) {
            return false;
        }
        mLocked = true;
        return true;
    }
};

// Fake recursive mutex for single-threaded mode
class RecursiveMutexFake {
private:
    int mLockCount = 0;

public:
    RecursiveMutexFake() = default;

    // Non-copyable and non-movable
    RecursiveMutexFake(const RecursiveMutexFake&) = delete;
    RecursiveMutexFake& operator=(const RecursiveMutexFake&) = delete;
    RecursiveMutexFake(RecursiveMutexFake&&) = delete;
    RecursiveMutexFake& operator=(RecursiveMutexFake&&) = delete;

    // Recursive mutex operations
    void lock() FL_NO_EXCEPT {
        // In single-threaded mode, we just track the lock count for debugging
        mLockCount++;
    }

    void unlock() FL_NO_EXCEPT {
        // In single-threaded mode, we just track the lock count for debugging
        FL_ASSERT(mLockCount > 0, "RecursiveMutexFake: unlock called without matching lock");
        mLockCount--;
    }

    bool try_lock() FL_NO_EXCEPT {
        // In single-threaded mode, always succeed and increment count
        mLockCount++;
        return true;
    }
};

// Platform implementation aliases for single-threaded mode
using mutex = MutexFake;
using recursive_mutex = RecursiveMutexFake;

// Define FASTLED_MULTITHREADED for single-threaded platforms
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

} // namespace platforms
} // namespace fl

#include "platforms/shared/nothrow_lock.h"
