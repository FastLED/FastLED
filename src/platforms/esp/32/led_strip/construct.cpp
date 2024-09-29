
#include "enabled.h"

#if FASTLED_RMT51

#include "namespace.h"

#include "rmt_strip.h"
#include "led_strip.h"
#include "esp_log.h"
#include "configure_led.h"
#include "led_strip_types.h"
#include "defs.h"

LED_STRIP_NAMESPACE_BEGIN

namespace {

rmt_bytes_encoder_config_t make_encoder(rmt_symbol_word_t* reset) {
    static_assert(LED_STRIP_RMT_DEFAULT_RESOLUTION == 10000000, "Assumes 10MHz");
    static const double ratio = 10.0f; // assumes 10mhz
    rmt_symbol_word_t bit0 = {
        .duration0 = static_cast<uint16_t>(0.3 * ratio), // T0H=0.3us
        .level0 = 1,
        .duration1 = static_cast<uint16_t>(0.9 * ratio), // T0L=0.9us
        .level1 = 0,
    };

    rmt_symbol_word_t bit1 = {
        .duration0 = static_cast<uint16_t>(0.6 * ratio), // T1H=0.6us
        .level0 = 1,
        .duration1 = static_cast<uint16_t>(0.6 * ratio), // T1L=0.6us
        .level1 = 0,
    };

    // reset code duration defaults to 280us to accomodate WS2812B-V5
    uint16_t reset_ticks = static_cast<uint16_t>(ratio * 280 / 2);
    *reset = {
        .duration0 = reset_ticks, // TRES=50us
        .level0 = 0,
        .duration1 = reset_ticks, // TRES=50us
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

config_led_t make_led_config(int pin, uint32_t max_leds, bool is_rgbw, uint8_t* pixel_buf) {
    rmt_symbol_word_t reset;
    rmt_bytes_encoder_config_t bytes_encoder_config = make_encoder(&reset);
    config_led_t config = {
        .pin = pin,
        .max_leds = max_leds,
        .rgbw = is_rgbw,
        .rmt_bytes_encoder_config = make_encoder(&reset),
        .reset_code = reset,
        .pixel_buf = pixel_buf
    };
    return config;
}

} // namespace


led_strip_handle_t construct_led_strip(int pin, uint32_t max_leds, bool is_rgbw, uint8_t* pixel_buf) {
    config_led_t config = make_led_config(pin, max_leds, is_rgbw, pixel_buf);
    led_strip_handle_t out = construct_new_led_strip(config);
    return out;
}

LED_STRIP_NAMESPACE_END

#endif // FASTLED_RMT51
