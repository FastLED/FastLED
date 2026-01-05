/// @file mutex_teensy.cpp
/// @brief Teensy mutex platform implementation (FreeRTOS, TeensyThreads, or interrupt-based fallback)

#if defined(FL_IS_TEENSY)

#include "mutex_teensy.h"
#include "fl/warn.h"

//=============================================================================
// FreeRTOS Mutex Implementation
//=============================================================================

#if FASTLED_TEENSY_HAS_FREERTOS

// Include FreeRTOS headers ONLY in .cpp file (following FastLED pattern from task_coroutine_esp32.cpp)
// This avoids polluting the global namespace in headers
// Note: FreeRTOS.h was already included by mutex_teensy.h for detection
extern "C" {
#include "semphr.h"
}

namespace fl {

MutexTeensyFreeRTOS::MutexTeensyFreeRTOS() : mHandle(nullptr) {
    // Create a FreeRTOS binary semaphore (mutex)
    SemaphoreHandle_t handle = xSemaphoreCreateMutex();
    FL_ASSERT(handle != nullptr, "FreeRTOS mutex creation failed");
    mHandle = static_cast<void*>(handle);
}

MutexTeensyFreeRTOS::~MutexTeensyFreeRTOS() {
    if (mHandle != nullptr) {
        SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);
        vSemaphoreDelete(handle);
        mHandle = nullptr;
    }
}

void MutexTeensyFreeRTOS::lock() {
    FL_ASSERT(mHandle != nullptr, "MutexTeensyFreeRTOS::lock called on null handle");
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Block indefinitely until mutex is acquired
    BaseType_t result = xSemaphoreTake(handle, portMAX_DELAY);
    FL_ASSERT(result == pdTRUE, "FreeRTOS mutex lock failed");
    (void)result;  // Suppress unused warning in release builds
}

void MutexTeensyFreeRTOS::unlock() {
    FL_ASSERT(mHandle != nullptr, "MutexTeensyFreeRTOS::unlock called on null handle");
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    BaseType_t result = xSemaphoreGive(handle);
    FL_ASSERT(result == pdTRUE, "FreeRTOS mutex unlock failed");
    (void)result;  // Suppress unused warning
}

bool MutexTeensyFreeRTOS::try_lock() {
    FL_ASSERT(mHandle != nullptr, "MutexTeensyFreeRTOS::try_lock called on null handle");
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Try to acquire without blocking (timeout = 0)
    BaseType_t result = xSemaphoreTake(handle, 0);
    return result == pdTRUE;
}

//=============================================================================
// RecursiveMutexTeensyFreeRTOS Implementation
//=============================================================================

RecursiveMutexTeensyFreeRTOS::RecursiveMutexTeensyFreeRTOS() : mHandle(nullptr) {
    // Create a FreeRTOS recursive mutex
    SemaphoreHandle_t handle = xSemaphoreCreateRecursiveMutex();
    FL_ASSERT(handle != nullptr, "FreeRTOS recursive mutex creation failed");
    mHandle = static_cast<void*>(handle);
}

RecursiveMutexTeensyFreeRTOS::~RecursiveMutexTeensyFreeRTOS() {
    if (mHandle != nullptr) {
        SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);
        vSemaphoreDelete(handle);
        mHandle = nullptr;
    }
}

void RecursiveMutexTeensyFreeRTOS::lock() {
    FL_ASSERT(mHandle != nullptr, "RecursiveMutexTeensyFreeRTOS::lock called on null handle");
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Block indefinitely until mutex is acquired
    BaseType_t result = xSemaphoreTakeRecursive(handle, portMAX_DELAY);
    FL_ASSERT(result == pdTRUE, "FreeRTOS recursive mutex lock failed");
    (void)result;  // Suppress unused warning
}

void RecursiveMutexTeensyFreeRTOS::unlock() {
    FL_ASSERT(mHandle != nullptr, "RecursiveMutexTeensyFreeRTOS::unlock called on null handle");
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    BaseType_t result = xSemaphoreGiveRecursive(handle);
    FL_ASSERT(result == pdTRUE, "FreeRTOS recursive mutex unlock failed");
    (void)result;  // Suppress unused warning
}

bool RecursiveMutexTeensyFreeRTOS::try_lock() {
    FL_ASSERT(mHandle != nullptr, "RecursiveMutexTeensyFreeRTOS::try_lock called on null handle");
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Try to acquire without blocking (timeout = 0)
    BaseType_t result = xSemaphoreTakeRecursive(handle, 0);
    return result == pdTRUE;
}

} // namespace fl

#endif  // FASTLED_TEENSY_HAS_FREERTOS

//=============================================================================
// Interrupt-Based Mutex Implementation (Fallback for bare metal)
//=============================================================================

#if !FASTLED_TEENSY_HAS_FREERTOS && !FASTLED_TEENSY_HAS_THREADS

// ARM Cortex CMSIS intrinsics for interrupt control
// __disable_irq() / __enable_irq() are available on all Teensy platforms
// (Cortex-M0+, M4, M4F, M7)
extern "C" {
    void __disable_irq(void);
    void __enable_irq(void);
}

namespace fl {

//=============================================================================
// MutexTeensyInterrupt Implementation
//=============================================================================

MutexTeensyInterrupt::MutexTeensyInterrupt() : mLocked(false) {
    // Constructor: mutex starts unlocked
}

void MutexTeensyInterrupt::lock() {
    // Critical section: disable interrupts
    __disable_irq();

    // In single-threaded mode, attempting to lock an already locked mutex
    // would deadlock. ASSERT to catch programming errors.
    if (mLocked) {
        __enable_irq();
        FL_ASSERT(false,
                 "MutexTeensyInterrupt: lock() on already locked mutex would deadlock "
                 "(single-threaded platform). Use try_lock() instead.");
        return;
    }

    mLocked = true;

    // Exit critical section: restore interrupts
    __enable_irq();
}

void MutexTeensyInterrupt::unlock() {
    // Critical section: disable interrupts
    __disable_irq();

    // Unlock should only be called if we hold the lock
    FL_ASSERT(mLocked, "MutexTeensyInterrupt: unlock() called on unlocked mutex");

    mLocked = false;

    // Exit critical section: restore interrupts
    __enable_irq();
}

bool MutexTeensyInterrupt::try_lock() {
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
// RecursiveMutexTeensyInterrupt Implementation
//=============================================================================

RecursiveMutexTeensyInterrupt::RecursiveMutexTeensyInterrupt() : mLockDepth(0) {
    // Constructor: recursive mutex starts unlocked
}

void RecursiveMutexTeensyInterrupt::lock() {
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

void RecursiveMutexTeensyInterrupt::unlock() {
    // Critical section: disable interrupts
    __disable_irq();

    // Unlock should only be called if we hold the lock
    FL_ASSERT(mLockDepth > 0, "RecursiveMutexTeensyInterrupt: unlock() called on unlocked mutex");

    --mLockDepth;

    // Exit critical section: restore interrupts
    __enable_irq();
}

bool RecursiveMutexTeensyInterrupt::try_lock() {
    // Critical section: disable interrupts
    __disable_irq();

    // For recursive mutex on single-threaded platform, we can always lock
    // (there's only one execution context, so we're always the "owner")
    ++mLockDepth;

    // Exit critical section: restore interrupts
    __enable_irq();

    return true;  // Always succeeds for recursive mutex
}

} // namespace fl

#endif // !FASTLED_TEENSY_HAS_FREERTOS && !FASTLED_TEENSY_HAS_THREADS

#endif // FL_IS_TEENSY
