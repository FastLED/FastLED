#pragma once

// IWYU pragma: private

// ESP32-specific coroutine runtime implementation
// Uses vTaskDelay to yield to FreeRTOS scheduler during event queue pumping.

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32) && defined(ARDUINO) && !defined(FASTLED_STUB_IMPL)

// IWYU pragma: begin_keep
#include "platforms/coroutine_runtime.h"
#include "fl/arduino.h"
#include "fl/singleton.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

class CoroutineRuntimeEsp32 : public ICoroutineRuntime {
public:
    void pumpEventQueueWithSleep(fl::u32 us) override {
        // Split into ms (yielding via vTaskDelay) and remainder us (busy-wait)
        fl::u32 ms = us / 1000;
        fl::u32 remainder_us = us % 1000;
        if (ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(ms));
        }
        if (remainder_us > 0) {
            ::delayMicroseconds(remainder_us);
        }
    }

    bool hasPlatformBackgroundEvents() override {
        return true;
    }

    void platformDelay(fl::u32 ms) override {
        if (ms == 0) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void platformDelayMicroseconds(fl::u32 us) override {
        // Split into ms (yielding via vTaskDelay) and remainder us (busy-wait)
        fl::u32 ms = us / 1000;
        fl::u32 remainder_us = us % 1000;
        if (ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(ms));
        }
        if (remainder_us > 0) {
            ::delayMicroseconds(remainder_us);
        }
    }

    fl::u32 platformMillis() override {
        return static_cast<fl::u32>(::millis());
    }

    fl::u32 platformMicros() override {
        return static_cast<fl::u32>(::micros());
    }

    void osYield() override {
        vTaskDelay(0);
    }
};

ICoroutineRuntime& ICoroutineRuntime::instance() {
    return fl::Singleton<CoroutineRuntimeEsp32>::instance();
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ESP32 && ARDUINO && !FASTLED_STUB_IMPL
