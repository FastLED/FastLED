#pragma once

// IWYU pragma: private

// ESP32-specific platform time implementation
// Uses vTaskDelay for millisecond-scale delays to yield to FreeRTOS scheduler,
// enabling coroutines and background tasks to run during delays.

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32) && defined(ARDUINO) && !defined(FASTLED_STUB_IMPL)

#include "platforms/time_platform.h"
#include "fl/arduino.h"

// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

void delay(fl::u32 ms) {
    if (ms == 0) {
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void delayMicroseconds(fl::u32 us) {
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

fl::u32 millis() {
    return static_cast<fl::u32>(::millis());
}

fl::u32 micros() {
    return static_cast<fl::u32>(::micros());
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ESP32 && ARDUINO && !FASTLED_STUB_IMPL
