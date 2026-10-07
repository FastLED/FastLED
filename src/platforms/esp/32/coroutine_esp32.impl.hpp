#pragma once

// IWYU pragma: private

/// @file coroutine_esp32.impl.hpp
/// @brief ESP32 coroutine runtime for FreeRTOS yielding
///
/// Task coroutine creation lives in the independent coroutine_tasks unit.

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
#include "fl/log/log.h"
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
FL_EXTERN_C_END

namespace fl {
namespace platforms {

class CoroutineRuntimeEsp32 : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32 us) FL_NO_EXCEPT override {
        // Convert microseconds to ticks. On a 1 kHz tick rate 1 tick ≈ 1 ms;
        // on 100 Hz it's 10 ms.
        const fl::u32 tick_period_us = 1000000u / configTICK_RATE_HZ;
        fl::u32 ticks = us / tick_period_us;

        if (us == 0) {
            // Caller explicitly asked for no delay — just yield among
            // equal/higher priority tasks.
            taskYIELD();
            return;
        }

        // Caller asked for a non-zero wait. Round UP to at least 1 tick so
        // that lower-priority tasks (e.g. lwIP/WiFi) actually get CPU time.
        // Otherwise, on default ESP-IDF tick rates (100 Hz, tick_period_us
        // = 10 000) a sub-tick request such as pumpCoroutines(1000) would
        // truncate to ticks == 0 and degenerate into taskYIELD(), which
        // does NOT yield to lower-priority tasks. That regression was the
        // root cause of websocket starvation on ESP32-S3 I2S wait loops
        // (see https://github.com/FastLED/FastLED/issues/2254, Issue 1).
        if (ticks == 0) {
            ticks = 1;
        }
        vTaskDelay(ticks);
    }

};

ICoroutineRuntime& ICoroutineRuntime::instance() FL_NO_EXCEPT {
    return fl::Singleton<CoroutineRuntimeEsp32>::instance();
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_ESP32
