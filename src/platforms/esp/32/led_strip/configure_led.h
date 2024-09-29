#pragma once

#include "led_strip.h"
#include "namespace.h"

LED_STRIP_NAMESPACE_BEGIN

// #ifndef FASTLED_RMT_MEMBLOCK_SYMBOLS
// #define FASTLED_RMT_MEMBLOCK_SYMBOLS 64
// #endif

#ifndef FASTLED_RMT_WITH_DMA
#define FASTLED_RMT_WITH_DMA 0
#endif


struct config_led_t {
    int pin;
    uint32_t max_leds;
    led_model_t chipset;
    led_pixel_format_t rgbw;
    size_t mem_block_symbols = 0;  // Let the library decide
    bool with_dma = FASTLED_RMT_WITH_DMA;
};


led_strip_handle_t configure_led(config_led_t config);

LED_STRIP_NAMESPACE_END
