
#pragma once
#ifdef ESP32
#include "enabled.h"
#include <stdint.h>

#if FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN


enum {
    kDefaultRmtFreq = (10 * 1000 * 1000) // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
};

void rmt_demo(int led_strip_gpio, uint32_t num_leds, uint32_t rmt_res_hz = kDefaultRmtFreq);


#endif  // FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN
#endif  // ESP32