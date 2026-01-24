/// @file condition_variable_esp32.cpp
/// @brief ESP32 FreeRTOS condition variable platform implementation

#ifdef ESP32

#include "condition_variable_esp32.h"
#include "mutex_esp32.h"
#include "fl/warn.h"

// Include FreeRTOS headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
FL_EXTERN_C_END

#include <chrono>  // ok include - for std::chrono

namespace fl {
namespace platforms {

//=============================================================================
// Internal Helper Structures
//=============================================================================

// Structure to hold a waiting task
struct WaitingTask {
    TaskHandle_t task;
    bool notified;
};

//=============================================================================
// ConditionVariableESP32 Implementation
//=============================================================================

ConditionVariableESP32::ConditionVariableESP32()
    : mMutex(nullptr), mWaitQueue(nullptr) {

    // Create internal mutex for protecting the wait queue
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr) {
        FL_WARN("ConditionVariableESP32: Failed to create internal mutex");
    }
    mMutex = static_cast<void*>(mutex);

    // Create queue to hold waiting task handles
    // Queue can hold up to 10 waiting tasks (configurable if needed)
    QueueHandle_t queue = xQueueCreate(10, sizeof(WaitingTask));
    if (queue == nullptr) {
        FL_WARN("ConditionVariableESP32: Failed to create wait queue");
    }
    mWaitQueue = static_cast<void*>(queue);
}

ConditionVariableESP32::~ConditionVariableESP32() {
    if (mWaitQueue != nullptr) {
        QueueHandle_t queue = static_cast<QueueHandle_t>(mWaitQueue);
        vQueueDelete(queue);
        mWaitQueue = nullptr;
    }

    if (mMutex != nullptr) {
        SemaphoreHandle_t mutex = static_cast<SemaphoreHandle_t>(mMutex);
        vSemaphoreDelete(mutex);
        mMutex = nullptr;
    }
}

void ConditionVariableESP32::notify_one() noexcept {
    if (mMutex == nullptr || mWaitQueue == nullptr) {
        return;
    }

    SemaphoreHandle_t mutex = static_cast<SemaphoreHandle_t>(mMutex);
    QueueHandle_t queue = static_cast<QueueHandle_t>(mWaitQueue);

    // Lock internal mutex
    xSemaphoreTake(mutex, portMAX_DELAY);

    // Try to dequeue one waiting task
    WaitingTask waiter;
    if (xQueueReceive(queue, &waiter, 0) == pdTRUE) {
        // Mark as notified and wake the task
        waiter.notified = true;
        xTaskNotifyGive(waiter.task);
    }

    // Unlock internal mutex
    xSemaphoreGive(mutex);
}

void ConditionVariableESP32::notify_all() noexcept {
    if (mMutex == nullptr || mWaitQueue == nullptr) {
        return;
    }

    SemaphoreHandle_t mutex = static_cast<SemaphoreHandle_t>(mMutex);
    QueueHandle_t queue = static_cast<QueueHandle_t>(mWaitQueue);

    // Lock internal mutex
    xSemaphoreTake(mutex, portMAX_DELAY);

    // Wake all waiting tasks
    WaitingTask waiter;
    while (xQueueReceive(queue, &waiter, 0) == pdTRUE) {
        waiter.notified = true;
        xTaskNotifyGive(waiter.task);
    }

    // Unlock internal mutex
    xSemaphoreGive(mutex);
}

//=============================================================================
// Template Method Implementations
//=============================================================================

template<typename Mutex>
void ConditionVariableESP32::wait(unique_lock<Mutex>& lock) {
    FL_ASSERT(lock.owns_lock(), "ConditionVariableESP32::wait() called on unlocked lock");

    // Get the mutex from the lock
    Mutex* mtx = lock.mutex();
    FL_ASSERT(mtx != nullptr, "ConditionVariableESP32::wait() called with null mutex");

    SemaphoreHandle_t internal_mutex = static_cast<SemaphoreHandle_t>(mMutex);
    QueueHandle_t queue = static_cast<QueueHandle_t>(mWaitQueue);

    // Get current task handle
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    // Add ourselves to the wait queue
    WaitingTask waiter = { current_task, false };

    // Lock internal mutex
    xSemaphoreTake(internal_mutex, portMAX_DELAY);

    // Add to wait queue
    BaseType_t result = xQueueSend(queue, &waiter, 0);
    if (result != pdTRUE) {
        FL_WARN("ConditionVariableESP32: Wait queue full");
        xSemaphoreGive(internal_mutex);
        return;
    }

    // Unlock internal mutex
    xSemaphoreGive(internal_mutex);

    // Unlock user mutex before waiting
    lock.unlock();

    // Wait for notification (blocking indefinitely)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Re-lock user mutex after waking up
    lock.lock();
}

template<typename Mutex, typename Predicate>
void ConditionVariableESP32::wait(unique_lock<Mutex>& lock, Predicate pred) {
    while (!pred()) {
        wait(lock);
    }
}

template<typename Mutex, typename Rep, typename Period>
cv_status ConditionVariableESP32::wait_for(
    unique_lock<Mutex>& lock,
    const std::chrono::duration<Rep, Period>& rel_time) {  // okay std namespace

    FL_ASSERT(lock.owns_lock(), "ConditionVariableESP32::wait_for() called on unlocked lock");

    // Convert duration to milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(rel_time);  // okay std namespace

    // Get the mutex from the lock
    Mutex* mtx = lock.mutex();
    FL_ASSERT(mtx != nullptr, "ConditionVariableESP32::wait_for() called with null mutex");

    SemaphoreHandle_t internal_mutex = static_cast<SemaphoreHandle_t>(mMutex);
    QueueHandle_t queue = static_cast<QueueHandle_t>(mWaitQueue);

    // Get current task handle
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    // Add ourselves to the wait queue
    WaitingTask waiter = { current_task, false };

    // Lock internal mutex
    xSemaphoreTake(internal_mutex, portMAX_DELAY);

    // Add to wait queue
    BaseType_t result = xQueueSend(queue, &waiter, 0);
    if (result != pdTRUE) {
        FL_WARN("ConditionVariableESP32: Wait queue full");
        xSemaphoreGive(internal_mutex);
        return cv_status::timeout;
    }

    // Unlock internal mutex
    xSemaphoreGive(internal_mutex);

    // Unlock user mutex before waiting
    lock.unlock();

    // Wait for notification with timeout
    TickType_t ticks = pdMS_TO_TICKS(ms.count());
    uint32_t notify_value = ulTaskNotifyTake(pdTRUE, ticks);

    // Re-lock user mutex after waking up
    lock.lock();

    // Check if we were notified or timed out
    return (notify_value == 0) ? cv_status::timeout : cv_status::no_timeout;
}

template<typename Mutex, typename Rep, typename Period, typename Predicate>
bool ConditionVariableESP32::wait_for(
    unique_lock<Mutex>& lock,
    const std::chrono::duration<Rep, Period>& rel_time,  // okay std namespace
    Predicate pred) {

    auto start = std::chrono::steady_clock::now();  // okay std namespace
    auto deadline = start + rel_time;

    while (!pred()) {
        auto now = std::chrono::steady_clock::now();  // okay std namespace
        if (now >= deadline) {
            return pred();  // Final check before returning
        }

        auto remaining = deadline - now;
        if (wait_for(lock, remaining) == cv_status::timeout) {
            return pred();  // Final check after timeout
        }
    }

    return true;
}

template<typename Mutex, typename Clock, typename Duration>
cv_status ConditionVariableESP32::wait_until(
    unique_lock<Mutex>& lock,
    const std::chrono::time_point<Clock, Duration>& abs_time) {  // okay std namespace

    // Convert absolute time to relative duration
    auto now = Clock::now();
    if (abs_time <= now) {
        return cv_status::timeout;
    }

    auto rel_time = abs_time - now;
    return wait_for(lock, rel_time);
}

template<typename Mutex, typename Clock, typename Duration, typename Predicate>
bool ConditionVariableESP32::wait_until(
    unique_lock<Mutex>& lock,
    const std::chrono::time_point<Clock, Duration>& abs_time,  // okay std namespace
    Predicate pred) {

    while (!pred()) {
        auto now = Clock::now();
        if (now >= abs_time) {
            return pred();
        }

        if (wait_until(lock, abs_time) == cv_status::timeout) {
            return pred();
        }
    }

    return true;
}

//=============================================================================
// Explicit Template Instantiations
//=============================================================================

// Instantiate for MutexESP32 (the most common use case)
template void ConditionVariableESP32::wait(unique_lock<MutexESP32>&);
template void ConditionVariableESP32::wait(unique_lock<RecursiveMutexESP32>&);

// Common predicate type: lambda returning bool
template void ConditionVariableESP32::wait(unique_lock<MutexESP32>&, bool(*)());

// wait_for with common duration types
template cv_status ConditionVariableESP32::wait_for(
    unique_lock<MutexESP32>&,
    const std::chrono::duration<long long, std::milli>&);  // okay std namespace

template cv_status ConditionVariableESP32::wait_for(
    unique_lock<MutexESP32>&,
    const std::chrono::duration<long long, std::micro>&);  // okay std namespace

// wait_until with common clock types
template cv_status ConditionVariableESP32::wait_until(
    unique_lock<MutexESP32>&,
    const std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long long, std::nano>>&);  // okay std namespace

} // namespace platforms
} // namespace fl

#endif // ESP32
