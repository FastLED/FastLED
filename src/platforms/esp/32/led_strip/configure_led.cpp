
#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT51

#include "configure_led.h"
#include "esp_log.h"
#include <cstring>

LED_STRIP_NAMESPACE_BEGIN

#define TAG "construct_new_led_strip.cpp"

led_strip_handle_t construct_new_led_strip(config_led_t config) {
    // LED strip general initialization, according to your led board design
    // led_strip_config_t strip_config = {};
    // strip_config.strip_gpio_num = config.pin;
    // strip_config.max_leds = config.max_leds;
    // strip_config.led_pixel_format = config.rgbw;
    // strip_config.led_model = config.chipset;
    // strip_config.flags.invert_out = 0;
    led_strip_config_t strip_config = {
        .strip_gpio_num = config.pin,
        .max_leds = config.max_leds,
        .rmt_bytes_encoder_config = config.rmt_bytes_encoder_config,
        .flags = {
            .invert_out = 0,
            .rgbw = config.rgbw,
        },
    };

    // led_strip_rmt_config_t rmt_config = {};
    // memset(&rmt_config, 0, sizeof(led_strip_rmt_config_t));
    // rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    // rmt_config.resolution_hz = 10000000;  // This is hard coded in other places so leave it.
    // rmt_config.mem_block_symbols = config.mem_block_symbols;
    // rmt_config.flags.with_dma = config.with_dma;
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = config.mem_block_symbols,
        .flags = {
            .with_dma = config.with_dma,
        },
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    if (config.pixel_buf) {
        ESP_ERROR_CHECK(led_strip_new_rmt_device_with_buffer(
            &strip_config, &rmt_config, config.pixel_buf, &led_strip));
    } else {
        ESP_ERROR_CHECK(
            led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    }
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

LED_STRIP_NAMESPACE_END


#endif // FASTLED_RMT51

#endif // ESP32
