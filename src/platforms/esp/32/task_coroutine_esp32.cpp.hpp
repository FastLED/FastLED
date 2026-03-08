// IWYU pragma: private

/// @file task_coroutine_esp32.cpp
/// @brief ESP32 FreeRTOS TaskCoroutine platform implementation
///
/// Uses native FreeRTOS tasks. Each coroutine is a real FreeRTOS task
/// managed by the FreeRTOS scheduler. No artificial serialization —
/// tasks run preemptively as the RTOS schedules them.

#include "platforms/esp/is_esp.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/task_coroutine_esp32.h"
#include "fl/stl/atomic.h"
#include "fl/warn.h"

// Include FreeRTOS headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/task.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// TaskWrapperContext - shared state between task and owner
//=============================================================================

struct TaskWrapperContext {
    ITaskCoroutine::TaskFunction function;
    fl::atomic<bool> completed{false};
};

//=============================================================================
// TaskCoroutineESP32 Implementation
//=============================================================================

// FreeRTOS task entry point. Calls the user function, marks completion,
// then self-deletes.
static void taskWrapperFunction(void* arg) {
    auto* ctx = static_cast<TaskWrapperContext*>(arg);
    if (ctx && ctx->function) {
        ctx->function();
    }
    if (ctx) {
        ctx->completed.store(true);
    }
    // Self-delete. This function does NOT return.
    vTaskDelete(NULL);
}

TaskCoroutineESP32::TaskCoroutineESP32(fl::string name,
                                       TaskFunction function,
                                       size_t stack_size,
                                       u8 priority)
    : mHandle(nullptr)
    , mName(fl::move(name))
    , mContext(nullptr) {

    auto* ctx = new TaskWrapperContext(); // ok bare allocation
    ctx->function = fl::move(function);
    mContext = ctx;

    TaskHandle_t handle = nullptr;

    // Convert stack size from bytes to words (FreeRTOS uses words)
    // ESP32 uses 32-bit words (4 bytes per word)
    const size_t stack_words = stack_size / sizeof(u32);

    BaseType_t result = xTaskCreate(
        taskWrapperFunction,
        mName.c_str(),
        stack_words,
        ctx,       // Pass context as user_data
        priority,
        &handle);

    if (result != pdPASS) {
        FL_WARN("TaskCoroutineESP32: Failed to create task '" << mName << "'");
        delete ctx; // ok bare allocation
        mContext = nullptr;
        return;
    }

    mHandle = static_cast<void*>(handle);
}

TaskCoroutineESP32::~TaskCoroutineESP32() {
    stop();
    // Context is safe to delete after stop() — task is either
    // already self-deleted (completed) or we just deleted it.
    delete static_cast<TaskWrapperContext*>(mContext); // ok bare allocation
    mContext = nullptr;
}

void TaskCoroutineESP32::stop() {
    if (mHandle == nullptr) {
        return;
    }

    auto* ctx = static_cast<TaskWrapperContext*>(mContext);

    // Only delete the task if it hasn't already self-deleted
    if (!ctx || !ctx->completed.load()) {
        TaskHandle_t task_handle = static_cast<TaskHandle_t>(mHandle);
        vTaskDelete(task_handle);
    }

    mHandle = nullptr;
}

bool TaskCoroutineESP32::isRunning() const {
    if (mHandle == nullptr) {
        return false;
    }
    auto* ctx = static_cast<TaskWrapperContext*>(mContext);
    if (ctx && ctx->completed.load()) {
        return false;
    }
    return true;
}

//=============================================================================
// Factory function
//=============================================================================

ITaskCoroutine* createTaskCoroutine(fl::string name,
                                     ITaskCoroutine::TaskFunction function,
                                     size_t stack_size,
                                     u8 priority) {
    return new TaskCoroutineESP32(fl::move(name), fl::move(function), stack_size, priority);  // ok bare allocation
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

#endif // FL_IS_ESP32
