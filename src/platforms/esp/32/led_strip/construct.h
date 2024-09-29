#pragma once

#include "enabled.h"

#if FASTLED_RMT51

#include <stdint.h>
#include "namespace.h"
#include "led_strip_types.h"

LED_STRIP_NAMESPACE_BEGIN

led_strip_handle_t construct_led_strip(int pin, uint32_t max_leds, bool is_rgbw, uint8_t* pixel_buf);

LED_STRIP_NAMESPACE_END

#endif // FASTLED_RMT51