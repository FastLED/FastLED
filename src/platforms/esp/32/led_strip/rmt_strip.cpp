#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT51

#include "rmt_strip.h"
#include "led_strip.h"
#include "esp_log.h"
#include "configure_led.h"
#include "led_strip_types.h"
#include "defs.h"

#include "namespace.h"

LED_STRIP_NAMESPACE_BEGIN

#define TAG "rtm_strip.cpp"

#define RMT_ASSERT(x)                  \
    {                                  \
        if (!(x)) {                    \
            ESP_ERROR_CHECK(ESP_FAIL); \
        }                              \
    }

#define RMT_ASSERT_LT(x, y)            \
    {                                  \
        if (!((x) < (y))) {            \
            ESP_ERROR_CHECK(ESP_FAIL); \
        }                              \
    }

static rmt_bytes_encoder_config_t make_encoder(rmt_symbol_word_t* reset) {
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

class RmtLedStrip : public IRmtLedStrip {
public:
    config_led_t make_config() const {
        rmt_symbol_word_t reset;
        rmt_bytes_encoder_config_t bytes_encoder_config = make_encoder(&reset);
        config_led_t config = {
            .pin = mPin,
            .max_leds = mMaxLeds,
            .rgbw = mIsRgbw,
            .rmt_bytes_encoder_config = make_encoder(&reset),
            .reset_code = reset,
            .pixel_buf = mBuffer
        };
        return config;
    }

    RmtLedStrip(int pin, uint32_t max_leds, bool is_rgbw)
        : mIsRgbw(is_rgbw),
          mPin(pin),
          mMaxLeds(max_leds) {
        const uint8_t bytes_per_pixel = is_rgbw ? 4 : 3;
        mBuffer = static_cast<uint8_t*>(calloc(max_leds, bytes_per_pixel));
    }

    void acquire_rmt() {
        assert(!mLedStrip);
        assert(!mAquired);
        config_led_t config = make_config();
        mLedStrip = construct_new_led_strip(config);
        mAquired = true;
    }

    void release_rmt() {
        if (!mAquired) {
            return;
        }
        led_strip_wait_refresh_done(mLedStrip, -1);
        if (mLedStrip) {
            led_strip_del(mLedStrip, false);
            mLedStrip = nullptr;
        }
        mAquired = false;
    }

    virtual ~RmtLedStrip() override {
        release_rmt();
        free(mBuffer);
    }

    virtual void set_pixel(uint32_t i, uint8_t r, uint8_t g, uint8_t b) override {
        RMT_ASSERT(!mAquired);
        RMT_ASSERT_LT(i, mMaxLeds);
        RMT_ASSERT(!mIsRgbw);
        mBuffer[i * 3 + 0] = r;
        mBuffer[i * 3 + 1] = g;
        mBuffer[i * 3 + 2] = b;
    }

    virtual void set_pixel_rgbw(uint32_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override {
        RMT_ASSERT(!mAquired);
        RMT_ASSERT_LT(i, mMaxLeds);
        RMT_ASSERT(mIsRgbw);
        mBuffer[i * 4 + 0] = r;
        mBuffer[i * 4 + 1] = g;
        mBuffer[i * 4 + 2] = b;
        mBuffer[i * 4 + 3] = w;
    }

    void draw_and_wait_for_completion() {
        ESP_ERROR_CHECK(led_strip_refresh(mLedStrip));
    }

    void draw_async() {
        ESP_ERROR_CHECK(led_strip_refresh_async(mLedStrip));
    }

    virtual void draw() override {
        release_rmt();
        if (!mAquired) {
            acquire_rmt();
        }
        draw_async();
    }

    virtual void wait_for_draw_complete() override {
        release_rmt();
    }

    virtual uint32_t num_pixels() const {
        return mMaxLeds;
    }

private:
    int mPin = -1;
    led_strip_handle_t mLedStrip = nullptr;
    bool mIsRgbw = false;
    uint32_t mMaxLeds = 0;
    uint8_t* mBuffer = nullptr;
    bool mAquired = false;
};

IRmtLedStrip* create_rmt_led_strip(int pin, uint32_t max_leds, bool is_rgbw) {
    return new RmtLedStrip(pin, max_leds, is_rgbw);
}

LED_STRIP_NAMESPACE_END

#endif  // FASTLED_RMT51

#endif  // ESP32
