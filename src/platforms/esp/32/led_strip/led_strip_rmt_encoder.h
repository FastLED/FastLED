/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "led_strip_types.h"
#include "driver/rmt_encoder.h"


namespace fastled_rmt51_strip {


/**
 * @brief Type of led strip encoder configuration
 */
typedef struct {
    uint32_t resolution;   /*!< Encoder resolution, in Hz */
    rmt_bytes_encoder_config_t bytes_encoder_config; /*!< RMT bytes encoder configuration */
    rmt_symbol_word_t reset_code; /*!< Reset code for LED strip */
} led_strip_encoder_config_t;

/**
 * @brief Create RMT encoder for encoding LED strip pixels into RMT symbols
 *
 * @param[in] config Encoder configuration
 * @param[out] ret_encoder Returned encoder handle
 * @return
 *      - ESP_ERR_INVALID_ARG for any invalid arguments
 *      - ESP_ERR_NO_MEM out of memory when creating led strip encoder
 *      - ESP_OK if creating encoder successfully
 */
esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);

}  // namespace fastled_rmt51_strip