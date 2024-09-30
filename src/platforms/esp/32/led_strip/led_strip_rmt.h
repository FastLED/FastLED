/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once


#include <stdint.h>

#include "esp_err.h"
#include "led_strip_types.h"
#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/rmt_types.h"
#else
#error "This library is only designed to run on ESP-IDF v5.0.0 and later"
#endif


namespace fastled_rmt51_strip {

/**
 * @brief LED Strip RMT specific configuration
 */
typedef struct {
    rmt_clock_source_t clk_src; /*!< RMT clock source */
    uint32_t resolution_hz;     /*!< RMT tick resolution, if set to zero, a default resolution (10MHz) will be applied */
    size_t mem_block_symbols;   /*!< How many RMT symbols can one RMT channel hold at one time. Set to 0 will fallback to use the default size. */
    struct {
        uint32_t with_dma: 1;   /*!< Use DMA to transmit data */
    } flags;                    /*!< Extra driver flags */
} led_strip_rmt_config_t;

/**
 * @brief Create LED strip based on RMT TX channel
 *
 * @param led_config LED strip configuration
 * @param rmt_config RMT specific configuration
 * @param ret_strip Returned LED strip handle
 * @return
 *      - ESP_OK: create LED strip handle successfully
 *      - ESP_ERR_INVALID_ARG: create LED strip handle failed because of invalid argument
 *      - ESP_ERR_NO_MEM: create LED strip handle failed because of out of memory
 *      - ESP_FAIL: create LED strip handle failed because some other error
 */
esp_err_t led_strip_new_rmt_device(
        const led_strip_config_t *led_config,
        const led_strip_rmt_config_t *rmt_config,
        led_strip_handle_t *ret_strip);

// Create a new led strip with a pre-allocated pixel buffer.
esp_err_t led_strip_new_rmt_device_with_buffer(
        const led_strip_config_t *led_config,
        const led_strip_rmt_config_t *rmt_config,
        uint8_t *pixel_buf,
        led_strip_handle_t *ret_strip);

// release_pixel_buffer is true then the pixel buffer will also be freed.
esp_err_t led_strip_release_rmt_device(led_strip_handle_t strip, bool release_pixel_buffer);

}  // namespace fastled_rmt51_strip
