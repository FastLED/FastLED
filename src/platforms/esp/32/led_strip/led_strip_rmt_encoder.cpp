/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT5

#include "enabled.h"


// enable fprmissive
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"


#include "esp_check.h"


#include "led_strip_rmt_encoder.h"
#include "cleanup.h"


namespace fastled_rmt51_strip {

#define TAG "led_strip"

// Replaces C's ability to or enums together. This is a C++ version of the same thing.
template<typename EnumT>
EnumT enum_or(EnumT a, EnumT b) {
    return static_cast<EnumT>(static_cast<int>(a) | static_cast<int>(b));
}

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
    rmt_encode_state_t session_state = rmt_encode_state_t(0);
    rmt_encode_state_t state = rmt_encode_state_t(0);
    size_t encoded_symbols = 0;
    switch (led_encoder->state) {
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state = enum_or(state, RMT_ENCODING_MEM_FULL);
            goto out; // yield if there's no free space for encoding artifacts
        }
    // fall-through
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 0; // back to the initial encoding session
            //state |= RMT_ENCODING_COMPLETE;
            state = enum_or(state, RMT_ENCODING_COMPLETE);
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            //state |= RMT_ENCODING_MEM_FULL;
            state = enum_or(state, RMT_ENCODING_MEM_FULL);
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

void delete_encoder(rmt_led_strip_encoder_t *led_encoder) {
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
}

// now replace these with C++ versions that do the same thing as the jump statement
#undef ESP_GOTO_ON_FALSE
#undef ESP_GOTO_ON_ERROR

#define ESP_GOTO_ON_FALSE(a, err_code, goto_tag, log_tag, format, ...) do {                                \
        if (unlikely(!(a))) {                                                                              \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
            ret = err_code;                                                                                \
            return ret;                                                                                    \
        }                                                                                                  \
    } while (0)

#define ESP_GOTO_ON_ERROR(x, goto_tag, log_tag, format, ...) do {                                          \
        esp_err_t err_rc_ = (x);                                                                           \
        if (unlikely(err_rc_ != ESP_OK)) {                                                                 \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(,) __VA_ARGS__);        \
            ret = err_rc_;                                                                                 \
            return ret;                                                                                    \
        }                                                                                                  \
    } while(0)




esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;
    Cleanup cleanup_if_fialure(delete_encoder, led_encoder);

    // RmtLedStripEncoderDeleterOnError deleter(&ret, &led_encoder);
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    //ESP_GOTO_ON_FALSE(config->led_model < LED_MODEL_INVALID, ESP_ERR_INVALID_ARG, err, TAG, "invalid led model");
    led_encoder = static_cast<rmt_led_strip_encoder_t*>(calloc(1, sizeof(rmt_led_strip_encoder_t)));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;
    rmt_bytes_encoder_config_t bytes_encoder_config = config->bytes_encoder_config;
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");
    led_encoder->reset_code = config->reset_code;
    *ret_encoder = &led_encoder->base;
    cleanup_if_fialure.release();
    return ESP_OK;
}


#pragma GCC diagnostic pop


}  // namespace fastled_rmt51_strip

#endif // FASTLED_RMT5

#endif // ESP32