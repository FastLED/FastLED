/// @file mutex_esp32.cpp
/// @brief ESP32 FreeRTOS mutex platform implementation

#ifdef ESP32

#include "mutex_esp32.h"
#include "fl/warn.h"

// Include FreeRTOS headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// MutexESP32 Implementation
//=============================================================================

MutexESP32::MutexESP32() : mHandle(nullptr) {
    // Create a FreeRTOS mutex (implemented as a binary semaphore)
    SemaphoreHandle_t handle = xSemaphoreCreateMutex();

    if (handle == nullptr) {
        FL_WARN("MutexESP32: Failed to create mutex");
    }

    mHandle = static_cast<void*>(handle);
}

MutexESP32::~MutexESP32() {
    if (mHandle != nullptr) {
        SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);
        vSemaphoreDelete(handle);
        mHandle = nullptr;
    }
}

void MutexESP32::lock() {
    FL_ASSERT(mHandle != nullptr, "MutexESP32::lock() called on null mutex");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Block indefinitely waiting for the mutex
    BaseType_t result = xSemaphoreTake(handle, portMAX_DELAY);

    FL_ASSERT(result == pdTRUE, "MutexESP32::lock() failed to acquire mutex");
}

void MutexESP32::unlock() {
    FL_ASSERT(mHandle != nullptr, "MutexESP32::unlock() called on null mutex");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    BaseType_t result = xSemaphoreGive(handle);

    FL_ASSERT(result == pdTRUE, "MutexESP32::unlock() failed to release mutex");
}

bool MutexESP32::try_lock() {
    if (mHandle == nullptr) {
        return false;
    }

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Try to acquire mutex without blocking (timeout = 0)
    BaseType_t result = xSemaphoreTake(handle, 0);

    return (result == pdTRUE);
}

//=============================================================================
// RecursiveMutexESP32 Implementation
//=============================================================================

RecursiveMutexESP32::RecursiveMutexESP32() : mHandle(nullptr) {
    // Create a FreeRTOS recursive mutex
    SemaphoreHandle_t handle = xSemaphoreCreateRecursiveMutex();

    if (handle == nullptr) {
        FL_WARN("RecursiveMutexESP32: Failed to create recursive mutex");
    }

    mHandle = static_cast<void*>(handle);
}

RecursiveMutexESP32::~RecursiveMutexESP32() {
    if (mHandle != nullptr) {
        SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);
        vSemaphoreDelete(handle);
        mHandle = nullptr;
    }
}

void RecursiveMutexESP32::lock() {
    FL_ASSERT(mHandle != nullptr, "RecursiveMutexESP32::lock() called on null mutex");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Block indefinitely waiting for the recursive mutex
    BaseType_t result = xSemaphoreTakeRecursive(handle, portMAX_DELAY);

    FL_ASSERT(result == pdTRUE, "RecursiveMutexESP32::lock() failed to acquire mutex");
}

void RecursiveMutexESP32::unlock() {
    FL_ASSERT(mHandle != nullptr, "RecursiveMutexESP32::unlock() called on null mutex");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    BaseType_t result = xSemaphoreGiveRecursive(handle);

    FL_ASSERT(result == pdTRUE, "RecursiveMutexESP32::unlock() failed to release mutex");
}

bool RecursiveMutexESP32::try_lock() {
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

#endif // ESP32
