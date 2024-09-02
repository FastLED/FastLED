/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "enabled.h"

#if FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN

#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>





#ifdef __cplusplus
extern "C" {
#endif


#include "esp_log.h"
#include "esp_check.h"
#include "driver/rmt_tx.h"

#include "led_strip.h"
#include "led_strip_interface.h"
#include "led_strip_rmt_encoder.h"

#define LED_STRIP_RMT_DEFAULT_RESOLUTION 10000000 // 10MHz resolution
#define LED_STRIP_RMT_DEFAULT_TRANS_QUEUE_SIZE 4
// the memory size of each RMT channel, in words (4 bytes)
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS 64
#else
#define LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS 48
#endif

// static const char *TAG = "led_strip_rmt";
#define TAG "led_strip_rmt"

typedef struct {
    led_strip_t base;
    rmt_channel_handle_t rmt_chan;
    rmt_encoder_handle_t strip_encoder;
    uint32_t strip_len;
    uint8_t bytes_per_pixel;
    uint8_t pixel_buf[];
} led_strip_rmt_obj;

static esp_err_t led_strip_rmt_set_pixel(led_strip_t *strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    ESP_RETURN_ON_FALSE(index < rmt_strip->strip_len, ESP_ERR_INVALID_ARG, TAG, "index out of maximum number of LEDs");
    uint32_t start = index * rmt_strip->bytes_per_pixel;
    // In thr order of GRB, as LED strip like WS2812 sends out pixels in this order
    rmt_strip->pixel_buf[start + 0] = green & 0xFF;
    rmt_strip->pixel_buf[start + 1] = red & 0xFF;
    rmt_strip->pixel_buf[start + 2] = blue & 0xFF;
    if (rmt_strip->bytes_per_pixel > 3) {
        rmt_strip->pixel_buf[start + 3] = 0;
    }
    return ESP_OK;
}

static esp_err_t led_strip_rmt_set_pixel_rgbw(led_strip_t *strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    ESP_RETURN_ON_FALSE(index < rmt_strip->strip_len, ESP_ERR_INVALID_ARG, TAG, "index out of maximum number of LEDs");
    ESP_RETURN_ON_FALSE(rmt_strip->bytes_per_pixel == 4, ESP_ERR_INVALID_ARG, TAG, "wrong LED pixel format, expected 4 bytes per pixel");
    uint8_t *buf_start = rmt_strip->pixel_buf + index * 4;
    // SK6812 component order is GRBW
    *buf_start = green & 0xFF;
    *++buf_start = red & 0xFF;
    *++buf_start = blue & 0xFF;
    *++buf_start = white & 0xFF;
    return ESP_OK;
}

static esp_err_t led_strip_rmt_refresh(led_strip_t *strip)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    rmt_transmit_config_t tx_conf = {
        .loop_count = 0,
    };

    ESP_RETURN_ON_ERROR(rmt_enable(rmt_strip->rmt_chan), TAG, "enable RMT channel failed");
    ESP_RETURN_ON_ERROR(rmt_transmit(rmt_strip->rmt_chan, rmt_strip->strip_encoder, rmt_strip->pixel_buf,
                                     rmt_strip->strip_len * rmt_strip->bytes_per_pixel, &tx_conf), TAG, "transmit pixels by RMT failed");
    ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(rmt_strip->rmt_chan, -1), TAG, "flush RMT channel failed");
    ESP_RETURN_ON_ERROR(rmt_disable(rmt_strip->rmt_chan), TAG, "disable RMT channel failed");
    return ESP_OK;
}

static esp_err_t led_strip_rmt_clear(led_strip_t *strip)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    // Write zero to turn off all leds
    memset(rmt_strip->pixel_buf, 0, rmt_strip->strip_len * rmt_strip->bytes_per_pixel);
    return led_strip_rmt_refresh(strip);
}

static esp_err_t led_strip_rmt_del(led_strip_t *strip)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    ESP_RETURN_ON_ERROR(rmt_del_channel(rmt_strip->rmt_chan), TAG, "delete RMT channel failed");
    ESP_RETURN_ON_ERROR(rmt_del_encoder(rmt_strip->strip_encoder), TAG, "delete strip encoder failed");
    free(rmt_strip);
    return ESP_OK;
}

rmt_tx_channel_config_t make_tx_channel_config(
    rmt_clock_source_t clock_src,
    size_t mem_block_symbols,
    uint32_t resolution_hz,
    int strip_gpio_num,
    size_t trans_queue_depth,
    int intr_priority,
    bool with_dma,
    bool invert_out
)
{
    rmt_tx_channel_config_t out = {};
    memset(&out, 0, sizeof(rmt_tx_channel_config_t));

    //     .clk_src = clock_src,
    //     .gpio_num = strip_gpio_num,
    //     .mem_block_symbols = mem_block_symbols,
    //     .resolution_hz = resolution_hz
    // };
    out.gpio_num = static_cast<gpio_num_t>(strip_gpio_num);
    out.clk_src = clock_src;
    out.resolution_hz = resolution_hz;
    out.mem_block_symbols = mem_block_symbols;
    out.trans_queue_depth = trans_queue_depth;
    out.intr_priority = intr_priority;
    out.flags.with_dma = with_dma;
    out.flags.invert_out = invert_out;
    return out;
}

#if FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN_COMPILE_PROBLEMATIC_CODE && !FASTLED_ESP32_COMPONENT_LED_STRIP_FORCE_IDF4

// Note that the RMT driver does a runtime check to see if the RMT new driver has been linked in. It doesn't
// matter if you install it or not, it will still crash on boot if it's linked in.
// https://github.com/espressif/esp-idf/blob/e026fd1f81afc6d19561101b1c8fe0006932cff3/components/driver/deprecated/rmt_legacy.c#L1379

esp_err_t led_strip_new_rmt_device(const led_strip_config_t *led_config, const led_strip_rmt_config_t *rmt_config, led_strip_handle_t *ret_strip)
{
    const int INTERRUPT_PRIORITY = 0;
    rmt_clock_source_t clk_src = RMT_CLK_SRC_DEFAULT;
    size_t mem_block_symbols = LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS;
    led_strip_rmt_obj *rmt_strip = NULL;
    esp_err_t ret = ESP_OK;
    led_strip_encoder_config_t strip_encoder_conf = {};
    uint32_t resolution = 0;
    // has to be declared before possible jump to error is needed
    rmt_tx_channel_config_t rmt_chan_config = {};
    uint8_t bytes_per_pixel = 3;
    ESP_GOTO_ON_FALSE(led_config && rmt_config && ret_strip, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE(led_config->led_pixel_format < LED_PIXEL_FORMAT_INVALID, ESP_ERR_INVALID_ARG, err, TAG, "invalid led_pixel_format");
    if (led_config->led_pixel_format == LED_PIXEL_FORMAT_GRBW) {
        bytes_per_pixel = 4;
    } else if (led_config->led_pixel_format == LED_PIXEL_FORMAT_GRB) {
        bytes_per_pixel = 3;
    } else {
        assert(false);
    }
    rmt_strip = static_cast<led_strip_rmt_obj*>(calloc(1, sizeof(led_strip_rmt_obj) + led_config->max_leds * bytes_per_pixel));
    ESP_GOTO_ON_FALSE(rmt_strip, ESP_ERR_NO_MEM, err, TAG, "no mem for rmt strip");
    resolution = rmt_config->resolution_hz ? rmt_config->resolution_hz : LED_STRIP_RMT_DEFAULT_RESOLUTION;

    // for backward compatibility, if the user does not set the clk_src, use the default value
    if (rmt_config->clk_src) {
        clk_src = rmt_config->clk_src;
    }

    // override the default value if the user sets it
    if (rmt_config->mem_block_symbols) {
        mem_block_symbols = rmt_config->mem_block_symbols;
    }
    // rmt_tx_channel_config_t rmt_chan_config = {
    //     .clk_src = clk_src,
    //     .gpio_num = led_config->strip_gpio_num,
    //     .mem_block_symbols = mem_block_symbols,
    //     .resolution_hz = resolution,
    //     .trans_queue_depth = LED_STRIP_RMT_DEFAULT_TRANS_QUEUE_SIZE,
    //     .flags.with_dma = rmt_config->flags.with_dma,
    //     .flags.invert_out = led_config->flags.invert_out,
    // };

    rmt_chan_config = make_tx_channel_config(
        clk_src,
        mem_block_symbols,
        resolution,
        led_config->strip_gpio_num,
        LED_STRIP_RMT_DEFAULT_TRANS_QUEUE_SIZE,
        INTERRUPT_PRIORITY,
        rmt_config->flags.with_dma,
        led_config->flags.invert_out
    );
    //ESP_GOTO_ON_ERROR(rmt_new_tx_channel(&rmt_chan_config, &rmt_strip->rmt_chan), err, TAG, "create RMT TX channel failed");

    strip_encoder_conf = {
        .resolution = resolution,
        .led_model = led_config->led_model
    };
    //ESP_GOTO_ON_ERROR(rmt_new_led_strip_encoder(&strip_encoder_conf, &rmt_strip->strip_encoder), err, TAG, "create LED strip encoder failed");


    rmt_strip->bytes_per_pixel = bytes_per_pixel;
    rmt_strip->strip_len = led_config->max_leds;
    rmt_strip->base.set_pixel = led_strip_rmt_set_pixel;
    rmt_strip->base.set_pixel_rgbw = led_strip_rmt_set_pixel_rgbw;
    rmt_strip->base.refresh = led_strip_rmt_refresh;
    rmt_strip->base.clear = led_strip_rmt_clear;
    rmt_strip->base.del = led_strip_rmt_del;

    *ret_strip = &rmt_strip->base;
    return ESP_OK;
err:
    if (rmt_strip) {
        if (rmt_strip->rmt_chan) {
            //rmt_del_channel(rmt_strip->rmt_chan);
        }
        if (rmt_strip->strip_encoder) {
            //rmt_del_encoder(rmt_strip->strip_encoder);
        }
        free(rmt_strip);
    }
    return ret;
}

#endif // FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN_COMPILE_PROBLEMATIC_CODE


#ifdef __cplusplus
}
#endif

#endif // ESP_IDF_VERSION
#endif // FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN
