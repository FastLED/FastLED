/// @file semaphore_esp32.cpp
/// @brief ESP32 FreeRTOS semaphore platform implementation

#ifdef ESP32

#include "semaphore_esp32.h"
#include "fl/warn.h"

// Include FreeRTOS headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
FL_EXTERN_C_END

#include <chrono>  // ok include - for std::chrono

namespace fl {
namespace platforms {

//=============================================================================
// CountingSemaphoreESP32 Implementation
//=============================================================================

template<ptrdiff_t LeastMaxValue>
CountingSemaphoreESP32<LeastMaxValue>::CountingSemaphoreESP32(ptrdiff_t desired)
    : mHandle(nullptr) {
    FL_ASSERT(desired >= 0 && desired <= LeastMaxValue,
             "CountingSemaphoreESP32: initial count out of range");

    // Create a FreeRTOS counting semaphore
    SemaphoreHandle_t handle = xSemaphoreCreateCounting(
        static_cast<UBaseType_t>(LeastMaxValue),
        static_cast<UBaseType_t>(desired)
    );

    if (handle == nullptr) {
        FL_WARN("CountingSemaphoreESP32: Failed to create counting semaphore");
    }

    mHandle = static_cast<void*>(handle);
}

template<ptrdiff_t LeastMaxValue>
CountingSemaphoreESP32<LeastMaxValue>::~CountingSemaphoreESP32() {
    if (mHandle != nullptr) {
        SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);
        vSemaphoreDelete(handle);
        mHandle = nullptr;
    }
}

template<ptrdiff_t LeastMaxValue>
void CountingSemaphoreESP32<LeastMaxValue>::release(ptrdiff_t update) {
    FL_ASSERT(update >= 0, "CountingSemaphoreESP32: release update must be non-negative");
    FL_ASSERT(mHandle != nullptr, "CountingSemaphoreESP32::release() called on null semaphore");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Release the semaphore 'update' times
    for (ptrdiff_t i = 0; i < update; ++i) {
        BaseType_t result = xSemaphoreGive(handle);

        // If we fail to give, it means we would exceed the max value
        if (result != pdTRUE) {
            FL_ASSERT(false, "CountingSemaphoreESP32: release would exceed max value");
            break;
        }
    }
}

template<ptrdiff_t LeastMaxValue>
void CountingSemaphoreESP32<LeastMaxValue>::acquire() {
    FL_ASSERT(mHandle != nullptr, "CountingSemaphoreESP32::acquire() called on null semaphore");

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Block indefinitely waiting for the semaphore
    BaseType_t result = xSemaphoreTake(handle, portMAX_DELAY);

    FL_ASSERT(result == pdTRUE, "CountingSemaphoreESP32::acquire() failed");
}

template<ptrdiff_t LeastMaxValue>
bool CountingSemaphoreESP32<LeastMaxValue>::try_acquire() {
    if (mHandle == nullptr) {
        return false;
    }

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);

    // Try to acquire semaphore without blocking (timeout = 0)
    BaseType_t result = xSemaphoreTake(handle, 0);

    return (result == pdTRUE);
}

template<ptrdiff_t LeastMaxValue>
template<class Rep, class Period>
bool CountingSemaphoreESP32<LeastMaxValue>::try_acquire_for(
    const std::chrono::duration<Rep, Period>& rel_time) {  // okay std namespace

    if (mHandle == nullptr) {
        return false;
    }

    // Convert duration to FreeRTOS ticks
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(rel_time);  // okay std namespace
    TickType_t ticks = pdMS_TO_TICKS(ms.count());

    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(mHandle);
    BaseType_t result = xSemaphoreTake(handle, ticks);

    return (result == pdTRUE);
}

template<ptrdiff_t LeastMaxValue>
template<class Clock, class Duration>
bool CountingSemaphoreESP32<LeastMaxValue>::try_acquire_until(
    const std::chrono::time_point<Clock, Duration>& abs_time) {  // okay std namespace

    // Convert absolute time to relative duration
    auto now = Clock::now();
    if (abs_time <= now) {
        // Already past the deadline, try immediate acquire
        return try_acquire();
    }

    auto rel_time = abs_time - now;
    return try_acquire_for(rel_time);
}

//=============================================================================
// Explicit template instantiations for common values
//=============================================================================

// Binary semaphore (max value = 1)
template class CountingSemaphoreESP32<1>;

// Common counting semaphore values
template class CountingSemaphoreESP32<2>;
template class CountingSemaphoreESP32<5>;
template class CountingSemaphoreESP32<10>;
template class CountingSemaphoreESP32<100>;
template class CountingSemaphoreESP32<1000>;

// Explicit instantiation of try_acquire_for for common duration types
template bool CountingSemaphoreESP32<1>::try_acquire_for(const std::chrono::duration<long long, std::milli>&);  // okay std namespace
template bool CountingSemaphoreESP32<1>::try_acquire_for(const std::chrono::duration<long long, std::micro>&);  // okay std namespace
template bool CountingSemaphoreESP32<1>::try_acquire_for(const std::chrono::duration<long long, std::nano>&);  // okay std namespace
template bool CountingSemaphoreESP32<1>::try_acquire_for(const std::chrono::duration<long long, std::ratio<1>>&);  // okay std namespace

// Explicit instantiation of try_acquire_until for common clock types
template bool CountingSemaphoreESP32<1>::try_acquire_until(const std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace
template bool CountingSemaphoreESP32<1>::try_acquire_until(const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace

} // namespace platforms
} // namespace fl

#endif // ESP32
