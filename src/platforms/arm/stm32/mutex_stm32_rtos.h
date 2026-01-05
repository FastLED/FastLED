// ok no namespace fl
#pragma once

/// @file mutex_stm32_rtos.h
/// STM32 CMSIS-RTOS mutex wrapper for FastLED
///
/// This header provides real mutex support for STM32 platforms when CMSIS-RTOS
/// (v1 or v2) is available. It automatically detects which RTOS version is present
/// and provides a unified interface compatible with FastLED's mutex API.
///
/// Detection order:
/// 1. CMSIS-RTOS v2 (cmsis_os2.h) - Preferred, modern API with osMutexNew
/// 2. CMSIS-RTOS v1 (cmsis_os.h) - Legacy API with osMutexCreate
/// 3. Fake mutex fallback - Single-threaded debugging mode
///
/// Usage:
/// - Include this header from STM32 platform code when real mutex support is needed
/// - Mutex creation happens in constructor (no separate init required)
/// - Provides both mutex (non-recursive) and recursive_mutex implementations
/// - Compatible with fl::unique_lock<> for RAII-style locking
///
/// Requirements:
/// - STM32 project with CMSIS-RTOS support enabled
/// - FreeRTOS or compatible RTOS configured via STM32CubeMX
/// - ARM Cortex-M3/M4/M7 or newer (for DMB memory barrier support)

#include "fl/has_include.h"
#include "fl/stl/assert.h"

// ============================================================================
// CMSIS-RTOS Version Detection
// ============================================================================

// Try CMSIS-RTOS v2 first (preferred)
#if FL_HAS_INCLUDE(<cmsis_os2.h>)
    #define FASTLED_STM32_HAS_CMSIS_RTOS_V2 1
    #include <cmsis_os2.h>
#elif FL_HAS_INCLUDE(<cmsis_os.h>)
    #define FASTLED_STM32_HAS_CMSIS_RTOS_V1 1
    #include <cmsis_os.h>
#else
    // No CMSIS-RTOS available - will use fake mutex
    #define FASTLED_STM32_HAS_CMSIS_RTOS_NONE 1
#endif

namespace fl {

// ============================================================================
// CMSIS-RTOS v2 Mutex Implementation
// ============================================================================

#if defined(FASTLED_STM32_HAS_CMSIS_RTOS_V2)

/// Real mutex implementation using CMSIS-RTOS v2 API
/// Non-recursive mutex with priority inheritance
class MutexSTM32_v2 {
private:
    osMutexId_t mMutexHandle;
    const char* mName;

public:
    /// Constructor - creates mutex with default attributes (non-recursive, priority inherit)
    explicit MutexSTM32_v2(const char* name = nullptr) : mMutexHandle(nullptr), mName(name) {
        osMutexAttr_t attr = {0};
        attr.name = mName;
        attr.attr_bits = osMutexPrioInherit; // Non-recursive, priority inheritance
        attr.cb_mem = nullptr; // Let RTOS allocate memory
        attr.cb_size = 0;

        mMutexHandle = osMutexNew(&attr);
        FL_ASSERT(mMutexHandle != nullptr, "Failed to create CMSIS-RTOS v2 mutex");
    }

    /// Destructor - deletes mutex
    ~MutexSTM32_v2() {
        if (mMutexHandle != nullptr) {
            osMutexDelete(mMutexHandle);
            mMutexHandle = nullptr;
        }
    }

    // Non-copyable and non-movable
    MutexSTM32_v2(const MutexSTM32_v2&) = delete;
    MutexSTM32_v2& operator=(const MutexSTM32_v2&) = delete;
    MutexSTM32_v2(MutexSTM32_v2&&) = delete;
    MutexSTM32_v2& operator=(MutexSTM32_v2&&) = delete;

    /// Lock the mutex (blocks until acquired)
    void lock() {
        FL_ASSERT(mMutexHandle != nullptr, "Mutex not initialized");
        osStatus_t status = osMutexAcquire(mMutexHandle, osWaitForever);
        FL_ASSERT(status == osOK, "Failed to acquire mutex");
    }

    /// Unlock the mutex
    void unlock() {
        FL_ASSERT(mMutexHandle != nullptr, "Mutex not initialized");
        osStatus_t status = osMutexRelease(mMutexHandle);
        FL_ASSERT(status == osOK, "Failed to release mutex");
    }

    /// Try to lock the mutex without blocking
    /// @return true if lock acquired, false if already locked
    bool try_lock() {
        FL_ASSERT(mMutexHandle != nullptr, "Mutex not initialized");
        osStatus_t status = osMutexAcquire(mMutexHandle, 0); // No timeout
        return (status == osOK);
    }
};

/// Recursive mutex implementation using CMSIS-RTOS v2 API
/// Allows same thread to acquire lock multiple times
class RecursiveMutexSTM32_v2 {
private:
    osMutexId_t mMutexHandle;
    const char* mName;

public:
    /// Constructor - creates recursive mutex with priority inheritance
    explicit RecursiveMutexSTM32_v2(const char* name = nullptr) : mMutexHandle(nullptr), mName(name) {
        osMutexAttr_t attr = {0};
        attr.name = mName;
        attr.attr_bits = osMutexRecursive | osMutexPrioInherit; // Recursive + priority inheritance
        attr.cb_mem = nullptr; // Let RTOS allocate memory
        attr.cb_size = 0;

        mMutexHandle = osMutexNew(&attr);
        FL_ASSERT(mMutexHandle != nullptr, "Failed to create CMSIS-RTOS v2 recursive mutex");
    }

    /// Destructor - deletes mutex
    ~RecursiveMutexSTM32_v2() {
        if (mMutexHandle != nullptr) {
            osMutexDelete(mMutexHandle);
            mMutexHandle = nullptr;
        }
    }

    // Non-copyable and non-movable
    RecursiveMutexSTM32_v2(const RecursiveMutexSTM32_v2&) = delete;
    RecursiveMutexSTM32_v2& operator=(const RecursiveMutexSTM32_v2&) = delete;
    RecursiveMutexSTM32_v2(RecursiveMutexSTM32_v2&&) = delete;
    RecursiveMutexSTM32_v2& operator=(RecursiveMutexSTM32_v2&&) = delete;

    /// Lock the mutex (blocks until acquired, allows recursive locking)
    void lock() {
        FL_ASSERT(mMutexHandle != nullptr, "Recursive mutex not initialized");
        osStatus_t status = osMutexAcquire(mMutexHandle, osWaitForever);
        FL_ASSERT(status == osOK, "Failed to acquire recursive mutex");
    }

    /// Unlock the mutex (must be called once per lock() call)
    void unlock() {
        FL_ASSERT(mMutexHandle != nullptr, "Recursive mutex not initialized");
        osStatus_t status = osMutexRelease(mMutexHandle);
        FL_ASSERT(status == osOK, "Failed to release recursive mutex");
    }

    /// Try to lock the mutex without blocking
    /// @return true if lock acquired, false if already locked by another thread
    bool try_lock() {
        FL_ASSERT(mMutexHandle != nullptr, "Recursive mutex not initialized");
        osStatus_t status = osMutexAcquire(mMutexHandle, 0); // No timeout
        return (status == osOK);
    }
};

// Export as primary mutex types for STM32 with CMSIS-RTOS v2
using MutexSTM32 = MutexSTM32_v2;
using RecursiveMutexSTM32 = RecursiveMutexSTM32_v2;

// ============================================================================
// CMSIS-RTOS v1 Mutex Implementation
// ============================================================================

#elif defined(FASTLED_STM32_HAS_CMSIS_RTOS_V1)

/// Real mutex implementation using CMSIS-RTOS v1 API
/// Non-recursive mutex (v1 API doesn't distinguish recursive vs non-recursive in attributes)
class MutexSTM32_v1 {
private:
    osMutexId mMutexHandle;
    const char* mName;

    // Static mutex definition (required by v1 API)
    // Note: Using anonymous union to avoid static-in-header issues on Teensy
    struct MutexDef {
        uint32_t os_mutex_cb[4]; // Placeholder for control block (implementation-specific)
    };
    MutexDef mMutexDef;

public:
    /// Constructor - creates mutex using CMSIS-RTOS v1 API
    explicit MutexSTM32_v1(const char* name = nullptr)
        : mMutexHandle(nullptr), mName(name), mMutexDef{} {

        // Create mutex definition structure
        osMutexDef_t mutex_def;
        mutex_def.mutex = mMutexDef.os_mutex_cb;

        mMutexHandle = osMutexCreate(&mutex_def);
        FL_ASSERT(mMutexHandle != nullptr, "Failed to create CMSIS-RTOS v1 mutex");
    }

    /// Destructor - deletes mutex
    ~MutexSTM32_v1() {
        if (mMutexHandle != nullptr) {
            osMutexDelete(mMutexHandle);
            mMutexHandle = nullptr;
        }
    }

    // Non-copyable and non-movable
    MutexSTM32_v1(const MutexSTM32_v1&) = delete;
    MutexSTM32_v1& operator=(const MutexSTM32_v1&) = delete;
    MutexSTM32_v1(MutexSTM32_v1&&) = delete;
    MutexSTM32_v1& operator=(MutexSTM32_v1&&) = delete;

    /// Lock the mutex (blocks until acquired)
    void lock() {
        FL_ASSERT(mMutexHandle != nullptr, "Mutex not initialized");
        osStatus status = osMutexWait(mMutexHandle, osWaitForever);
        FL_ASSERT(status == osOK, "Failed to acquire mutex");
    }

    /// Unlock the mutex
    void unlock() {
        FL_ASSERT(mMutexHandle != nullptr, "Mutex not initialized");
        osStatus status = osMutexRelease(mMutexHandle);
        FL_ASSERT(status == osOK, "Failed to release mutex");
    }

    /// Try to lock the mutex without blocking
    /// @return true if lock acquired, false if already locked
    bool try_lock() {
        FL_ASSERT(mMutexHandle != nullptr, "Mutex not initialized");
        osStatus status = osMutexWait(mMutexHandle, 0); // No timeout
        return (status == osOK);
    }
};

/// Recursive mutex implementation using CMSIS-RTOS v1 API
/// Note: v1 API doesn't have explicit recursive mutex support, so we track recursion manually
/// This is a compatibility layer - prefer v2 API for true recursive mutex support
class RecursiveMutexSTM32_v1 {
private:
    osMutexId mMutexHandle;
    const char* mName;
    osThreadId mOwnerThread;
    int mLockCount;

    // Static mutex definition (required by v1 API)
    struct MutexDef {
        uint32_t os_mutex_cb[4]; // Placeholder for control block (implementation-specific)
    };
    MutexDef mMutexDef;

public:
    /// Constructor - creates recursive mutex using CMSIS-RTOS v1 API
    explicit RecursiveMutexSTM32_v1(const char* name = nullptr)
        : mMutexHandle(nullptr), mName(name), mOwnerThread(nullptr), mLockCount(0), mMutexDef{} {

        // Create mutex definition structure
        osMutexDef_t mutex_def;
        mutex_def.mutex = mMutexDef.os_mutex_cb;

        mMutexHandle = osMutexCreate(&mutex_def);
        FL_ASSERT(mMutexHandle != nullptr, "Failed to create CMSIS-RTOS v1 recursive mutex");
    }

    /// Destructor - deletes mutex
    ~RecursiveMutexSTM32_v1() {
        if (mMutexHandle != nullptr) {
            osMutexDelete(mMutexHandle);
            mMutexHandle = nullptr;
        }
    }

    // Non-copyable and non-movable
    RecursiveMutexSTM32_v1(const RecursiveMutexSTM32_v1&) = delete;
    RecursiveMutexSTM32_v1& operator=(const RecursiveMutexSTM32_v1&) = delete;
    RecursiveMutexSTM32_v1(RecursiveMutexSTM32_v1&&) = delete;
    RecursiveMutexSTM32_v1& operator=(RecursiveMutexSTM32_v1&&) = delete;

    /// Lock the mutex (blocks until acquired, allows recursive locking)
    void lock() {
        FL_ASSERT(mMutexHandle != nullptr, "Recursive mutex not initialized");

        osThreadId currentThread = osThreadGetId();

        // Check if current thread already owns the mutex
        if (mOwnerThread == currentThread) {
            // Recursive lock - just increment count
            mLockCount++;
            return;
        }

        // First lock - acquire mutex from RTOS
        osStatus status = osMutexWait(mMutexHandle, osWaitForever);
        FL_ASSERT(status == osOK, "Failed to acquire recursive mutex");

        mOwnerThread = currentThread;
        mLockCount = 1;
    }

    /// Unlock the mutex (must be called once per lock() call)
    void unlock() {
        FL_ASSERT(mMutexHandle != nullptr, "Recursive mutex not initialized");
        FL_ASSERT(mLockCount > 0, "unlock() called without matching lock()");
        FL_ASSERT(mOwnerThread == osThreadGetId(), "unlock() called from non-owner thread");

        mLockCount--;

        // Only release RTOS mutex when all recursive locks are released
        if (mLockCount == 0) {
            mOwnerThread = nullptr;
            osStatus status = osMutexRelease(mMutexHandle);
            FL_ASSERT(status == osOK, "Failed to release recursive mutex");
        }
    }

    /// Try to lock the mutex without blocking
    /// @return true if lock acquired, false if already locked by another thread
    bool try_lock() {
        FL_ASSERT(mMutexHandle != nullptr, "Recursive mutex not initialized");

        osThreadId currentThread = osThreadGetId();

        // Check if current thread already owns the mutex
        if (mOwnerThread == currentThread) {
            // Recursive lock - just increment count
            mLockCount++;
            return true;
        }

        // Try to acquire mutex from RTOS (no blocking)
        osStatus status = osMutexWait(mMutexHandle, 0);
        if (status == osOK) {
            mOwnerThread = currentThread;
            mLockCount = 1;
            return true;
        }

        return false;
    }
};

// Export as primary mutex types for STM32 with CMSIS-RTOS v1
using MutexSTM32 = MutexSTM32_v1;
using RecursiveMutexSTM32 = RecursiveMutexSTM32_v1;

// ============================================================================
// Fake Mutex Fallback (No RTOS)
// ============================================================================

#else // No CMSIS-RTOS available

/// Fake mutex for single-threaded debugging
/// Provides assertion-based validation without actual locking
class MutexSTM32Fake {
private:
    bool mLocked = false;

public:
    explicit MutexSTM32Fake(const char* /* name */ = nullptr) {}

    // Non-copyable and non-movable
    MutexSTM32Fake(const MutexSTM32Fake&) = delete;
    MutexSTM32Fake& operator=(const MutexSTM32Fake&) = delete;
    MutexSTM32Fake(MutexSTM32Fake&&) = delete;
    MutexSTM32Fake& operator=(MutexSTM32Fake&&) = delete;

    void lock() {
        FL_ASSERT(!mLocked, "MutexSTM32Fake: attempting to lock already locked mutex (non-recursive)");
        mLocked = true;
    }

    void unlock() {
        FL_ASSERT(mLocked, "MutexSTM32Fake: unlock called on unlocked mutex");
        mLocked = false;
    }

    bool try_lock() {
        if (mLocked) {
            return false;
        }
        mLocked = true;
        return true;
    }
};

/// Fake recursive mutex for single-threaded debugging
class RecursiveMutexSTM32Fake {
private:
    int mLockCount = 0;

public:
    explicit RecursiveMutexSTM32Fake(const char* /* name */ = nullptr) {}

    // Non-copyable and non-movable
    RecursiveMutexSTM32Fake(const RecursiveMutexSTM32Fake&) = delete;
    RecursiveMutexSTM32Fake& operator=(const RecursiveMutexSTM32Fake&) = delete;
    RecursiveMutexSTM32Fake(RecursiveMutexSTM32Fake&&) = delete;
    RecursiveMutexSTM32Fake& operator=(RecursiveMutexSTM32Fake&&) = delete;

    void lock() {
        mLockCount++;
    }

    void unlock() {
        FL_ASSERT(mLockCount > 0, "RecursiveMutexSTM32Fake: unlock called without matching lock");
        mLockCount--;
    }

    bool try_lock() {
        mLockCount++;
        return true;
    }
};

// Export as primary mutex types for STM32 without RTOS (fake fallback)
using MutexSTM32 = MutexSTM32Fake;
using RecursiveMutexSTM32 = RecursiveMutexSTM32Fake;

#endif // CMSIS-RTOS version detection

} // namespace fl
