/// @file mutex_teensy.h
/// @brief Optional mutex support for Teensy platforms with automatic detection
///
/// This file provides mutex implementations for Teensy platforms with three modes:
/// 1. FreeRTOS (Teensy 4.x): Preemptive multitasking with real mutexes
/// 2. TeensyThreads: Cooperative multitasking (yield-based, not preemptive)
/// 3. Interrupt-based fallback: ISR-safe mutex for bare metal (no RTOS)
///
/// Detection Priority:
/// 1. FreeRTOS (if INC_FREERTOS_H is defined) - preemptive, recommended
/// 2. TeensyThreads (if TeensyThreads.h is detected) - cooperative
/// 3. Interrupt-based fallback (bare metal, ISR-safe)
///
/// Usage:
/// To enable real mutex support, include the threading library before FastLED:
///
/// @code
/// // For FreeRTOS on Teensy 4.x:
/// #include <Arduino_FreeRTOS_ARM.h>  // or your FreeRTOS variant
/// #include <FastLED.h>
///
/// // For TeensyThreads:
/// #include <TeensyThreads.h>
/// #include <FastLED.h>
/// @endcode
///
/// Important Notes:
/// - TeensyThreads uses COOPERATIVE scheduling - threads only switch at yield points
/// - FreeRTOS uses PREEMPTIVE scheduling - true concurrent thread safety
/// - Interrupt-based fallback provides ISR protection but NOT thread safety
/// - All Teensy platforms have ARM DMB support for memory barriers (even Cortex-M0+ on LC)

// ok no namespace fl
// allow-include-after-namespace
#pragma once

#include "platforms/arm/teensy/is_teensy.h"
#include "fl/stl/assert.h"

// Only compile this header on Teensy platforms
#if defined(FL_IS_TEENSY)

//=============================================================================
// Detection: Check for FreeRTOS or TeensyThreads availability
//=============================================================================

// FreeRTOS detection (check for FreeRTOS.h inclusion via INC_FREERTOS_H guard)
// Priority: FreeRTOS first (preemptive > cooperative)
#if __has_include(<FreeRTOS.h>)
    #ifndef INC_FREERTOS_H
        #include <FreeRTOS.h>
    #endif
    #define FASTLED_TEENSY_HAS_FREERTOS 1
#elif __has_include(<Arduino_FreeRTOS_ARM.h>)
    #ifndef INC_FREERTOS_H
        #include <Arduino_FreeRTOS_ARM.h>
    #endif
    #define FASTLED_TEENSY_HAS_FREERTOS 1
#else
    #define FASTLED_TEENSY_HAS_FREERTOS 0
#endif

// TeensyThreads detection (only enable if FreeRTOS is not available)
#if __has_include(<TeensyThreads.h>) && !FASTLED_TEENSY_HAS_FREERTOS
    #ifndef TEENSY_THREADS_H
        #include <TeensyThreads.h>
    #endif
    #define FASTLED_TEENSY_HAS_THREADS 1
#else
    #define FASTLED_TEENSY_HAS_THREADS 0
#endif

// Overall threading support detection
#if FASTLED_TEENSY_HAS_FREERTOS || FASTLED_TEENSY_HAS_THREADS
    #define FASTLED_TEENSY_REAL_MUTEX 1
#else
    #define FASTLED_TEENSY_REAL_MUTEX 0
#endif

namespace fl {

//=============================================================================
// FreeRTOS Mutex Implementation (Preemptive)
//=============================================================================

#if FASTLED_TEENSY_HAS_FREERTOS

/// @brief Real mutex implementation using FreeRTOS semaphores (preemptive)
/// @note This provides true concurrent thread safety with preemptive scheduling
class MutexTeensyFreeRTOS {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer to avoid FreeRTOS header dependency)

public:
    MutexTeensyFreeRTOS();
    ~MutexTeensyFreeRTOS();

    // Non-copyable and non-movable
    MutexTeensyFreeRTOS(const MutexTeensyFreeRTOS&) = delete;
    MutexTeensyFreeRTOS& operator=(const MutexTeensyFreeRTOS&) = delete;
    MutexTeensyFreeRTOS(MutexTeensyFreeRTOS&&) = delete;
    MutexTeensyFreeRTOS& operator=(MutexTeensyFreeRTOS&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

/// @brief Real recursive mutex implementation using FreeRTOS recursive semaphores (preemptive)
/// @note Allows the same thread to lock the mutex multiple times
class RecursiveMutexTeensyFreeRTOS {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer)

public:
    RecursiveMutexTeensyFreeRTOS();
    ~RecursiveMutexTeensyFreeRTOS();

    // Non-copyable and non-movable
    RecursiveMutexTeensyFreeRTOS(const RecursiveMutexTeensyFreeRTOS&) = delete;
    RecursiveMutexTeensyFreeRTOS& operator=(const RecursiveMutexTeensyFreeRTOS&) = delete;
    RecursiveMutexTeensyFreeRTOS(RecursiveMutexTeensyFreeRTOS&&) = delete;
    RecursiveMutexTeensyFreeRTOS& operator=(RecursiveMutexTeensyFreeRTOS&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// Alias to the real implementations
using MutexTeensy = MutexTeensyFreeRTOS;
using RecursiveMutexTeensy = RecursiveMutexTeensyFreeRTOS;

//=============================================================================
// TeensyThreads Mutex Implementation (Cooperative)
//=============================================================================

#elif FASTLED_TEENSY_HAS_THREADS

/// @brief Real mutex implementation using TeensyThreads (cooperative)
/// @warning TeensyThreads uses COOPERATIVE scheduling - threads only switch at
///          yield points. This is NOT preemptive multitasking.
class MutexTeensyThreads {
private:
    Threads::Mutex mMutex;  // TeensyThreads mutex object

public:
    MutexTeensyThreads() = default;
    ~MutexTeensyThreads() = default;

    // Non-copyable and non-movable
    MutexTeensyThreads(const MutexTeensyThreads&) = delete;
    MutexTeensyThreads& operator=(const MutexTeensyThreads&) = delete;
    MutexTeensyThreads(MutexTeensyThreads&&) = delete;
    MutexTeensyThreads& operator=(MutexTeensyThreads&&) = delete;

    void lock() {
        // Blocks the current thread until lock is acquired (yields to other threads)
        int result = mMutex.lock();
        FL_ASSERT(result == 0, "TeensyThreads mutex lock failed");
        (void)result;  // Suppress unused warning in release builds
    }

    void unlock() {
        int result = mMutex.unlock();
        FL_ASSERT(result == 0, "TeensyThreads mutex unlock failed");
        (void)result;  // Suppress unused warning
    }

    bool try_lock() {
        // Returns immediately: true if acquired, false if already locked
        return mMutex.try_lock() == 0;
    }
};

/// @brief Recursive mutex implementation using TeensyThreads (cooperative)
/// @warning TeensyThreads is cooperative - see MutexTeensyThreads warning above
class RecursiveMutexTeensyThreads {
private:
    Threads::Mutex mMutex;      // Underlying mutex
    int mLockCount;             // Number of times the owner has locked
    int mOwnerThreadId;         // Thread ID of the current owner (-1 if unlocked)

public:
    RecursiveMutexTeensyThreads() : mLockCount(0), mOwnerThreadId(-1) {}
    ~RecursiveMutexTeensyThreads() {
        FL_ASSERT(mLockCount == 0, "RecursiveMutexTeensyThreads destroyed while locked");
    }

    // Non-copyable and non-movable
    RecursiveMutexTeensyThreads(const RecursiveMutexTeensyThreads&) = delete;
    RecursiveMutexTeensyThreads& operator=(const RecursiveMutexTeensyThreads&) = delete;
    RecursiveMutexTeensyThreads(RecursiveMutexTeensyThreads&&) = delete;
    RecursiveMutexTeensyThreads& operator=(RecursiveMutexTeensyThreads&&) = delete;

    void lock() {
        int currentThreadId = Threads::id();

        // Fast path: already own the lock
        if (mOwnerThreadId == currentThreadId) {
            mLockCount++;
            return;
        }

        // Slow path: acquire the underlying mutex
        mMutex.lock();
        mOwnerThreadId = currentThreadId;
        mLockCount = 1;
    }

    void unlock() {
        FL_ASSERT(mOwnerThreadId == Threads::id(),
                  "RecursiveMutexTeensyThreads unlock called by non-owner thread");
        FL_ASSERT(mLockCount > 0,
                  "RecursiveMutexTeensyThreads unlock called when not locked");

        mLockCount--;
        if (mLockCount == 0) {
            mOwnerThreadId = -1;
            mMutex.unlock();
        }
    }

    bool try_lock() {
        int currentThreadId = Threads::id();

        // Fast path: already own the lock
        if (mOwnerThreadId == currentThreadId) {
            mLockCount++;
            return true;
        }

        // Slow path: try to acquire the underlying mutex
        if (mMutex.try_lock() == 0) {
            mOwnerThreadId = currentThreadId;
            mLockCount = 1;
            return true;
        }
        return false;
    }
};

// Alias to the real implementations
using MutexTeensy = MutexTeensyThreads;
using RecursiveMutexTeensy = RecursiveMutexTeensyThreads;

//=============================================================================
// Interrupt-Based Mutex Implementation (Bare Metal Fallback)
//=============================================================================

#else

/// @brief Teensy interrupt-based mutex for bare metal (ISR-safe, NOT thread-safe)
///
/// CRITICAL LIMITATIONS:
/// - lock() on locked mutex will ASSERT (would deadlock)
/// - Use try_lock() for safe non-blocking operation
/// - Protects against ISR preemption using CMSIS __disable_irq()/__enable_irq()
/// - No actual blocking - this is ISR protection, not thread synchronization
class MutexTeensyInterrupt {
private:
    bool mLocked;

public:
    MutexTeensyInterrupt();
    ~MutexTeensyInterrupt() = default;

    // Non-copyable and non-movable
    MutexTeensyInterrupt(const MutexTeensyInterrupt&) = delete;
    MutexTeensyInterrupt& operator=(const MutexTeensyInterrupt&) = delete;
    MutexTeensyInterrupt(MutexTeensyInterrupt&&) = delete;
    MutexTeensyInterrupt& operator=(MutexTeensyInterrupt&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

/// @brief Teensy interrupt-based recursive mutex for bare metal
///
/// Allows the same "thread" (actually same execution context) to lock multiple times.
/// Uses interrupt disable/restore for critical sections.
class RecursiveMutexTeensyInterrupt {
private:
    uint32_t mLockDepth;

public:
    RecursiveMutexTeensyInterrupt();
    ~RecursiveMutexTeensyInterrupt() = default;

    // Non-copyable and non-movable
    RecursiveMutexTeensyInterrupt(const RecursiveMutexTeensyInterrupt&) = delete;
    RecursiveMutexTeensyInterrupt& operator=(const RecursiveMutexTeensyInterrupt&) = delete;
    RecursiveMutexTeensyInterrupt(RecursiveMutexTeensyInterrupt&&) = delete;
    RecursiveMutexTeensyInterrupt& operator=(RecursiveMutexTeensyInterrupt&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// Alias to interrupt-based implementations
using MutexTeensy = MutexTeensyInterrupt;
using RecursiveMutexTeensy = RecursiveMutexTeensyInterrupt;

#endif  // FASTLED_TEENSY_HAS_FREERTOS / FASTLED_TEENSY_HAS_THREADS

} // namespace fl

#endif  // FL_IS_TEENSY
