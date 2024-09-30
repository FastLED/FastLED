/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT5

#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#error "This library is only designed to run on ESP-IDF v5.0.0 and later"
#endif

#include "esp_log.h"
#include "esp_check.h"

#include "led_strip.h"
#include "led_strip_interface.h"



namespace fastled_rmt51_strip {


// static const char *TAG = "led_strip";
#define TAG "led_strip"


esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->set_pixel(strip, index, red, green, blue);
}

esp_err_t led_strip_set_pixel_hsv(led_strip_handle_t strip, uint32_t index, uint16_t hue, uint8_t saturation, uint8_t value)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;

    uint32_t rgb_max = value;
    uint32_t rgb_min = rgb_max * (255 - saturation) / 255.0f;

    uint32_t i = hue / 60;
    uint32_t diff = hue % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        red = rgb_max;
        green = rgb_min + rgb_adj;
        blue = rgb_min;
        break;
    case 1:
        red = rgb_max - rgb_adj;
        green = rgb_max;
        blue = rgb_min;
        break;
    case 2:
        red = rgb_min;
        green = rgb_max;
        blue = rgb_min + rgb_adj;
        break;
    case 3:
        red = rgb_min;
        green = rgb_max - rgb_adj;
        blue = rgb_max;
        break;
    case 4:
        red = rgb_min + rgb_adj;
        green = rgb_min;
        blue = rgb_max;
        break;
    default:
        red = rgb_max;
        green = rgb_min;
        blue = rgb_max - rgb_adj;
        break;
    }

    return strip->set_pixel(strip, index, red, green, blue);
}

esp_err_t led_strip_refresh_async(led_strip_handle_t strip) {
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->refresh_async(strip);
}

esp_err_t led_strip_wait_refresh_done(led_strip_handle_t strip, int32_t timeout_ms) {
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->wait_refresh_done(strip, timeout_ms);
}

esp_err_t led_strip_set_pixel_rgbw(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->set_pixel_rgbw(strip, index, red, green, blue, white);
}

esp_err_t led_strip_refresh(led_strip_handle_t strip)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->refresh(strip);
}

esp_err_t led_strip_clear(led_strip_handle_t strip)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->clear(strip);
}

esp_err_t led_strip_del(led_strip_handle_t strip, bool release_pixel_buffer)
{
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return strip->del(strip, release_pixel_buffer);
}

}  // namespace fastled_rmt51_strip

#endif // FASTLED_RMT5

#endif // ESP32
