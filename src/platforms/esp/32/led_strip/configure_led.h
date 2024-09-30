#pragma once

#include <stdint.h>
#include "led_strip.h"

#include "defs.h"

namespace fastled_rmt51_strip {

struct config_led_t {
    int pin;
    uint32_t max_leds;
    // led_model_t chipset;
    bool rgbw;
    //led_pixel_format_t rgbw;
    rmt_bytes_encoder_config_t rmt_bytes_encoder_config;
    rmt_symbol_word_t reset_code;
    size_t mem_block_symbols = FASTLED_RMT_MEMBLOCK_SYMBOLS;
    bool with_dma = FASTLED_RMT_WITH_DMA;
    uint8_t* pixel_buf = nullptr;
};


esp_err_t construct_new_led_strip(config_led_t config, led_strip_handle_t* ret_strip);

}
