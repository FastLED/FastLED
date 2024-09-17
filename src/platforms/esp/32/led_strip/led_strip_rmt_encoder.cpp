/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "enabled.h"

#if FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN

template<typename EnumT>
EnumT enum_or(EnumT a, EnumT b) {
    return static_cast<EnumT>(static_cast<int>(a) | static_cast<int>(b));
}

#define RMT_FREQ 1000000


#ifdef __cplusplus
extern "C" {
#endif


#include "esp_check.h"


#include "led_strip_rmt_encoder.h"

//static const char *TAG = "led_rmt_encoder";
#define TAG "led_rmt_encoder"

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
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
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

inline rmt_symbol_word_t make_symbol_word(uint16_t level0, uint16_t duration0, uint16_t level1, uint16_t duration1)
{
    struct Data {
        uint16_t duration0 : 15; /*!< Duration of level0 */
        uint16_t level0 : 1;     /*!< Level of the first part */
        uint16_t duration1 : 15; /*!< Duration of level1 */
        uint16_t level1 : 1;     /*!< Level of the second part */
    };
    Data data = {
        .duration0 = duration0,
        .level0 = level0,
        .duration1 = duration1,
        .level1 = level1,
    };
    static_assert(sizeof(data) == sizeof(rmt_symbol_word_t));
    rmt_symbol_word_t out = *reinterpret_cast<rmt_symbol_word_t*>(&data);
    return out;
    
}

esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;
    rmt_bytes_encoder_config_t bytes_encoder_config = {};
    rmt_copy_encoder_config_t copy_encoder_config = {};
    uint32_t reset_ticks = config->resolution / RMT_FREQ * 280 / 2; // reset code duration defaults to 280us to accomodate WS2812B-V5
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE(config->led_model < LED_MODEL_INVALID, ESP_ERR_INVALID_ARG, err, TAG, "invalid led model");
    led_encoder = static_cast<rmt_led_strip_encoder_t*>(calloc(1, sizeof(rmt_led_strip_encoder_t)));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;

    if (config->led_model == LED_MODEL_SK6812) {
        rmt_symbol_word_t bit0 = make_symbol_word(
            1,
            0.3 * config->resolution / RMT_FREQ,
            0,
            0.9 * config->resolution / RMT_FREQ); // T0H=0.3us, T0L=0.9us
        rmt_symbol_word_t bit1 = make_symbol_word(
            1,
            0.6 * config->resolution / RMT_FREQ,
            0,
            0.6 * config->resolution / RMT_FREQ); // T1H=0.6us, T1L=0.6us
        rmt_bytes_encoder_config_t config = {};
        config.bit0 = bit0;
        config.bit1 = bit1;
        config.flags.msb_first = 1;
        // bytes_encoder_config = (rmt_bytes_encoder_config_t) {
        //     .bit0 = bit0, // T0H=0.3us, T0L=0.9us
        //     //.bit0 = {
        //     //    .level0 = 1,
        //     //    .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
        //     //    .level1 = 0,
        //     //    .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
        //     //},
        //     // .bit1 = {
        //     //     .level0 = 1,
        //     //     .duration0 = 0.6 * config->resolution / 1000000, // T1H=0.6us
        //     //     .level1 = 0,
        //     //     .duration1 = 0.6 * config->resolution / 1000000, // T1L=0.6us
        //     // },
        //     .bit1 = bit1, // T1H=0.6us, T1L=0.6us
        //     .flags.msb_first = 1 // SK6812 transfer bit order: G7...G0R7...R0B7...B0(W7...W0)
        // };
    } else if (config->led_model == LED_MODEL_WS2812) {
        // different led strip might have its own timing requirements, following parameter is for WS2812
        rmt_symbol_word_t bit0 = make_symbol_word(
            1,
            0.3 * config->resolution / RMT_FREQ,
            0,
            0.9 * config->resolution / RMT_FREQ); // T0H=0.3us, T0L=0.9us
        rmt_symbol_word_t bit1 = make_symbol_word(
            1,
            0.9 * config->resolution / RMT_FREQ,
            0,
            0.3 * config->resolution / RMT_FREQ); // T1H=0.9us, T1L=0.3us
        rmt_bytes_encoder_config_t bytes_encoder_config = {0};
        bytes_encoder_config.bit0 = bit0;
        bytes_encoder_config.bit1 = bit1;
        bytes_encoder_config.flags.msb_first = 1;
        /*
        bytes_encoder_config = (rmt_bytes_encoder_config_t) {
            .bit0 = {
                .level0 = 1,
                .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
                .level1 = 0,
                .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
            },
            .bit1 = {
                .level0 = 1,
                .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
                .level1 = 0,
                .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
            },
            .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
        };
        */
    } else {
        assert(false);
    }
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");




    led_encoder->reset_code = make_symbol_word(0, reset_ticks, 0, reset_ticks);
    // led_encoder->reset_code = (rmt_symbol_word_t) {
    //     .level0 = 0,
    //     .duration0 = reset_ticks,
    //     .level1 = 0,
    //     .duration1 = reset_ticks,
    // };
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


#ifdef __cplusplus
}
#endif

#endif // FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN
