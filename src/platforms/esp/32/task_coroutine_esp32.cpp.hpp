/// @file task_coroutine_esp32.cpp
/// @brief ESP32 FreeRTOS TaskCoroutine platform implementation

#ifdef ESP32

#include "task_coroutine_esp32.h"
#include "fl/warn.h"

// Include FreeRTOS headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// TaskCoroutineESP32 Implementation
//=============================================================================

// FreeRTOS task wrapper that calls the fl::function
static void taskWrapperFunction(void* arg) {
    auto* function = static_cast<ITaskCoroutine::TaskFunction*>(arg);
    if (function) {
        (*function)();  // Call the fl::function (no arguments)
    }
    // Task should NOT return - it should call ITaskCoroutine::exitCurrent()
    // If it does return, self-delete to prevent FreeRTOS assertion
    vTaskDelete(NULL);
}

TaskCoroutineESP32::TaskCoroutineESP32(fl::string name,
                                       TaskFunction function,
                                       size_t stack_size,
                                       uint8_t priority)
    : mHandle(nullptr)
    , mName(fl::move(name))
    , mFunction(fl::move(function)) {

    TaskHandle_t handle = nullptr;

    // Convert stack size from bytes to words (FreeRTOS uses words)
    // ESP32 uses 32-bit words (4 bytes per word)
    const size_t stack_words = stack_size / sizeof(uint32_t);

    // Pass the function object address as user_data
    // SAFETY: The TaskCoroutineESP32 object owns mFunction, which lives as long as the task
    BaseType_t result = xTaskCreate(
        taskWrapperFunction,
        mName.c_str(),  // FreeRTOS expects const char*
        stack_words,
        &mFunction,  // Pass function address as user_data
        priority,
        &handle);

    if (result != pdPASS) {
        FL_WARN("TaskCoroutineESP32: Failed to create task '" << mName << "'");
        return;
    }

    mHandle = static_cast<void*>(handle);
}

TaskCoroutineESP32::~TaskCoroutineESP32() {
    stop();
}

void TaskCoroutineESP32::stop() {
    if (mHandle == nullptr) {
        return;  // No-op for nullptr
    }

    TaskHandle_t task_handle = static_cast<TaskHandle_t>(mHandle);
    vTaskDelete(task_handle);
    mHandle = nullptr;
}

bool TaskCoroutineESP32::isRunning() const {
    return mHandle != nullptr;
}

//=============================================================================
// Factory function
//=============================================================================

ITaskCoroutine* createTaskCoroutine(fl::string name,
                                     ITaskCoroutine::TaskFunction function,
                                     size_t stack_size,
                                     uint8_t priority) {
    return new TaskCoroutineESP32(fl::move(name), fl::move(function), stack_size, priority);
}

//=============================================================================
// Static exitCurrent implementation
//=============================================================================

void ITaskCoroutine::exitCurrent() {
    // Self-delete the currently executing task
    // This function does NOT return on ESP32/FreeRTOS
    vTaskDelete(NULL);
}

} // namespace platforms
} // namespace fl

#endif // ESP32
