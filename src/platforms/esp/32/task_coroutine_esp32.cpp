/// @file task_coroutine_freertos.cpp
/// @brief ESP32 FreeRTOS TaskCoroutine platform implementation

#ifdef ESP32

#include "fl/task.h"
#include "fl/log.h"
#include "fl/warn.h"

// Include FreeRTOS headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
FL_EXTERN_C_END

namespace fl {

//=============================================================================
// TaskCoroutine Platform Implementation (ESP32/FreeRTOS)
//=============================================================================

// FreeRTOS task wrapper that calls the fl::function
static void taskWrapperFunction(void* arg) {
    auto* function = static_cast<TaskCoroutine::TaskFunction*>(arg);
    if (function) {
        (*function)();  // Call the fl::function (no arguments)
    }
    // Task should NOT return - it should call TaskCoroutine::exitCurrent()
    // If it does return, self-delete to prevent FreeRTOS assertion
    vTaskDelete(NULL);
}

TaskCoroutine::Handle TaskCoroutine::createTaskImpl(const fl::string& name,
                                                     const TaskFunction& function,
                                                     size_t stack_size,
                                                     uint8_t priority) {
    TaskHandle_t handle = nullptr;

    // Convert stack size from bytes to words (FreeRTOS uses words)
    // ESP32 uses 32-bit words (4 bytes per word)
    const size_t stack_words = stack_size / sizeof(uint32_t);

    // Pass the function object address as user_data
    // SAFETY: The TaskCoroutine object owns mFunction, which lives as long as the task
    BaseType_t result = xTaskCreate(
        taskWrapperFunction,
        name.c_str(),  // FreeRTOS expects const char*
        stack_words,
        const_cast<TaskFunction*>(&function),  // Pass function address as user_data
        priority,
        &handle);

    if (result != pdPASS) {
        FL_WARN("TaskCoroutine: Failed to create task '" << name << "'");
        return nullptr;
    }

    return static_cast<Handle>(handle);
}

void TaskCoroutine::deleteTaskImpl(Handle handle) {
    if (handle == nullptr) {
        return;  // No-op for nullptr
    }

    TaskHandle_t task_handle = static_cast<TaskHandle_t>(handle);
    vTaskDelete(task_handle);
}

void TaskCoroutine::exitCurrentImpl() {
    // Self-delete the currently executing task
    // This function does NOT return on ESP32/FreeRTOS
    vTaskDelete(NULL);
}

} // namespace fl

#endif // ESP32
