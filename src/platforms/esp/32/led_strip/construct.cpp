
#include "enabled.h"

#if FASTLED_RMT5



#include "rmt_strip.h"
#include "led_strip.h"
#include "esp_log.h"
#include "configure_led.h"
#include "led_strip_types.h"
#include "defs.h"

namespace fastled_rmt51_strip {

#define TAG "construct.cpp"

namespace {

rmt_bytes_encoder_config_t make_encoder_config(
        uint16_t T0H, uint16_t T0L, uint16_t T1H, uint16_t T1L, uint32_t TRESET,
        rmt_symbol_word_t* reset) {
    static_assert(LED_STRIP_RMT_DEFAULT_RESOLUTION == 10000000, "Assumes 10MHz");
    static const double ratio = .01; // assumes 10mhz
    uint16_t t0h = static_cast<uint16_t>(T0H * ratio);
    uint16_t t0l = static_cast<uint16_t>(T0L * ratio);
    uint16_t t1h = static_cast<uint16_t>(T1H * ratio);
    uint16_t t1l = static_cast<uint16_t>(T1L * ratio);
    uint16_t treset = static_cast<uint16_t>(TRESET * ratio *.5);
    rmt_symbol_word_t bit0 = {
        .duration0 = t0h,
        .level0 = 1,
        .duration1 = t0l,
        .level1 = 0,
    };

    rmt_symbol_word_t bit1 = {
        .duration0 = t1h,
        .level0 = 1,
        .duration1 = t1l,
        .level1 = 0,
    };

    // reset code duration defaults to 280us to accomodate WS2812B-V5
    uint16_t reset_ticks = treset;
    *reset = {
        .duration0 = reset_ticks,
        .level0 = 0,
        .duration1 = reset_ticks,
        .level1 = 0,
    };

    rmt_bytes_encoder_config_t out = {
        .bit0 = bit0,
        .bit1 = bit1,
        .flags = {
            .msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0(W7...W0)
        }
    };
    return out;
}

config_led_t make_led_config(
        uint16_t T0H, uint16_t T0L, uint16_t T1H, uint16_t T1L, uint32_t TRESET,
        int pin, uint32_t max_leds, bool is_rgbw, uint8_t* pixel_buf) {
    rmt_symbol_word_t reset;
    rmt_bytes_encoder_config_t bytes_encoder_config = make_encoder_config(
        T0H, T0L, T1H, T1L, TRESET, &reset);
    config_led_t config = {
        .pin = pin,
        .max_leds = max_leds,
        .rgbw = is_rgbw,
        .rmt_bytes_encoder_config = bytes_encoder_config,
        .reset_code = reset,
        .pixel_buf = pixel_buf
    };
    return config;
}

} // namespace

#define WARN_ON_ERROR(x, tag, format, ...) do { \
    esp_err_t err_rc_ = (x); \
    if (unlikely(err_rc_ != ESP_OK)) { \
        ESP_LOGW(tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(,) __VA_ARGS__); \
    } \
} while(0)

esp_err_t construct_led_strip(
        uint16_t T0H, uint16_t T0L, uint16_t T1H, uint16_t T1L, uint32_t TRESET,
        int pin, uint32_t max_leds, bool is_rgbw, uint8_t* pixel_buf,
        led_strip_handle_t* out) {
    config_led_t config = make_led_config(
        T0H, T0L, T1H, T1L, TRESET,
        pin, max_leds, is_rgbw, pixel_buf);
    esp_err_t err = construct_new_led_strip(config, out);
    WARN_ON_ERROR(err, TAG, "construct_new_led_strip failed");
    if (err != ESP_OK) {
        out = nullptr;
    }
    return err;
}

}  // namespace fastled_rmt51_strip

#endif // FASTLED_RMT5
