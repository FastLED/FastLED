/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"


namespace fastled_rmt51_strip {

typedef struct led_strip_t led_strip_t; /*!< Type of LED strip */

/**
 * @brief LED strip interface definition
 */
struct led_strip_t {
    /**
     * @brief Set RGB for a specific pixel
     *
     * @param strip: LED strip
     * @param index: index of pixel to set
     * @param red: red part of color
     * @param green: green part of color
     * @param blue: blue part of color
     *
     * @return
     *      - ESP_OK: Set RGB for a specific pixel successfully
     *      - ESP_ERR_INVALID_ARG: Set RGB for a specific pixel failed because of invalid parameters
     *      - ESP_FAIL: Set RGB for a specific pixel failed because other error occurred
     */
    esp_err_t (*set_pixel)(led_strip_t *strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue);

    /**
     * @brief Set RGBW for a specific pixel. Similar to `set_pixel` but also set the white component
     *
     * @param strip: LED strip
     * @param index: index of pixel to set
     * @param red: red part of color
     * @param green: green part of color
     * @param blue: blue part of color
     * @param white: separate white component
     *
     * @return
     *      - ESP_OK: Set RGBW color for a specific pixel successfully
     *      - ESP_ERR_INVALID_ARG: Set RGBW color for a specific pixel failed because of an invalid argument
     *      - ESP_FAIL: Set RGBW color for a specific pixel failed because other error occurred
     */
    esp_err_t (*set_pixel_rgbw)(led_strip_t *strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white);

    /**
     * @brief Refresh memory colors to LEDs (blocking)
     *
     * @param strip: LED strip
     *
     * @return
     *      - ESP_OK: Refresh successfully
     *      - ESP_FAIL: Refresh failed because some other error occurred
     *
     * @note:
     *      After updating the LED colors in the memory, a following invocation of this API is needed to flush colors to strip.
     *      This function blocks until the refresh is complete.
     */
    esp_err_t (*refresh)(led_strip_t *strip);

    /**
     * @brief Refresh memory colors to LEDs asynchronously
     *
     * @param strip: LED strip
     *
     * @return
     *      - ESP_OK: Async refresh started successfully
     *      - ESP_FAIL: Failed to start async refresh
     *
     * @note:
     *      This function starts the refresh process and returns immediately.
     *      Use wait_refresh_done to wait for the refresh to complete.
     */
    esp_err_t (*refresh_async)(led_strip_t *strip);

    /**
     * @brief Wait for an asynchronous refresh operation to complete
     *
     * @param strip: LED strip
     * @param timeout_ms: timeout value in milliseconds
     *
     * @return
     *      - ESP_OK: Refresh completed successfully within the timeout
     *      - ESP_ERR_TIMEOUT: Refresh did not complete within the specified timeout
     *      - ESP_FAIL: Waiting for refresh failed due to other errors
     */
    esp_err_t (*wait_refresh_done)(led_strip_t *strip, int32_t timeout_ms);

    /**
     * @brief Clear LED strip (turn off all LEDs)
     *
     * @param strip: LED strip
     * @param timeout_ms: timeout value for clearing task
     *
     * @return
     *      - ESP_OK: Clear LEDs successfully
     *      - ESP_FAIL: Clear LEDs failed because some other error occurred
     */
    esp_err_t (*clear)(led_strip_t *strip);

    /**
     * @brief Free LED strip resources
     *
     * @param strip: LED strip
     *
     * @return
     *      - ESP_OK: Free resources successfully
     *      - ESP_FAIL: Free resources failed because error occurred
     */
    esp_err_t (*del)(led_strip_t *strip, bool release_pixel_buffer);
};

}  // namespace fastled_rmt51_strip
