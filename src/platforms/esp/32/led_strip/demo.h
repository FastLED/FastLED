
#pragma once

#include <stdint.h>

#include "namespace.h"
LED_STRIP_NAMESPACE_BEGIN

enum LedStripMode {
    WS2812,
    kSK6812,
    WS2812_RGBW,
    kSK6812_RGBW,
};


void demo(int led_strip_gpio, uint32_t num_leds, LedStripMode mode);
void draw_loop(led_strip_handle_t led_strip, uint32_t num_leds, bool rgbw_active);
void set_pixel(led_strip_handle_t led_strip, uint32_t index, bool is_rgbw_active, uint8_t r, uint8_t g, uint8_t b);

LED_STRIP_NAMESPACE_END
