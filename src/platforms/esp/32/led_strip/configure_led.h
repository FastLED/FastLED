#pragma once

#include "led_strip.h"
#include "namespace.h"

LED_STRIP_NAMESPACE_BEGIN

led_strip_handle_t configure_led(int pin, uint32_t max_leds, led_model_t chipset, led_pixel_format_t rgbw);

LED_STRIP_NAMESPACE_END
