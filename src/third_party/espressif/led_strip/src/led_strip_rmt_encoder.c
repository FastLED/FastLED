
#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT5

#include "fl/unused.h"


/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "led_strip_rmt_encoder.h"

static const char *TAG = "led_rmt_encoder";

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = 0;
    rmt_encode_state_t state = 0;
    size_t encoded_symbols = 0;
    switch (led_encoder->state) {
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    // fall-through
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 0; // back to the initial encoding session
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = 0;
    return ESP_OK;
}


esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    FASTLED_UNUSED(ret);
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE(config->led_model < LED_MODEL_INVALID, ESP_ERR_INVALID_ARG, err, TAG, "invalid led model");
    // Create a temporary config with the same base values
    led_strip_encoder_config_t timing_config = *config;
    const bool has_encoder_timings = config->timings.t0h || config->timings.t0l || config->timings.t1h || config->timings.t1l || config->timings.reset;

    if (!has_encoder_timings) {
        // Set the timing values based on LED model
        if (config->led_model == LED_MODEL_SK6812) {
            timing_config.timings = (led_strip_encoder_timings_t) {
                .t0h = 300,  // 0.3us = 300ns
                .t0l = 900,  // 0.9us = 900ns
                .t1h = 600,  // 0.6us = 600ns
                .t1l = 600,  // 0.6us = 600ns
                .reset = 280 // 280us
            };
        } else if (config->led_model == LED_MODEL_WS2812) {
            timing_config.timings = (led_strip_encoder_timings_t) {
                .t0h = 300,  // 0.3us = 300ns
                .t0l = 900,  // 0.9us = 900ns
                .t1h = 900,  // 0.9us = 900ns
                .t1l = 300,  // 0.3us = 300ns
                .reset = 280 // 280us
            };
        } else if (config->led_model == LED_MODEL_WS2811) {
            timing_config.timings = (led_strip_encoder_timings_t) {
                .t0h = 500,   // 0.5us = 500ns
                .t0l = 2000,  // 2.0us = 2000ns
                .t1h = 1200,  // 1.2us = 1200ns
                .t1l = 1300,  // 1.3us = 1300ns
                .reset = 50   // 50us
            };
        } else {
            return ESP_ERR_INVALID_ARG;
        }
    }


    // Delegate to the timing-based encoder creation
    return rmt_new_led_strip_encoder_with_timings(&timing_config, ret_encoder);

err:
    return ESP_ERR_INVALID_ARG;
}


esp_err_t rmt_new_led_strip_encoder_with_timings(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder) {
    esp_err_t ret = ESP_OK;
    FASTLED_UNUSED(ret);
    rmt_led_strip_encoder_t *led_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    
    led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;

    // Convert nanosecond timings to ticks using resolution
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = (float)config->timings.t0h * config->resolution / 1000000000, // Convert ns to ticks
            .level1 = 0,
            .duration1 = (float)config->timings.t0l * config->resolution / 1000000000,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = (float)config->timings.t1h * config->resolution / 1000000000,
            .level1 = 0,
            .duration1 = (float)config->timings.t1l * config->resolution / 1000000000,
        },
        .flags.msb_first = 1
    };

    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    // Convert reset time from microseconds to ticks
    uint32_t reset_ticks = config->resolution / 1000000 * config->timings.reset / 2;
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };
    
    *ret_encoder = &led_encoder->base;
    return ESP_OK;
err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
    return ret;
}


#endif  // FASTLED_RMT5

#endif // ESP32
