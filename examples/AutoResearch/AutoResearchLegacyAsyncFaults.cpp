// Dedicated failure-test firmware only. Build with FL_RMT4_FAULT_TEST=1 and:
// -Wl,--wrap=xTaskCreatePinnedToCore -Wl,--wrap=xQueueCreateMutex
// -Wl,--wrap=rmt_driver_install
// No wrapper or control state is present in ordinary firmware.

#include "AutoResearchLegacyAsyncErrors.h"

#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES && FL_RMT4_ASYNC && defined(FL_RMT4_FAULT_TEST) && FL_RMT4_FAULT_TEST == 1 // ok platform specific - opt-in RMT4 SDK linker fault fixtures have no platform-neutral equivalent.
#include "fl/stl/atomic.h"
#include "fl/stl/cstring.h"

// SDK headers stay in this translation unit, never the fixture header.
extern "C" {
#include "freertos/FreeRTOS.h" // ok platform specific - exact wrapper ABI comes from the SDK's declarations.
#include "freertos/task.h" // ok platform specific - matches xTaskCreatePinnedToCore and identifies the arming task.
#include "freertos/queue.h" // ok platform specific - matches the actual xQueueCreateMutex signature.
#include "driver/rmt.h" // ok platform specific - matches rmt_driver_install without replacing the peripheral implementation.
}

namespace autoresearch {
namespace legacy_async_errors {
namespace {
fl::atomic_int armedFault{static_cast<int>(Fault::None)};
fl::atomic_uint failuresConsumed{0};
fl::atomic<TaskHandle_t> armingTask{nullptr}; // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
} // namespace

void armFault(Fault fault) FL_NO_EXCEPT {
    failuresConsumed.store(0);
    armingTask.store(xTaskGetCurrentTaskHandle()); // ok platform specific - avoid failing unrelated tasks' mutex allocations.
    armedFault.store(static_cast<int>(fault));
}

void disarmFault() FL_NO_EXCEPT { armedFault.store(static_cast<int>(Fault::None)); }
unsigned consumedFaults() FL_NO_EXCEPT { return failuresConsumed.load(); }

bool consumeFault(Fault fault) FL_NO_EXCEPT {
    int expected = static_cast<int>(fault);
    if (!armedFault.compare_exchange_strong(expected, static_cast<int>(Fault::None),
                                            fl::memory_order_acquire)) {
        return false;
    }
    failuresConsumed.fetch_add(1);
    return true;
}

bool isArmingTask() FL_NO_EXCEPT {
    return xTaskGetCurrentTaskHandle() == armingTask.load(); // ok platform specific - only the local driver constructor may consume mutex failure.
}

} // namespace legacy_async_errors
} // namespace autoresearch

extern "C" {
BaseType_t __real_xTaskCreatePinnedToCore(TaskFunction_t code, // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
    const char *const name, const uint32_t stackBytes, void *const argument,
    UBaseType_t priority, TaskHandle_t *const task, const BaseType_t core) FL_NO_EXCEPT; // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
QueueHandle_t __real_xQueueCreateMutex(const uint8_t queueType) FL_NO_EXCEPT; // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
esp_err_t __real_rmt_driver_install(rmt_channel_t channel, size_t receiveBytes, // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
                                   int interruptFlags) FL_NO_EXCEPT;

BaseType_t __wrap_xTaskCreatePinnedToCore(TaskFunction_t code, // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
    const char *const name, const uint32_t stackBytes, void *const argument,
    UBaseType_t priority, TaskHandle_t *const task, const BaseType_t core) FL_NO_EXCEPT { // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
    using namespace autoresearch::legacy_async_errors;
    if (name && fl::strcmp(name, "fl_rmt4") == 0 &&
        consumeFault(Fault::TaskAllocation)) {
        if (task) { *task = nullptr; }
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY; // ok platform specific - return the real SDK allocation failure from RMT4 fault linker wrappers; no neutral equivalent exists.
    }
    return __real_xTaskCreatePinnedToCore(code, name, stackBytes, argument, // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
                                          priority, task, core);
}

QueueHandle_t __wrap_xQueueCreateMutex(const uint8_t queueType) FL_NO_EXCEPT { // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
    using namespace autoresearch::legacy_async_errors;
    if (isArmingTask() && consumeFault(Fault::MutexAllocation)) {
        return nullptr;
    }
    return __real_xQueueCreateMutex(queueType); // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
}

esp_err_t __wrap_rmt_driver_install(rmt_channel_t channel, size_t receiveBytes, // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
                                   int interruptFlags) FL_NO_EXCEPT {
    using namespace autoresearch::legacy_async_errors;
    if (consumeFault(Fault::DriverInstall)) { return ESP_ERR_NO_MEM; } // ok platform specific - return the real SDK allocation failure from RMT4 fault linker wrappers; no neutral equivalent exists.
    return __real_rmt_driver_install(channel, receiveBytes, interruptFlags); // ok platform specific - RMT4 fault linker wrappers must match the exact FreeRTOS and RMT SDK ABI; no neutral wrapper exists.
}
}
#endif
