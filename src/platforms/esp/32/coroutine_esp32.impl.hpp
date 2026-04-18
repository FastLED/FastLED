#pragma once

// IWYU pragma: private

/// @file coroutine_esp32.impl.hpp
/// @brief ESP32 coroutine infrastructure — declarations and implementations
///
/// Contains FreeRTOS-based coroutine runtime, task coroutine class,
/// factory function, and exitCurrent.

#include "platforms/esp/is_esp.h"
#include "platforms/esp/esp_version.h"

#ifdef FL_IS_ESP32

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "platforms/coroutine.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "fl/stl/string.h"
#include "fl/stl/functional.h"
#include "fl/stl/atomic.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/singleton.h"
#include "fl/system/log.h"
#include "fl/system/arduino.h"
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
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "soc/soc_caps.h"
// IWYU pragma: end_keep
#ifndef SOC_WIFI_SUPPORTED
#define SOC_WIFI_SUPPORTED 0
#endif
#if SOC_WIFI_SUPPORTED
#include "esp_wifi.h"
// Weak symbol: resolves to nullptr when WiFi component is not linked
FL_LINK_WEAK esp_err_t esp_wifi_get_mode(wifi_mode_t *mode);
#endif
FL_EXTERN_C_END

namespace fl {
namespace platforms {

//=============================================================================
// Coroutine Runtime — yields to FreeRTOS scheduler
//=============================================================================

class CoroutineRuntimeEsp32 : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) FL_NOEXCEPT override {
        // Convert microseconds to ticks. On a 1 kHz tick rate 1 tick ≈ 1 ms;
        // on 100 Hz it's 10 ms.
        const fl::u32 tick_period_us = 1000000u / configTICK_RATE_HZ;
        fl::u32 ticks = us / tick_period_us;

        if (ticks > 0) {
            vTaskDelay(ticks);
        } else {
            taskYIELD();
        }
    }

    bool needsDeepYield() const FL_NOEXCEPT override {
        // Cache WiFi detection — recheck at most once per second.
        static TickType_t s_lastCheck = 0;
        static bool s_cached = false;
        TickType_t now = xTaskGetTickCount();
        if ((now - s_lastCheck) >= pdMS_TO_TICKS(1000)) {
            s_lastCheck = now;
            s_cached = isWiFiActive();
        }
        return s_cached;
    }

private:
    static bool isWiFiActive() FL_NOEXCEPT {
#if SOC_WIFI_SUPPORTED
        if (esp_wifi_get_mode == nullptr) {
            return false; // WiFi component not linked
        }
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL) {
            return true;
        }
#endif
        return false;
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
                                    u8 priority = 5,
                                    int core_id = -1) FL_NOEXCEPT;

    ~TaskCoroutineESP32() override;

    void stop() FL_NOEXCEPT override;
    bool isRunning() const FL_NOEXCEPT override;

private:
    TaskCoroutineESP32() = default;

    static void task_entry(void* param) FL_NOEXCEPT;

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

ICoroutineRuntime& ICoroutineRuntime::instance() FL_NOEXCEPT {
    return fl::Singleton<CoroutineRuntimeEsp32>::instance();
}

//=============================================================================
// FreeRTOS task entry — runs user function, marks completed
//=============================================================================

void TaskCoroutineESP32::task_entry(void* param) FL_NOEXCEPT {
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
        u8 priority,
        int core_id) FL_NOEXCEPT {
    if (stack_size < kMinStackSize) {
        stack_size = kMinStackSize;
    }

    // Resolve requested core_id against platform capabilities. An out-of-range
    // or negative value is silently downgraded to tskNO_AFFINITY — the caller's
    // task still runs, just without affinity. This matches the existing
    // behaviour where every caller was implicitly tskNO_AFFINITY.
    BaseType_t affinity = tskNO_AFFINITY;
    if (core_id >= 0 && core_id < FL_CPU_CORES) {
        affinity = static_cast<BaseType_t>(core_id);
    }

    TaskCoroutinePtr task(new TaskCoroutineESP32()) FL_NOEXCEPT;  // ok bare allocation
    auto* impl = static_cast<TaskCoroutineESP32*>(task.get());
    impl->mName = fl::move(name);
    impl->mFunction = fl::move(function);

#if ESP_IDF_VERSION_4_OR_HIGHER
    // IDF 4.0+: Allocate stack + TCB in internal RAM — SPIRAM crashes on ESP32-P4
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
        affinity);
#else
    // IDF 3.x: xTaskCreateStaticPinnedToCore may not be available
    // (configSUPPORT_STATIC_ALLOCATION not guaranteed). Use dynamic
    // allocation variant instead.
    BaseType_t rc = xTaskCreatePinnedToCore(
        task_entry,
        impl->mName.c_str(),
        stack_size,
        impl,                       // param = this
        tskIDLE_PRIORITY + priority,
        &impl->mTask,
        affinity);
    if (rc != pdPASS) {
        impl->mTask = nullptr;
    }
#endif

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

void TaskCoroutineESP32::stop() FL_NOEXCEPT {
    if (!mTask) return;
    mShouldStop.store(true);
    vTaskDelete(mTask);
    mTask = nullptr;
    mCompleted.store(true);
}

bool TaskCoroutineESP32::isRunning() const FL_NOEXCEPT {
    return !mCompleted.load();
}

//=============================================================================
// Factory function — wired into platforms/coroutine.impl.cpp.hpp dispatch
//=============================================================================

TaskCoroutinePtr createTaskCoroutine(fl::string name,
                                      ICoroutineTask::TaskFunction function,
                                      size_t stack_size,
                                      u8 priority,
                                      int core_id) FL_NOEXCEPT {
    return TaskCoroutineESP32::create(fl::move(name), fl::move(function),
                                      stack_size, priority, core_id);
}

//=============================================================================
// Static exitCurrent — delete current FreeRTOS task
//=============================================================================

void ICoroutineTask::exitCurrent() FL_NOEXCEPT {
    vTaskDelete(nullptr);  // Delete the calling task
    while (true) {}        // Should never reach here
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_ESP32
