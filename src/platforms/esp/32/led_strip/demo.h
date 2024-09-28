
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

LED_STRIP_NAMESPACE_END
