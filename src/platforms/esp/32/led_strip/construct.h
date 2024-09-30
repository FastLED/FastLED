#pragma once

#include "enabled.h"

#if FASTLED_RMT5

#include <stdint.h>

#include "led_strip_types.h"

namespace fastled_rmt51_strip {

esp_err_t construct_led_strip(
    uint16_t T0H, uint16_t T0L, uint16_t T1H, uint16_t T1L, uint32_t TRESET,
    int pin, uint32_t max_leds, bool is_rgbw, uint8_t* pixel_buf,
    led_strip_handle_t* out);

}  // namespace fastled_rmt51_strip

#endif // FASTLED_RMT5