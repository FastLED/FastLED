#include "led_strip/enabled.h"

#if FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN

#include "idf5_rmt.h"


#include "led_strip/led_strip.h"
#include "esp_log.h"
#include "led_strip/demo.h"
#include "led_strip/configure_led.h"

USING_NAMESPACE_LED_STRIP

#define TAG "idf5_rmt.cpp"

void to_esp_modes(LedStripMode mode, led_model_t* out_chipset, led_pixel_format_t* out_rgbw) {
    switch (mode) {
        case WS2812:
            *out_rgbw = LED_PIXEL_FORMAT_GRB;
            *out_chipset = LED_MODEL_WS2812;
            break;
        case kSK6812:
            *out_rgbw = LED_PIXEL_FORMAT_GRB;
            *out_chipset = LED_MODEL_SK6812;
            break;
        case WS2812_RGBW:
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


#endif  // FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN

