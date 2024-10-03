/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT5

#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>


#include "esp_log.h"
#include "esp_check.h"
#include "driver/rmt_tx.h"

#include "led_strip.h"
#include "led_strip_interface.h"
#include "led_strip_rmt_encoder.h"
#include "defs.h"



#include "cleanup.h"


namespace fastled_rmt51_strip {

// static const char *TAG = "led_strip_rmt";
#define TAG "led_strip_rmt"



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

static esp_err_t led_strip_rmt_refresh_async(led_strip_t *strip)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    rmt_transmit_config_t tx_conf = {
        .loop_count = 0,
    };
    ESP_RETURN_ON_ERROR(rmt_enable(rmt_strip->rmt_chan), TAG, "enable RMT channel failed");
    ESP_RETURN_ON_ERROR(rmt_transmit(rmt_strip->rmt_chan, rmt_strip->strip_encoder, rmt_strip->pixel_buf,
                                     rmt_strip->strip_len * rmt_strip->bytes_per_pixel, &tx_conf), TAG, "transmit pixels by RMT failed");
    return ESP_OK;
}

static esp_err_t led_strip_rmt_wait_refresh_done(led_strip_t *strip, int32_t timeout_ms)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(rmt_strip->rmt_chan, timeout_ms), TAG, "wait for RMT channel done failed");
    ESP_RETURN_ON_ERROR(rmt_disable(rmt_strip->rmt_chan), TAG, "disable RMT channel failed");
    return ESP_OK;
} 

static esp_err_t led_strip_rmt_refresh(led_strip_t *strip)
{
    ESP_RETURN_ON_ERROR(led_strip_rmt_refresh_async(strip), TAG, "refresh LED strip failed");
    ESP_RETURN_ON_ERROR(led_strip_rmt_wait_refresh_done(strip, -1), TAG, "wait for RMT channel done failed");
    return ESP_OK;
}



static esp_err_t led_strip_rmt_clear(led_strip_t *strip)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    // Write zero to turn off all leds
    memset(rmt_strip->pixel_buf, 0, rmt_strip->strip_len * rmt_strip->bytes_per_pixel);
    return led_strip_rmt_refresh(strip);
}

#define WARN_ON_ERROR(x, tag, format, ...) do { \
    esp_err_t err_rc_ = (x); \
    if (unlikely(err_rc_ != ESP_OK)) { \
        ESP_LOGW(tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(,) __VA_ARGS__); \
    } \
} while(0)


static esp_err_t led_strip_rmt_release_channel(led_strip_t *strip)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    if (rmt_strip->rmt_chan) {
        WARN_ON_ERROR(rmt_del_channel(rmt_strip->rmt_chan), TAG, "delete RMT channel failed");
        rmt_strip->rmt_chan = NULL;
    }
    return ESP_OK;
}

static esp_err_t led_strip_rmt_release_encoder(led_strip_t *strip)
{
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    if (rmt_strip->strip_encoder) {
        WARN_ON_ERROR(rmt_del_encoder(rmt_strip->strip_encoder), TAG, "delete strip encoder failed");
        rmt_strip->strip_encoder = NULL;
    }
    return ESP_OK;
}


static esp_err_t led_strip_rmt_del(led_strip_t *strip)
{
    WARN_ON_ERROR(led_strip_rmt_release_channel(strip), TAG, "remove RMT channel failed");
    WARN_ON_ERROR(led_strip_rmt_release_encoder(strip), TAG, "remove RMT encoder failed");
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    uint8_t *pixel_buf = rmt_strip->pixel_buf;
    if (pixel_buf) {
        free(pixel_buf);
    }
    free(rmt_strip);
    return ESP_OK;
}


static esp_err_t led_strip_rmt_del2(led_strip_t *strip, bool free_pixel_buf)
{
    WARN_ON_ERROR(led_strip_rmt_release_channel(strip), TAG, "remove RMT channel failed");
    WARN_ON_ERROR(led_strip_rmt_release_encoder(strip), TAG, "remove RMT encoder failed");
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    if (free_pixel_buf) {
        uint8_t *pixel_buf = rmt_strip->pixel_buf;
        if (pixel_buf) {
            free(pixel_buf);
        }
    }
    free(rmt_strip);
    return ESP_OK;
}


void delete_strip(led_strip_rmt_obj *rmt_strip) {
    if (rmt_strip) {
        if (rmt_strip->rmt_chan) {
            rmt_del_channel(rmt_strip->rmt_chan);
        }
        if (rmt_strip->strip_encoder) {
            rmt_del_encoder(rmt_strip->strip_encoder);
        }
        uint8_t* pixel_buf = rmt_strip->pixel_buf;
        if (pixel_buf) {
            free(pixel_buf);
        }
        free(rmt_strip);
    }
}

void delete_strip_leave_buffer(led_strip_rmt_obj *rmt_strip) {
    if (rmt_strip) {
        if (rmt_strip->rmt_chan) {
            rmt_del_channel(rmt_strip->rmt_chan);
        }
        if (rmt_strip->strip_encoder) {
            rmt_del_encoder(rmt_strip->strip_encoder);
        }
        free(rmt_strip);
    }
}

#undef ESP_RETURN_ON_FALSE
#undef ESP_RETURN_ON_ERROR
#define ESP_RETURN_ON_FALSE(a, err_code, goto_tag, log_tag, format, ...) do {                                \
        if (unlikely(!(a))) {                                                                              \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
            ret = err_code;                                                                                \
            return ret;                                                                                    \
        }                                                                                                  \
    } while (0)

#define ESP_RETURN_ON_ERROR(x, goto_tag, log_tag, format, ...) do {                                          \
        esp_err_t err_rc_ = (x);                                                                           \
        if (unlikely(err_rc_ != ESP_OK)) {                                                                 \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
            ret = err_rc_;                                                                                 \
            return ret;                                                                                    \
        }                                                                                                  \
    } while(0)

esp_err_t led_strip_new_rmt_device_with_buffer(
        const led_strip_config_t *led_config,
        const led_strip_rmt_config_t *rmt_config,
        uint8_t *pixel_buf,
        led_strip_handle_t *ret_strip) {
    led_strip_rmt_obj *rmt_strip = NULL;
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(led_config && rmt_config && ret_strip, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    uint8_t bytes_per_pixel = led_config->flags.rgbw ? 4 : 3;
    uint32_t resolution = rmt_config->resolution_hz ? rmt_config->resolution_hz : LED_STRIP_RMT_DEFAULT_RESOLUTION;

    // for backward compatibility, if the user does not set the clk_src, use the default value
    rmt_clock_source_t clk_src = RMT_CLK_SRC_DEFAULT;
    if (rmt_config->clk_src) {
        clk_src = rmt_config->clk_src;
    }
    size_t mem_block_symbols = rmt_config->mem_block_symbols;
    assert(mem_block_symbols > 0);
    // override the default value if the user sets it
    //if (rmt_config->mem_block_symbols) {
    //    mem_block_symbols = rmt_config->mem_block_symbols;
    //}
    rmt_tx_channel_config_t rmt_chan_config = {
        .gpio_num = gpio_num_t(led_config->strip_gpio_num),
        .clk_src = clk_src,
        .resolution_hz = resolution,
        .mem_block_symbols = mem_block_symbols,
        .trans_queue_depth = LED_STRIP_RMT_DEFAULT_TRANS_QUEUE_SIZE,
        .intr_priority = FASTLED_RMT_INTERRUPT_PRIORITY,
        //.flags.with_dma = rmt_config->flags.with_dma,
        //.flags.invert_out = led_config->flags.invert_out,
        .flags = {
            .invert_out = led_config->flags.invert_out,
            .with_dma = rmt_config->flags.with_dma,
        }
    };
    // Temporary object to hold the RMT object. If acquiring the RMT channel fails, we can just return the error.
    // then we don't need to free the memory.
    led_strip_rmt_obj rmt_obj_tmp = {};
    rmt_obj_tmp.pixel_buf = pixel_buf;
    esp_err_t err = rmt_new_tx_channel(&rmt_chan_config, &rmt_obj_tmp.rmt_chan);
    if (err == ESP_ERR_NOT_FOUND) {  // No channels available
        // We failed but we didn't allocate from the heap yet, so we can just return the error.
        ret_strip = nullptr;
        return err;
    }
    // Some other error occurred.
    ESP_RETURN_ON_ERROR(err, err, TAG, "create RMT channel failed");
    // Creating the rmt object worked so go ahead and allocate from the heap.
    rmt_strip = static_cast<led_strip_rmt_obj*>(calloc(1, sizeof(led_strip_rmt_obj)));
    ESP_RETURN_ON_FALSE(rmt_strip, ESP_ERR_NO_MEM, err, TAG, "no mem for rmt strip");
    Cleanup cleanup_if_failure(delete_strip_leave_buffer, rmt_strip);
    // Okay to copy the temporary object to the heap.
    *rmt_strip = rmt_obj_tmp;
    led_strip_encoder_config_t strip_encoder_conf = {
        .resolution = resolution,
        .bytes_encoder_config = led_config->rmt_bytes_encoder_config,
        .reset_code = led_config->reset_code,
    };
    ESP_RETURN_ON_ERROR(rmt_new_led_strip_encoder(&strip_encoder_conf, &rmt_strip->strip_encoder), err, TAG, "create LED strip encoder failed");

    rmt_strip->bytes_per_pixel = bytes_per_pixel;
    rmt_strip->strip_len = led_config->max_leds;
    rmt_strip->base.set_pixel = led_strip_rmt_set_pixel;
    rmt_strip->base.set_pixel_rgbw = led_strip_rmt_set_pixel_rgbw;
    rmt_strip->base.refresh = led_strip_rmt_refresh;
    rmt_strip->base.clear = led_strip_rmt_clear;
    rmt_strip->base.del = led_strip_rmt_del2;
    rmt_strip->base.refresh_async = led_strip_rmt_refresh_async;
    rmt_strip->base.wait_refresh_done = led_strip_rmt_wait_refresh_done;

    *ret_strip = &rmt_strip->base;
    cleanup_if_failure.release();
    return ESP_OK;
}


esp_err_t led_strip_new_rmt_device(
        const led_strip_config_t *led_config,
        const led_strip_rmt_config_t *rmt_config,
        led_strip_handle_t *ret_strip)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(led_config && rmt_config && ret_strip, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    // ESP_RETURN_ON_FALSE(led_config->led_pixel_format < LED_PIXEL_FORMAT_INVALID, ESP_ERR_INVALID_ARG, err, TAG, "invalid led_pixel_format");
    //uint8_t bytes_per_pixel = (led_config->led_pixel_format == LED_PIXEL_FORMAT_GRBW) ? 4 : 3;
    uint8_t bytes_per_pixel = led_config->flags.rgbw ? 4 : 3;
    uint8_t* pixel_buf = static_cast<uint8_t*>(calloc(1, led_config->max_leds * bytes_per_pixel));
    ESP_RETURN_ON_FALSE(pixel_buf, ESP_ERR_NO_MEM, err, TAG, "no mem for pixel buffer");
    ret = led_strip_new_rmt_device_with_buffer(led_config, rmt_config, pixel_buf, ret_strip);
    if (ret != ESP_OK) {
        free(pixel_buf);
    }
    return ret;
}

esp_err_t led_strip_release_rmt_device(led_strip_handle_t strip, bool release_pixel_buffer) {
    led_strip_rmt_obj *rmt_strip = __containerof(strip, led_strip_rmt_obj, base);
    if (!release_pixel_buffer) {
        rmt_strip->pixel_buf = NULL;
    }
    return led_strip_rmt_del(strip);
}

}  // namespace fastled_rmt51_strip

#endif // FASTLED_RMT5

#endif // ESP32
