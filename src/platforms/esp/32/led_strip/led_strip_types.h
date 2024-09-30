/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"


namespace fastled_rmt51_strip {

/**
 * @brief LED strip pixel format
 */
typedef enum {
    // Note - Zach Vorhies - Only this enum means that there is no re-ordering of the colors.
    LED_PIXEL_FORMAT_GRB,    /*!< Pixel format: GRB */
    LED_PIXEL_FORMAT_GRBW,   /*!< Pixel format: GRBW */
    LED_PIXEL_FORMAT_INVALID /*!< Invalid pixel format */
} led_pixel_format_t;

/**
 * @brief LED strip model
 * @note Different led model may have different timing parameters, so we need to distinguish them.
 */
typedef enum {
    LED_MODEL_WS2812, /*!< LED strip model: WS2812 */
    LED_MODEL_SK6812, /*!< LED strip model: SK6812 */
    LED_MODEL_INVALID /*!< Invalid LED strip model */
} led_model_t;

/**
 * @brief LED strip handle
 */
typedef struct led_strip_t *led_strip_handle_t;

/**
 * @brief LED Strip Configuration
 */
typedef struct {
    int strip_gpio_num;      /*!< GPIO number that used by LED strip */
    uint32_t max_leds;       /*!< Maximum LEDs in a single strip */
    //led_pixel_format_t led_pixel_format; /*!< LED pixel format */
    // led_model_t led_model;   /*!< LED model */
    rmt_bytes_encoder_config_t rmt_bytes_encoder_config; /*!< RMT bytes encoder configuration */
    rmt_symbol_word_t reset_code; /*!< Reset code for LED strip */
    struct {
        uint32_t invert_out: 1; /*!< Invert output signal */
        uint32_t rgbw: 1;       /*!< RGBW mode */
    } flags;                    /*!< Extra driver flags */


} led_strip_config_t;

}  // namespace fastled_rmt51_strip
