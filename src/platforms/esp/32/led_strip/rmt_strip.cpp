#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT51

#include "rmt_strip.h"
#include "demo.h"
#include "led_strip.h"
#include "esp_log.h"
#include "demo.h"
#include "configure_led.h"

#include "namespace.h"

LED_STRIP_NAMESPACE_BEGIN

#define TAG "rtm_strip.cpp"

namespace {
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

}  // namespace


RmtLedStrip::RmtLedStrip(int pin, uint32_t max_leds, bool is_rgbw) {
    LedStripMode mode = is_rgbw ? kWS2812_RGBW : kWS2812;
    mMaxLeds = max_leds;
    led_model_t chipset;
    led_pixel_format_t rgbw_mode;
    to_esp_modes(mode, &chipset, &rgbw_mode);

    config_led_t config = {
        .pin = pin,
        .max_leds = max_leds,
        .chipset = chipset,
        .rgbw = rgbw_mode
    };
    led_strip_handle_t led_strip = construct_new_led_strip(config);
    led_strip_rmt_obj *rmt_strip = __containerof(led_strip, led_strip_rmt_obj, base);
    mBuffer = rmt_strip->pixel_buf;
    is_rgbw = (mode == kWS2812_RGBW || mode == kSK6812_RGBW);
    mLedStrip = led_strip;
}

RmtLedStrip::~RmtLedStrip() {
    bool release_pixel_buffer = false;
    led_strip_handle_t led_strip = static_cast<led_strip_handle_t>(mLedStrip);
    led_strip_del(led_strip, release_pixel_buffer);
    free(mBuffer);
}

void RmtLedStrip::set_pixel(uint32_t i, uint8_t r, uint8_t g, uint8_t b) {
    // convert to
    led_strip_handle_t led_strip = static_cast<led_strip_handle_t>(mLedStrip);
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, g, r, b));
}

void RmtLedStrip::set_pixel_rgbw(uint32_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    led_strip_handle_t led_strip = static_cast<led_strip_handle_t>(mLedStrip);
    ESP_ERROR_CHECK(led_strip_set_pixel_rgbw(led_strip, i, g, r, b, w));
}

void RmtLedStrip::draw() {
    led_strip_handle_t led_strip = static_cast<led_strip_handle_t>(mLedStrip);
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

LED_STRIP_NAMESPACE_END

#endif  // FASTLED_RMT51

#endif  // ESP32