
#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT51

#include <Arduino.h>

#include "led_strip.h"
#include "demo.h"
#include "esp_log.h"
#include "configure_led.h"

#include "namespace.h"
LED_STRIP_NAMESPACE_BEGIN

#define TAG "rmt_demo.cpp"

#define DRAW_BLINK_DEMO

void to_esp_modes(LedStripMode mode, led_model_t* out_chipset, led_pixel_format_t* out_rgbw) {
    switch (mode) {
        case kWS2812:
            *out_rgbw = LED_PIXEL_FORMAT_GRB;
            *out_chipset = LED_MODEL_WS2812;
            break;
        case kSK6812:
            *out_rgbw = LED_PIXEL_FORMAT_GRB;
            *out_chipset = LED_MODEL_SK6812;
            break;
        case kWS2812_RGBW:
            *out_rgbw = LED_PIXEL_FORMAT_GRBW;
            *out_chipset = LED_MODEL_WS2812;
            break;
        case kSK6812_RGBW:
            *out_rgbw = LED_PIXEL_FORMAT_GRBW;
            *out_chipset = LED_MODEL_SK6812;
            break;
        default:
            ESP_LOGE(TAG, "Invalid LedStripMode");
            break;
    }
}

bool is_rgbw_mode_active(led_pixel_format_t rgbw_mode) {
    return rgbw_mode == LED_PIXEL_FORMAT_GRBW;
}

void convert_to_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    // This is a simple conversion that just takes the average of the RGB values and assigns it to the W value.
    // This is not a good conversion, but it is a simple one.
    *out_r = r;
    *out_g = g;
    *out_b = b;
    *out_w = 0;
}

void set_pixel(led_strip_handle_t led_strip, uint32_t index, bool is_rgbw_active, uint8_t r, uint8_t g, uint8_t b) {
    if (is_rgbw_active) {
        uint8_t w = 0;
        convert_to_rgbw(r, g, b, &r, &g, &b, &w);
        ESP_ERROR_CHECK(led_strip_set_pixel_rgbw(led_strip, index, g, r, b, w));
    } else {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, index, g, r, b));
    }
}

void draw_strip(led_strip_handle_t led_strip) {
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void draw_loop_color_cycle(led_strip_handle_t led_strip, uint32_t num_leds, bool rgbw_active) {
    const int MAX_BRIGHTNESS = 64;
    const float SPEED = 0.05f;
    float time = 0.0f;

    while (1) {        
        for (int i = 0; i < num_leds; i++) {
            float hue = fmodf(time + (float)i / num_leds, 1.0f);
            
            float r = MAX_BRIGHTNESS * (0.5f + 0.5f * std::sin(2 * PI * (hue + 0.0f / 3.0f)));
            float g = MAX_BRIGHTNESS * (0.5f + 0.5f * std::sin(2 * PI * (hue + 1.0f / 3.0f)));
            float b = MAX_BRIGHTNESS * (0.5f + 0.5f * std::sin(2 * PI * (hue + 2.0f / 3.0f)));
            
            set_pixel(led_strip, i, rgbw_active, r, g, b);
        }
        draw_strip(led_strip);

        time += SPEED;
    }
}

void draw_loop_blink_on_off_white(led_strip_handle_t led_strip, uint32_t num_leds, bool rgbw_active) {
    const int MAX_BRIGHTNESS = 5;
    bool led_on_off = false;
    while (1) {
        ESP_LOGI(TAG, "Looping");
        if (led_on_off) {
            /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each
             * color */
            uint8_t r = MAX_BRIGHTNESS;
            uint8_t g = MAX_BRIGHTNESS;
            uint8_t b = MAX_BRIGHTNESS;
            for (int i = 0; i < num_leds; i++) {
                set_pixel(led_strip, i, rgbw_active, r, g, b);
            }
            /* Refresh the strip to send data */
            draw_strip(led_strip);
            ESP_LOGI(TAG, "LED ON!");
            vTaskDelay(pdMS_TO_TICKS(8));
        } else {
            /* Set all LED off to clear all pixels */
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
            ESP_LOGI(TAG, "LED OFF!");
        }

        led_on_off = !led_on_off;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void draw_loop(led_strip_handle_t led_strip, uint32_t num_leds, bool rgbw_active) {

    #ifdef DRAW_BLINK_DEMO
    draw_loop_blink_on_off_white(led_strip, num_leds, rgbw_active);
    #else
    draw_loop_color_cycle(led_strip, num_leds, rgbw_active);
    #endif
}

void demo(int led_strip_gpio, uint32_t num_leds, LedStripMode mode) {
    led_pixel_format_t rgbw_mode = {};
    led_model_t chipset = {};
    to_esp_modes(mode, &chipset, &rgbw_mode);
    const bool is_rgbw_active = is_rgbw_mode_active(rgbw_mode);
    config_led_t config = {
        .pin = led_strip_gpio,
        .max_leds = num_leds,
        .chipset = chipset,
        .rgbw = rgbw_mode
    };
    led_strip_handle_t led_strip = configure_led(config);
    draw_loop(led_strip, num_leds, is_rgbw_active);
}


LED_STRIP_NAMESPACE_END

#endif // FASTLED_RMT51

#endif // ESP32
