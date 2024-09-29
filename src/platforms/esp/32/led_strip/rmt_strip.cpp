#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT51

#include "rmt_strip.h"
#include "esp_log.h"
#include "configure_led.h"
#include "construct.h"

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

// WS2812 timings.
const uint16_t T0H = 300; // 0.3us
const uint16_t T0L = 900; // 0.9us
const uint16_t T1H = 600; // 0.6us
const uint16_t T1L = 600; // 0.6us
const uint32_t TRESET = 280000; // 280us (WS2812-V5)

class RmtLedStrip : public IRmtLedStrip {
public:

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
        mLedStrip = construct_led_strip(
            T0H, T0L, T1H, T1L, TRESET,
            mPin, mMaxLeds, mIsRgbw, mBuffer);
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
