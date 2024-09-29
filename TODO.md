# Testing

  * Esp32 testing
    * https://github.com/marketplace/actions/esp32-qemu-runner will run a sketch for X seconds and see's if it crashes
      * There's specific tests we'd like to run with this including the WS2812 and APA102 tests to test the clockless and clocked drivers

# Feature Enhancements

  * I2S driver for ESP32 WS2812
    * https://github.com/hpwit/I2SClocklessLedDriver
      * Our copy is here: https://github.com/FastLED/FastLED/blob/master/src/platforms/esp/32/clockless_i2s_esp32.h
    * Apparently, this driver allows MASSIVE parallelization for WS2812
    * Timing guide for reducing RMT frequency https://github.com/Makuna/NeoPixelBus/pull/795
    * ESp32 LED guide
      * web: https://components.espressif.com/components/espressif/led_strip
      * repo: https://github.com/espressif/idf-extra-components/tree/60c14263f3b69ac6e98ecae79beecbe5c18d5596/led_strip
      * adafruit conversation on RMT progress: https://github.com/adafruit/Adafruit_NeoPixel/issues/375


# ESP32 - IDF 4.4
  * RMT: we should use the esp_err_trmt_translator_init
    * https://docs.espressif.com/projects/esp-idf/en/v3.3/api-reference/peripherals/rmt.html

# ESP32 - IDF 5.1
  * https://github.com/espressif/arduino-esp32/blob/gh-pages/LIBRARIES_TEST.md

## RMT driver for esp-idf 5.1

Porting notes

It looks like the 5.1 built in driver is going to be the easiest to get
working. There's a sync manager designed for running all the RMT drivers
together. This should be done on the onEndShow() call to the ClockLess
driver.

Good sources of info:
  * esp32 forum: https://esp32.com/viewtopic.php?f=13&t=41170&p=135831&hilit=RMT#p135831
  * esp32 example: https://components.espressif.com/components/espressif/led_strip

*Espressif Example*
```C++
/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  2
// Numbers of the LED in the strip
#define LED_STRIP_LED_NUMBERS 24
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static const char *TAG = "example";

led_strip_handle_t construct_new_led_strip(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void app_main(void)
{
    led_strip_handle_t led_strip = construct_new_led_strip();
    bool led_on_off = false;

    ESP_LOGI(TAG, "Start blinking LED strip");
    while (1) {
        if (led_on_off) {
            /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
            for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 5, 5, 5));
            }
            /* Refresh the strip to send data */
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
            ESP_LOGI(TAG, "LED ON!");
        } else {
            /* Set all LED off to clear all pixels */
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
            ESP_LOGI(TAG, "LED OFF!");
        }

        led_on_off = !led_on_off;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

```
