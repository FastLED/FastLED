#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT5

#include "rmt_strip.h"
#include "esp_log.h"
#include "configure_led.h"
#include "construct.h"
#include "esp_check.h"

#include "fl/warn.h"
#include "fl/assert.h"

namespace fastled_rmt51_strip {

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

#define RMT_ASSERT_MSG(x, msg)         \
    {                                  \
        if (!(x)) {                    \
            ESP_LOGE(TAG, msg);        \
            ESP_ERROR_CHECK(ESP_FAIL); \
        }                              \
    }


// No recycle version of the RMT driver.
class RmtLedStripNoRecycle : public IRmtLedStrip {
public:
    RmtLedStripNoRecycle(uint16_t T0H, uint16_t T0L, uint16_t T1H, uint16_t T1L, uint32_t TRESET,
                int pin, uint32_t max_leds, bool is_rgbw)
        : mPin(pin),
          mIsRgbw(is_rgbw),
          mMaxLeds(max_leds),
          mT0H(T0H),
          mT0L(T0L),
          mT1H(T1H),
          mT1L(T1L),
          mTRESET(TRESET) {
        const uint8_t bytes_per_pixel = is_rgbw ? 4 : 3;
        mBuffer = static_cast<uint8_t*>(calloc(max_leds, bytes_per_pixel));
        // Unlike it's recycling counterpart, we acquire the RMT channel here.
        init();
        FASTLED_WARN("RmtLedStripNoRecycle constructor");
    }

    void init() {
        esp_err_t err = construct_led_strip(
            mT0H, mT0L, mT1H, mT1L, mTRESET,
            mPin, mMaxLeds, mIsRgbw, mBuffer,
            &mLedStrip);

        if (err == ESP_OK) {
            return;
        }

        if (err == ESP_ERR_NOT_FOUND) {  // No free RMT channels yet.
            // Update the total number of active strips allowed.
            mError = true;
            // FASTLED_WARN("All available RMT channels are in use, and no more can be allocated.");
            // ESP_LOGE("All available RMT channels are in use, failed to allocate RMT driver on pin: " << mPin << ".");
            ESP_LOGE(TAG, "All available RMT channels are in use, failed to allocate RMT driver on pin: %d.", mPin);
            return;
        }
        // Some other error that we can't handle.

        ESP_LOGE(TAG, "construct_led_strip failed because of unexpected error, is DMA not supported on this device?: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }

    virtual ~RmtLedStripNoRecycle() override {
        FASTLED_WARN("RmtLedStripNoRecycle destructor");
        mLedStrip->del(mLedStrip, false);
        mLedStrip = nullptr;
        free(mBuffer);
    }

    virtual void set_pixel(uint32_t i, uint8_t r, uint8_t g, uint8_t b) override {
        RMT_ASSERT_LT(i, mMaxLeds);
        RMT_ASSERT(!mIsRgbw);
        uint8_t* pixel = &mBuffer[i * 3];
        pixel[0] = r;
        pixel[1] = g;
        pixel[2] = b;
    }

    virtual void set_pixel_rgbw(uint32_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t w) override {
        RMT_ASSERT_LT(i, mMaxLeds);
        RMT_ASSERT(mIsRgbw);
        uint8_t* pixel = &mBuffer[i * 4];
        pixel[0] = r;
        pixel[1] = g;
        pixel[2] = b;
        pixel[3] = w;
    }

    virtual void draw() override {
        FASTLED_DBG("draw");
        if (mError) {
            FASTLED_WARN("draw called but mError is true");
            return;
        }
        if (mDrawing) {
            wait_for_draw_complete();
        }
        ESP_ERROR_CHECK(led_strip_refresh_async(mLedStrip));
        mDrawing = true;
    }

    virtual void wait_for_draw_complete() override {
        FASTLED_DBG("wait_for_draw_complete");
        if (!mDrawing) {
            FASTLED_WARN("wait_for_draw_complete called but not drawing");
            return;
        }
        if (mError) {
            FASTLED_WARN("wait_for_draw_complete called but mError is true");
            return;
        }
        led_strip_wait_refresh_done(mLedStrip, -1);
        mDrawing = false;
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
    uint16_t mT0H = 0;
    uint16_t mT0L = 0;
    uint16_t mT1H = 0;
    uint16_t mT1L = 0;
    uint32_t mTRESET = 0;
    bool mDrawing = false;
    bool mError = false;
};


IRmtLedStrip* create_rmt_led_strip(
        uint16_t T0H, uint16_t T0L, uint16_t T1H, uint16_t T1L, uint32_t TRESET, // Timing is in nanoseconds
        int pin, uint32_t max_leds, bool is_rgbw) {
    return new RmtLedStripNoRecycle(T0H, T0L, T1H, T1L, TRESET, pin, max_leds, is_rgbw);
}

}  // namespace fastled_rmt51_strip

#endif  // FASTLED_RMT5

#endif  // ESP32
