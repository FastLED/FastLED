#pragma once

#include "led_strip.h"
#include "namespace.h"

LED_STRIP_NAMESPACE_BEGIN

struct config_led_t {
    int pin;
    uint32_t max_leds;
    led_model_t chipset;
    led_pixel_format_t rgbw;
};


led_strip_handle_t configure_led(config_led_t config);

LED_STRIP_NAMESPACE_END
