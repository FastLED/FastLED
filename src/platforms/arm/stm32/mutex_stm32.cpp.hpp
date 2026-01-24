/// @file mutex_stm32.cpp
/// @brief STM32 FreeRTOS mutex platform implementation

#if defined(FL_IS_STM32)

// Only compile if FreeRTOS is available
#if __has_include("FreeRTOS.h")

#include "mutex_stm32.h"
#include "fl/warn.h"

// Include FreeRTOS headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "FreeRTOS.h"
#include "semphr.h"
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// MutexSTM32 Implementation
//=============================================================================

MutexSTM32::MutexSTM32() : mHandle(nullptr) {
    // Create a FreeRTOS mutex (implemented as a binary semaphore)
    SemaphoreHandle_t handle = xSemaphoreCreateMutex();

    if (handle == nullptr) {
        FL_WARN("MutexSTM32: Failed to create mutex");
    }

    mHandle = static_cast<void*>(handle);
}

MutexSTM32::~MutexSTM32() {
    if (mHandle != nullptr) {
        SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);
        vSemaphoreDelete(handle);
        mHandle = nullptr;
    }
}

void MutexSTM32::lock() {
    FL_ASSERT(mHandle != nullptr, "MutexSTM32::lock() called on null mutex");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Block indefinitely waiting for the mutex
    BaseType_t result = xSemaphoreTake(handle, portMAX_DELAY);

    FL_ASSERT(result == pdTRUE, "MutexSTM32::lock() failed to acquire mutex");
}

void MutexSTM32::unlock() {
    FL_ASSERT(mHandle != nullptr, "MutexSTM32::unlock() called on null mutex");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    BaseType_t result = xSemaphoreGive(handle);

    FL_ASSERT(result == pdTRUE, "MutexSTM32::unlock() failed to release mutex");
}

bool MutexSTM32::try_lock() {
    if (mHandle == nullptr) {
        return false;
    }

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Try to acquire mutex without blocking (timeout = 0)
    BaseType_t result = xSemaphoreTake(handle, 0);

    return (result == pdTRUE);
}

//=============================================================================
// RecursiveMutexSTM32 Implementation
//=============================================================================

RecursiveMutexSTM32::RecursiveMutexSTM32() : mHandle(nullptr) {
    // Create a FreeRTOS recursive mutex
    SemaphoreHandle_t handle = xSemaphoreCreateRecursiveMutex();

    if (handle == nullptr) {
        FL_WARN("RecursiveMutexSTM32: Failed to create recursive mutex");
    }

    mHandle = static_cast<void*>(handle);
}

RecursiveMutexSTM32::~RecursiveMutexSTM32() {
    if (mHandle != nullptr) {
        SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);
        vSemaphoreDelete(handle);
        mHandle = nullptr;
    }
}

void RecursiveMutexSTM32::lock() {
    FL_ASSERT(mHandle != nullptr, "RecursiveMutexSTM32::lock() called on null mutex");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Block indefinitely waiting for the recursive mutex
    BaseType_t result = xSemaphoreTakeRecursive(handle, portMAX_DELAY);

    FL_ASSERT(result == pdTRUE, "RecursiveMutexSTM32::lock() failed to acquire mutex");
}

void RecursiveMutexSTM32::unlock() {
    FL_ASSERT(mHandle != nullptr, "RecursiveMutexSTM32::unlock() called on null mutex");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    BaseType_t result = xSemaphoreGiveRecursive(handle);

    FL_ASSERT(result == pdTRUE, "RecursiveMutexSTM32::unlock() failed to release mutex");
}

bool RecursiveMutexSTM32::try_lock() {
    if (mHandle == nullptr) {
        return false;
    }

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Try to acquire recursive mutex without blocking (timeout = 0)
    BaseType_t result = xSemaphoreTakeRecursive(handle, 0);

    return (result == pdTRUE);
}

} // namespace platforms
} // namespace fl

#endif // __has_include("FreeRTOS.h")
#endif // FL_IS_STM32
