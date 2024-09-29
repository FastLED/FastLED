#pragma once

#include <stdint.h>
#include "led_strip.h"
#include "namespace.h"

LED_STRIP_NAMESPACE_BEGIN

#ifndef FASTLED_RMT_WITH_DMA
#define FASTLED_RMT_WITH_DMA 1
#else
#define FASTLED_RMT_WITH_DMA 0
#endif

#ifndef FASTLED_RMT_MEMBLOCK_SYMBOLS
#if FASTLED_RMT_WITH_DMA
#define FASTLED_RMT_MEMBLOCK_SYMBOLS 1024
#else
#define FASTLED_RMT_MEMBLOCK_SYMBOLS 0  // Let the library decide
#endif  // FASTLED_RMT_WITH_DMA
#endif  // FASTLED_RMT_MEMBLOCK_SYMBOLS

struct config_led_t {
    int pin;
    uint32_t max_leds;
    led_model_t chipset;
    led_pixel_format_t rgbw;
    size_t mem_block_symbols = FASTLED_RMT_MEMBLOCK_SYMBOLS;
    bool with_dma = FASTLED_RMT_WITH_DMA;
    uint8_t* pixel_buf = nullptr;
};


led_strip_handle_t construct_new_led_strip(config_led_t config);

LED_STRIP_NAMESPACE_END
