#pragma once

// SDK fault injection for staged AutoResearch only. Compiler aliases route
// production calls here; ordinary SDK calls and other tasks remain unchanged.
#include "fl/stl/atomic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"

namespace spi_provider_faults {
struct State {
    fl::atomic<TaskHandle_t> heapTask{nullptr};
    size_t heapSize = 0;
    unsigned heapFailures = 0;
    fl::atomic<TaskHandle_t> queueTask{nullptr};
    unsigned queueFailures = 0;
    fl::atomic<TaskHandle_t> spiTask{nullptr};
    unsigned spiTimeouts = 0;
    fl::atomic<TaskHandle_t> guardOwner{nullptr};
    fl::atomic<QueueHandle_t> guardQueue{nullptr};
    fl::atomic<bool> guardObserved{false};
};
inline State& state() { static State value; return value; }
}

#ifdef heap_caps_malloc
#undef heap_caps_malloc
extern "C" void* heap_caps_malloc(size_t, uint32_t);
extern "C" void* fl_test_heap_caps_malloc(size_t bytes, uint32_t caps) {
    auto& fault = spi_provider_faults::state();
    if (fault.heapTask && fault.heapTask == xTaskGetCurrentTaskHandle() && bytes == fault.heapSize) {
        ++fault.heapFailures;
        return nullptr;
    }
    return heap_caps_malloc(bytes, caps);
}
#endif

#ifdef xQueueCreateMutex
#undef xQueueCreateMutex
extern "C" QueueHandle_t xQueueCreateMutex(uint8_t);
extern "C" QueueHandle_t fl_test_xQueueCreateMutex(uint8_t kind) {
    auto& fault = spi_provider_faults::state();
    if (fault.queueTask && fault.queueTask == xTaskGetCurrentTaskHandle()) {
        fault.queueTask = nullptr;
        ++fault.queueFailures;
        return nullptr;
    }
    return xQueueCreateMutex(kind);
}
#endif

#ifdef spi_device_get_trans_result
#undef spi_device_get_trans_result
extern "C" esp_err_t spi_device_get_trans_result(spi_device_handle_t, spi_transaction_t**, TickType_t);
extern "C" esp_err_t fl_test_spi_get_trans_result(spi_device_handle_t device, spi_transaction_t** result, TickType_t ticks) {
    auto& fault = spi_provider_faults::state();
    if (fault.spiTask && fault.spiTask == xTaskGetCurrentTaskHandle() && fault.spiTimeouts) {
        --fault.spiTimeouts;
        return ESP_ERR_TIMEOUT;
    }
    return spi_device_get_trans_result(device, result, ticks);
}
#endif

#ifdef xQueueSemaphoreTake
#undef xQueueSemaphoreTake
extern "C" BaseType_t xQueueSemaphoreTake(QueueHandle_t, TickType_t);
extern "C" BaseType_t fl_test_xQueueSemaphoreTake(QueueHandle_t queue, TickType_t ticks) {
    auto& fault = spi_provider_faults::state();
    if (fault.guardOwner == xTaskGetCurrentTaskHandle() && !fault.guardQueue) fault.guardQueue = queue;
    else if (fault.guardQueue && fault.guardQueue == queue) fault.guardObserved.store(true);
    return xQueueSemaphoreTake(queue, ticks);
}
#endif
