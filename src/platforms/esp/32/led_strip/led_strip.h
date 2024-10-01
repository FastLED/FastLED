#pragma once

/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */




#ifdef ESP32

#include <stdint.h>
#include "esp_err.h"
#include "led_strip_rmt.h"
#include "platforms/esp/esp_version.h"
#include "led_strip_interface.h"


namespace fastled_rmt51_strip {


typedef struct {
    led_strip_t base;
    rmt_channel_handle_t rmt_chan;
    rmt_encoder_handle_t strip_encoder;
    uint32_t strip_len;
    uint8_t bytes_per_pixel;
    uint8_t* pixel_buf;
} led_strip_rmt_obj;

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
esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue);

/**
 * @brief Set RGBW for a specific pixel
 *
 * @note Only call this function if your led strip does have the white component (e.g. SK6812-RGBW)
 * @note Also see `led_strip_set_pixel` if you only want to specify the RGB part of the color and bypass the white component
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
esp_err_t led_strip_set_pixel_rgbw(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white);

/**
 * @brief Refresh memory colors to LEDs
 *
 * @param strip: LED strip
 *
 * @return
 *      - ESP_OK: Refresh successfully
 *      - ESP_FAIL: Refresh failed because some other error occurred
 *
 * @note:
 *      After updating the LED colors in the memory, a following invocation of this API is needed to flush colors to strip.
 */
esp_err_t led_strip_refresh(led_strip_handle_t strip);


/**
 * @brief Refresh memory colors to LEDs asynchronously
 *
 * @param strip: LED strip
 *
 * @return
 *      - ESP_OK: Asynchronous refresh started successfully
 *      - ESP_FAIL: Asynchronous refresh failed to start because some other error occurred
 *
 * @note:
 *      This function starts the refresh process asynchronously. It returns immediately without waiting for the refresh to complete.
 *      Use led_strip_wait_refresh_done to wait for the refresh to finish.
 */
esp_err_t led_strip_refresh_async(led_strip_handle_t strip);

/**
 * @brief Wait for an asynchronous refresh operation to complete
 *
 * @param strip: LED strip
 * @param timeout_ms: Timeout in milliseconds to wait for the refresh to complete
 *
 * @return
 *      - ESP_OK: Refresh completed successfully within the timeout period
 *      - ESP_ERR_TIMEOUT: Refresh did not complete within the specified timeout
 *      - ESP_FAIL: Waiting for refresh completion failed because some other error occurred
 *
 * @note:
 *      This function blocks until the asynchronous refresh operation completes or the timeout is reached.
 *      It should be called after led_strip_refresh_async to ensure the refresh process has finished.
 */
esp_err_t led_strip_wait_refresh_done(led_strip_handle_t strip, int32_t timeout_ms);

/**
 * @brief Clear LED strip (turn off all LEDs)
 *
 * @param strip: LED strip
 *
 * @return
 *      - ESP_OK: Clear LEDs successfully
 *      - ESP_FAIL: Clear LEDs failed because some other error occurred
 */
esp_err_t led_strip_clear(led_strip_handle_t strip);

/**
 * @brief Free LED strip resources
 *
 * @param strip: LED strip
 *
 * @return
 *      - ESP_OK: Free resources successfully
 *      - ESP_FAIL: Free resources failed because error occurred
 */
esp_err_t led_strip_del(led_strip_handle_t strip, bool release_pixel_buffer);

}  // namespace fastled_rmt51_strip

#endif
