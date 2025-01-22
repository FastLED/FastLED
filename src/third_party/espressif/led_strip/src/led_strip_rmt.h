/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "led_strip_types.h"
#include "esp_idf_version.h"
#include "driver/rmt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED Strip RMT specific configuration
 */
typedef struct {
    rmt_clock_source_t clk_src; /*!< RMT clock source */
    uint32_t resolution_hz;     /*!< RMT tick resolution, if set to zero, a default resolution (10MHz) will be applied */
    size_t mem_block_symbols;   /*!< How many RMT symbols can one RMT channel hold at one time. Set to 0 will fallback to use the default size. */
    /*!< Extra RMT specific driver flags */
    struct led_strip_rmt_extra_config {
        uint32_t with_dma: 1;   /*!< Use DMA to transmit data */
    } flags;                    /*!< Extra driver flags */
    uint8_t interrupt_priority; /*!< RMT interrupt priority, 0-3 are valid values */
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
esp_err_t led_strip_new_rmt_device(const led_strip_config_t *led_config, const led_strip_rmt_config_t *rmt_config, led_strip_handle_t *ret_strip);

#ifdef __cplusplus
}
#endif
