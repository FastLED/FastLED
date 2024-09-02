/// @file    Blink.ino
/// @brief   Blink the first LED of an LED strip
/// @example Blink.ino

#include <FastLED.h>


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platforms/esp/32/led_strip/led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  2
// Numbers of the LED in the strip
#define LED_STRIP_LED_NUMBERS 24
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static const char *TAG = "example";

led_strip_config_t make_config(
      int strip_gpio_num,
      uint32_t max_leds,
      led_pixel_format_t led_pixel_format,
      led_model_t led_model,
      uint32_t invert_out) {
    // Note this is the idf 5.1 code with the idf 4.4 code stripped out.
    led_strip_config_t config = {};
    config.strip_gpio_num = strip_gpio_num;
    config.max_leds = max_leds;
    config.led_pixel_format = led_pixel_format;
    config.led_model = led_model;
    config.flags.invert_out = invert_out;
    return config;
}

led_strip_rmt_config_t make_rmt_config(rmt_clock_source_t clk_src, uint32_t resolution_hz, size_t mem_block_symbols = 0, bool with_dma = false) {
    // assume #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    led_strip_rmt_config_t config = {};
    #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    config.rmt_channel = rmt_channel;
    #else
    config.clk_src = clk_src;
    config.resolution_hz = resolution_hz;
    #endif
    config.mem_block_symbols = mem_block_symbols;
    config.flags.with_dma = with_dma;
    return config;
}

led_strip_handle_t configure_led(void)
{

    led_strip_config_t strip_config = make_config(LED_STRIP_BLINK_GPIO, LED_STRIP_LED_NUMBERS, LED_PIXEL_FORMAT_GRB, LED_MODEL_WS2812, 0);
    led_strip_rmt_config_t rmt_config = make_rmt_config(RMT_CLK_SRC_DEFAULT, LED_STRIP_RMT_RES_HZ);

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void app_main(void)
{
    led_strip_handle_t led_strip = configure_led();
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

// How many leds in your strip?
#define NUM_LEDS 10

// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define DATA_PIN 9

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812, DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assume
    delay(2000);  // If something ever goes wrong this delay will allow upload.
}

void loop() { 
  // Fill the LED array with red, then show
  #if 0
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
  delay(500);
  // Fill the LED array with black (off), then show
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(500);
  #else
  app_main();
  #endif
}
