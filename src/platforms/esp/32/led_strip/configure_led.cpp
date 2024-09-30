
#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT5

#include "configure_led.h"
#include "esp_log.h"
#include <cstring>

namespace fastled_rmt51_strip {

#define TAG "construct_new_led_strip.cpp"

esp_err_t construct_new_led_strip(config_led_t config, led_strip_handle_t* ret_strip) {
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = config.pin,
        .max_leds = config.max_leds,
        .rmt_bytes_encoder_config = config.rmt_bytes_encoder_config,
        .reset_code = config.reset_code,
        .flags = {
            .invert_out = 0,
            .rgbw = config.rgbw,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_DEFAULT_RESOLUTION,
        .mem_block_symbols = config.mem_block_symbols,
        .flags = {
            .with_dma = config.with_dma,
        },
    };

    // LED Strip object handle
    esp_err_t err = ESP_OK;
    if (config.pixel_buf) {
        err = led_strip_new_rmt_device_with_buffer(
            &strip_config, &rmt_config, config.pixel_buf, ret_strip);
    } else {
        err = led_strip_new_rmt_device(&strip_config, &rmt_config, ret_strip);
    }
    return err;
}

}


#endif // FASTLED_RMT5

#endif // ESP32
