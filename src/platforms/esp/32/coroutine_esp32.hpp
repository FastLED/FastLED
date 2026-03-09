#pragma once

// IWYU pragma: private

/// @file coroutine_esp32.hpp
/// @brief ESP32 coroutine infrastructure — declarations and implementations
///
/// Contains FreeRTOS-based coroutine runtime, task coroutine class,
/// factory function, and exitCurrent.

#include "platforms/esp/is_esp.h"

#ifdef FL_IS_ESP32

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "platforms/coroutine.h"
#include "fl/stl/string.h"
#include "fl/stl/functional.h"
#include "fl/stl/atomic.h"
#include "fl/stl/unique_ptr.h"
#include "fl/singleton.h"
#include "fl/warn.h"
#include "fl/arduino.h"
// IWYU pragma: end_keep

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/task.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "esp_heap_caps.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// Coroutine Runtime — yields to FreeRTOS scheduler
//=============================================================================

class CoroutineRuntimeEsp32 : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) override {
        fl::u32 ms = us / 1000;
        fl::u32 remainder_us = us % 1000;
        if (ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(ms));
        }
        if (remainder_us > 0) {
            ::delayMicroseconds(remainder_us);
        }
    }
};

//=============================================================================
// Task Coroutine — direct FreeRTOS tasks
//=============================================================================

/// @brief Custom deleter for heap_caps_malloc'd memory (internal RAM)
struct HeapCapsDeleter {
    void operator()(void* ptr) const {
        if (ptr) heap_caps_free(ptr);
    }
};

/// @brief Typed unique_ptr aliases for FreeRTOS internal-RAM allocations
using UniqueStackBuf = fl::unique_ptr<StackType_t[], HeapCapsDeleter>;
using UniqueTcb = fl::unique_ptr<StaticTask_t, HeapCapsDeleter>;

/// @brief ESP32 task coroutine using direct FreeRTOS tasks
class TaskCoroutineESP32 : public ICoroutineTask {
public:
    static TaskCoroutinePtr create(fl::string name,
                                    TaskFunction function,
                                    size_t stack_size = 4096,
                                    u8 priority = 5);

    ~TaskCoroutineESP32() override;

    void stop() override;
    bool isRunning() const override;

private:
    TaskCoroutineESP32() = default;

    static void task_entry(void* param);

    fl::string mName;
    TaskFunction mFunction;
    TaskHandle_t mTask = nullptr;
    UniqueStackBuf mStackBuf;
    UniqueTcb mTaskTcb;
    fl::atomic<bool> mCompleted{false};
    fl::atomic<bool> mShouldStop{false};
};

//=============================================================================
// Coroutine Runtime singleton
//=============================================================================

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeEsp32>::instance();
}

//=============================================================================
// FreeRTOS task entry — runs user function, marks completed
//=============================================================================

void TaskCoroutineESP32::task_entry(void* param) {
    auto* self = static_cast<TaskCoroutineESP32*>(param);
    if (self->mFunction) {
        self->mFunction();
    }
    self->mCompleted.store(true);
    // Block forever — task will be deleted by the destructor or stop()
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

//=============================================================================
// TaskCoroutineESP32 Implementation
//=============================================================================

static constexpr size_t kMinStackSize = 2048;

TaskCoroutinePtr TaskCoroutineESP32::create(
        fl::string name,
        TaskFunction function,
        size_t stack_size,
        u8 priority) {
    if (stack_size < kMinStackSize) {
        stack_size = kMinStackSize;
    }

    TaskCoroutinePtr task(new TaskCoroutineESP32());  // ok bare allocation
    auto* impl = static_cast<TaskCoroutineESP32*>(task.get());
    impl->mName = fl::move(name);
    impl->mFunction = fl::move(function);

    // Allocate stack + TCB in internal RAM — SPIRAM crashes on ESP32-P4
    impl->mStackBuf.reset(static_cast<StackType_t*>(
        heap_caps_malloc(stack_size * sizeof(StackType_t),
                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    impl->mTaskTcb.reset(static_cast<StaticTask_t*>(
        heap_caps_malloc(sizeof(StaticTask_t),
                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    if (!impl->mStackBuf || !impl->mTaskTcb) {
        FL_WARN("TaskCoroutineESP32: Failed to allocate stack/TCB for '"
                << impl->mName << "'");
        task.reset();
        return nullptr;
    }

    impl->mTask = xTaskCreateStaticPinnedToCore(
        task_entry,
        impl->mName.c_str(),
        stack_size,
        impl,                       // param = this
        tskIDLE_PRIORITY + priority,
        impl->mStackBuf.get(),
        impl->mTaskTcb.get(),
        tskNO_AFFINITY);

    if (!impl->mTask) {
        FL_WARN("TaskCoroutineESP32: Failed to create FreeRTOS task for '"
                << impl->mName << "'");
        task.reset();
        return nullptr;
    }

    return task;
}

TaskCoroutineESP32::~TaskCoroutineESP32() {
    stop();
    // mStackBuf and mTaskTcb are freed automatically by unique_ptr destructors
}

void TaskCoroutineESP32::stop() {
    if (!mTask) return;
    mShouldStop.store(true);
    vTaskDelete(mTask);
    mTask = nullptr;
    mCompleted.store(true);
}

bool TaskCoroutineESP32::isRunning() const {
    return !mCompleted.load();
}

//=============================================================================
// Factory function — wired into platforms/coroutine.cpp.hpp dispatch
//=============================================================================

TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ICoroutineTask::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int /*core_id*/) {
    return TaskCoroutineESP32::create(fl::move(name), fl::move(function),
                                      stack_size, priority);
}

//=============================================================================
// Static exitCurrent — delete current FreeRTOS task
//=============================================================================

void ICoroutineTask::exitCurrent() {
    vTaskDelete(nullptr);  // Delete the calling task
    while (true) {}        // Should never reach here
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_ESP32
