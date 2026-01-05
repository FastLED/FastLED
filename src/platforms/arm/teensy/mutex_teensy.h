// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/teensy/mutex_teensy.h
/// @brief Teensy interrupt-based mutex implementation
///
/// This header provides Teensy-specific mutex implementations using interrupt
/// disable/restore for critical sections. Since Teensy platforms are single-core
/// bare metal (no threading), the mutex is ISR-safe but NOT thread-safe.
///
/// IMPORTANT: These are ISR-protection primitives, NOT threading primitives.
/// - lock() will ASSERT/PANIC if mutex is already locked (would deadlock)
/// - Use try_lock() for ISR-safe non-blocking locking
/// - Protects against ISR preemption using CMSIS __disable_irq()/__enable_irq()
///
/// Supported platforms:
/// - Teensy LC (ARM Cortex-M0+, 48 MHz)
/// - Teensy 3.x (ARM Cortex-M4/M4F, 48-180 MHz)
/// - Teensy 4.x (ARM Cortex-M7, 600 MHz)

#include "fl/stl/assert.h"

namespace fl {
namespace platforms {

// Forward declarations
class MutexTeensy;
class RecursiveMutexTeensy;

// Platform implementation aliases for Teensy
using mutex = MutexTeensy;
using recursive_mutex = RecursiveMutexTeensy;

// Lock constructor tag types (define at platform namespace level)
struct defer_lock_t { explicit defer_lock_t() = default; };
struct try_to_lock_t { explicit try_to_lock_t() = default; };
struct adopt_lock_t { explicit adopt_lock_t() = default; };

inline constexpr defer_lock_t defer_lock{};
inline constexpr try_to_lock_t try_to_lock{};
inline constexpr adopt_lock_t adopt_lock{};

// Simple unique_lock implementation for Teensy
template<typename Mutex>
class unique_lock {
private:
    Mutex* mMutex;
    bool mOwnsLock;

public:
    // Default constructor
    unique_lock() noexcept : mMutex(nullptr), mOwnsLock(false) {}

    // Lock on construction
    explicit unique_lock(Mutex& m) : mMutex(&m), mOwnsLock(false) {
        lock();
    }

    // Defer locking
    unique_lock(Mutex& m, defer_lock_t) noexcept : mMutex(&m), mOwnsLock(false) {}

    // Try to lock
    unique_lock(Mutex& m, try_to_lock_t) : mMutex(&m), mOwnsLock(false) {
        try_lock();
    }

    // Adopt existing lock
    unique_lock(Mutex& m, adopt_lock_t) noexcept : mMutex(&m), mOwnsLock(true) {}

    // Destructor
    ~unique_lock() {
        if (mOwnsLock) {
            unlock();
        }
    }

    // Non-copyable but movable
    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

    unique_lock(unique_lock&& other) noexcept
        : mMutex(other.mMutex), mOwnsLock(other.mOwnsLock) {
        other.mMutex = nullptr;
        other.mOwnsLock = false;
    }

    unique_lock& operator=(unique_lock&& other) noexcept {
        if (mOwnsLock) {
            unlock();
        }
        mMutex = other.mMutex;
        mOwnsLock = other.mOwnsLock;
        other.mMutex = nullptr;
        other.mOwnsLock = false;
        return *this;
    }

    void lock() {
        FL_ASSERT(mMutex != nullptr, "unique_lock::lock() called with null mutex");
        FL_ASSERT(!mOwnsLock, "unique_lock::lock() called when already owning lock");
        mMutex->lock();
        mOwnsLock = true;
    }

    bool try_lock() {
        FL_ASSERT(mMutex != nullptr, "unique_lock::try_lock() called with null mutex");
        FL_ASSERT(!mOwnsLock, "unique_lock::try_lock() called when already owning lock");
        mOwnsLock = mMutex->try_lock();
        return mOwnsLock;
    }

    void unlock() {
        FL_ASSERT(mOwnsLock, "unique_lock::unlock() called when not owning lock");
        FL_ASSERT(mMutex != nullptr, "unique_lock::unlock() called with null mutex");
        mMutex->unlock();
        mOwnsLock = false;
    }

    bool owns_lock() const noexcept {
        return mOwnsLock;
    }

    explicit operator bool() const noexcept {
        return mOwnsLock;
    }

    Mutex* mutex() const noexcept {
        return mMutex;
    }

    Mutex* release() noexcept {
        Mutex* result = mMutex;
        mMutex = nullptr;
        mOwnsLock = false;
        return result;
    }
};

/// @brief Teensy interrupt-based mutex
///
/// This class provides a mutex implementation using interrupt disable/restore
/// for critical sections. Compatible with standard mutex interface, but optimized
/// for single-core bare metal.
///
/// CRITICAL LIMITATIONS:
/// - lock() on locked mutex will ASSERT (would deadlock)
/// - Use try_lock() for safe non-blocking operation
/// - No actual blocking - this is ISR protection, not thread synchronization
class MutexTeensy {
private:
    bool mLocked;

public:
    MutexTeensy();
    ~MutexTeensy() = default;

    // Non-copyable and non-movable
    MutexTeensy(const MutexTeensy&) = delete;
    MutexTeensy& operator=(const MutexTeensy&) = delete;
    MutexTeensy(MutexTeensy&&) = delete;
    MutexTeensy& operator=(MutexTeensy&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

/// @brief Teensy interrupt-based recursive mutex
///
/// This class provides a recursive mutex implementation using interrupt disable/restore
/// for critical sections. Allows the same "thread" (actually same execution context)
/// to lock the mutex multiple times.
///
/// CRITICAL LIMITATIONS:
/// - lock() on locked mutex (by different context) will ASSERT (would deadlock)
/// - Use try_lock() for safe non-blocking operation
/// - Recursion is tracked by depth counter, not thread ID (single-threaded)
class RecursiveMutexTeensy {
private:
    uint32_t mLockDepth;

public:
    RecursiveMutexTeensy();
    ~RecursiveMutexTeensy() = default;

    // Non-copyable and non-movable
    RecursiveMutexTeensy(const RecursiveMutexTeensy&) = delete;
    RecursiveMutexTeensy& operator=(const RecursiveMutexTeensy&) = delete;
    RecursiveMutexTeensy(RecursiveMutexTeensy&&) = delete;
    RecursiveMutexTeensy& operator=(RecursiveMutexTeensy&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// Define FASTLED_MULTITHREADED for Teensy (single-threaded, but ISR-safe)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

} // namespace platforms
} // namespace fl
