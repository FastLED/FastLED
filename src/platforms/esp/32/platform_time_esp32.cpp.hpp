#pragma once

// IWYU pragma: private

// ESP32-specific platform time implementation
// Uses vTaskDelay for millisecond-scale delays to yield to FreeRTOS scheduler,
// enabling coroutines and background tasks to run during delays.
//
// Pure ESP-IDF implementation (esp_timer + FreeRTOS + ROM delay) — no Arduino
// core dependency. Identical under Arduino-ESP32 (which bundles ESP-IDF) and
// bare ESP-IDF builds.

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL)

#include "platforms/time_platform.h"
#include "platforms/esp/esp_version.h"

// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#if ESP_IDF_VERSION_4_OR_HIGHER
#include "esp_rom_sys.h"  // esp_rom_delay_us (ESP-IDF v4.0+)
#else
#include "rom/ets_sys.h"  // ets_delay_us (ESP-IDF v3.x)
#endif
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
#if ESP_IDF_VERSION_4_OR_HIGHER
        esp_rom_delay_us(remainder_us);
#else
        ets_delay_us(remainder_us);
#endif
    }
}

fl::u32 millis() {
    return static_cast<fl::u32>(esp_timer_get_time() / 1000);
}

fl::u32 micros() {
    return static_cast<fl::u32>(esp_timer_get_time());
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ESP32 && !FASTLED_STUB_IMPL
