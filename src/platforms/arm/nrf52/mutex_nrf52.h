/*
  FastLED — nRF52 Mutex Implementation
  -------------------------------------
  Real mutex support for nRF52 using FreeRTOS primitives.

  Thread Safety:
  - Uses FreeRTOS semaphores (SemaphoreHandle_t) for real mutexes
  - Provides both MutexNRF52 (non-recursive) and RecursiveMutexNRF52 (recursive)
  - SoftDevice compatible (BLE stack safe)

  SoftDevice Compatibility:
  - FreeRTOS is the standard RTOS on Adafruit nRF52 boards
  - The SoftDevice (Nordic's BLE stack) runs alongside FreeRTOS
  - FreeRTOS semaphores are safe to use with SoftDevice active
  - Mutexes use priority inheritance to avoid priority inversion

  Alternative: Nordic SDK Critical Regions
  - For applications NOT using FreeRTOS, Nordic provides CRITICAL_REGION macros
  - CRITICAL_REGION_ENTER/EXIT use ARM CPSID/CPSIE (global interrupt disable)
  - This implementation assumes FreeRTOS is available (Adafruit BSP standard)

  Usage:
  - Only enabled when FreeRTOS headers are detected
  - Non-copyable, non-movable semantics (like std::mutex)
  - Asserts if mutex creation fails (critical error)

  License: MIT (FastLED)
*/

#pragma once

#if defined(FL_IS_NRF52) || defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(NRF52833)

// Detect FreeRTOS availability by checking for FreeRTOS.h inclusion
// The Adafruit nRF52 BSP includes FreeRTOS by default
#if __has_include("FreeRTOS.h")
#define FASTLED_NRF52_HAS_FREERTOS 1
#endif

#ifdef FASTLED_NRF52_HAS_FREERTOS

#include "fl/stl/assert.h"
#include "fl/compiler_control.h"

// Include FreeRTOS headers
FL_EXTERN_C_BEGIN
#include "FreeRTOS.h"
#include "semphr.h"
FL_EXTERN_C_END

namespace fl {

// =============================================================================
// MutexNRF52 — Non-Recursive Mutex using FreeRTOS
// =============================================================================

class MutexNRF52 {
private:
    SemaphoreHandle_t mHandle;

public:
    MutexNRF52() {
        // Create a mutex semaphore (binary semaphore with priority inheritance)
        mHandle = xSemaphoreCreateMutex();
        FL_ASSERT(mHandle != nullptr, "MutexNRF52: failed to create mutex (out of heap memory?)");
    }

    ~MutexNRF52() {
        if (mHandle != nullptr) {
            vSemaphoreDelete(mHandle);
            mHandle = nullptr;
        }
    }

    // Non-copyable and non-movable
    MutexNRF52(const MutexNRF52&) = delete;
    MutexNRF52& operator=(const MutexNRF52&) = delete;
    MutexNRF52(MutexNRF52&&) = delete;
    MutexNRF52& operator=(MutexNRF52&&) = delete;

    // Lock the mutex (blocks until available)
    void lock() {
        FL_ASSERT(mHandle != nullptr, "MutexNRF52: lock called on destroyed mutex");
        BaseType_t result = xSemaphoreTake(mHandle, portMAX_DELAY);
        FL_ASSERT(result == pdTRUE, "MutexNRF52: lock failed unexpectedly");
        (void)result;  // Suppress unused warning in release builds
    }

    // Unlock the mutex
    void unlock() {
        FL_ASSERT(mHandle != nullptr, "MutexNRF52: unlock called on destroyed mutex");
        BaseType_t result = xSemaphoreGive(mHandle);
        FL_ASSERT(result == pdTRUE, "MutexNRF52: unlock failed (mutex not owned?)");
        (void)result;  // Suppress unused warning in release builds
    }

    // Try to lock the mutex (non-blocking)
    // Returns: true if lock acquired, false if already locked
    bool try_lock() {
        FL_ASSERT(mHandle != nullptr, "MutexNRF52: try_lock called on destroyed mutex");
        BaseType_t result = xSemaphoreTake(mHandle, 0);
        return (result == pdTRUE);
    }
};

// =============================================================================
// RecursiveMutexNRF52 — Recursive Mutex using FreeRTOS
// =============================================================================

class RecursiveMutexNRF52 {
private:
    SemaphoreHandle_t mHandle;

public:
    RecursiveMutexNRF52() {
        // Create a recursive mutex semaphore
        mHandle = xSemaphoreCreateRecursiveMutex();
        FL_ASSERT(mHandle != nullptr, "RecursiveMutexNRF52: failed to create mutex (out of heap memory?)");
    }

    ~RecursiveMutexNRF52() {
        if (mHandle != nullptr) {
            vSemaphoreDelete(mHandle);
            mHandle = nullptr;
        }
    }

    // Non-copyable and non-movable
    RecursiveMutexNRF52(const RecursiveMutexNRF52&) = delete;
    RecursiveMutexNRF52& operator=(const RecursiveMutexNRF52&) = delete;
    RecursiveMutexNRF52(RecursiveMutexNRF52&&) = delete;
    RecursiveMutexNRF52& operator=(RecursiveMutexNRF52&&) = delete;

    // Lock the mutex (blocks until available, allows recursive locking)
    void lock() {
        FL_ASSERT(mHandle != nullptr, "RecursiveMutexNRF52: lock called on destroyed mutex");
        BaseType_t result = xSemaphoreTakeRecursive(mHandle, portMAX_DELAY);
        FL_ASSERT(result == pdTRUE, "RecursiveMutexNRF52: lock failed unexpectedly");
        (void)result;  // Suppress unused warning in release builds
    }

    // Unlock the mutex (must match number of lock calls)
    void unlock() {
        FL_ASSERT(mHandle != nullptr, "RecursiveMutexNRF52: unlock called on destroyed mutex");
        BaseType_t result = xSemaphoreGiveRecursive(mHandle);
        FL_ASSERT(result == pdTRUE, "RecursiveMutexNRF52: unlock failed (mutex not owned?)");
        (void)result;  // Suppress unused warning in release builds
    }

    // Try to lock the mutex (non-blocking, allows recursive locking)
    // Returns: true if lock acquired, false if already locked by another task
    bool try_lock() {
        FL_ASSERT(mHandle != nullptr, "RecursiveMutexNRF52: try_lock called on destroyed mutex");
        BaseType_t result = xSemaphoreTakeRecursive(mHandle, 0);
        return (result == pdTRUE);
    }
};

} // namespace fl

#endif // FASTLED_NRF52_HAS_FREERTOS

#endif // FL_IS_NRF52 || NRF52 || NRF52832 || NRF52840 || NRF52833
