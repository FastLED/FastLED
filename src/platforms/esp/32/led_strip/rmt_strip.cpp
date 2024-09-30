#ifdef ESP32

#include "enabled.h"

#if FASTLED_RMT5

#include "rmt_strip.h"
#include "esp_log.h"
#include "configure_led.h"
#include "construct.h"
#include "esp_check.h"
#include "namespace.h"

LED_STRIP_NAMESPACE_BEGIN

#define MAX_RMT_LED_STRIPS 24

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

class RmtLedStrip;



static int gTotalActiveStripsAllowed = -1;  // start off as unknown.
static RmtLedStrip* gAllRmtLedStrips[MAX_RMT_LED_STRIPS] = {};
static void add_active_strip(RmtLedStrip* strip) {
    for (int i = 0; i < MAX_RMT_LED_STRIPS; i++) {
        if (gAllRmtLedStrips[i] == nullptr) {
            gAllRmtLedStrips[i] = strip;
            return;
        }
    }
    ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
}

static void remove_active_strip(RmtLedStrip* strip) {
    for (int i = 0; i < MAX_RMT_LED_STRIPS; i++) {
        if (gAllRmtLedStrips[i] == strip) {
            gAllRmtLedStrips[i] = nullptr;
            return;
        }
    }
    ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
}

static void wait_for_any_strip_to_complete();

static int count_active_strips() {
    int count = 0;
    for (int i = 0; i < MAX_RMT_LED_STRIPS; i++) {
        if (gAllRmtLedStrips[i]) {
            count++;
        }
    }
    return count;
}

static void wait_if_max_number_of_strips_active() {
    if (gTotalActiveStripsAllowed == -1) {
        // We don't know the limit yet.
        return;
    }
    if (gTotalActiveStripsAllowed == 0) {
        // in invalid number of active strips. In this case we just abort
        // the program.
        ESP_ERROR_CHECK(ESP_FAIL);
    }
    // We've hit the limit before and now the number of known max number
    // active strips is known and that we are saturated. Therefore we block
    // the main thread until a strip is available.
    if (count_active_strips() >= gTotalActiveStripsAllowed) {
        wait_for_any_strip_to_complete();
    }
}

class RmtLedStrip : public IRmtLedStrip {
public:
    RmtLedStrip(uint16_t T0H, uint16_t T0L, uint16_t T1H, uint16_t T1L, uint32_t TRESET,
                int pin, uint32_t max_leds, bool is_rgbw)
        : mT0H(T0H),
          mT0L(T0L),
          mT1H(T1H),
          mT1L(T1L),
          mTRESET(TRESET),
          mIsRgbw(is_rgbw),
          mPin(pin),
          mMaxLeds(max_leds) {
        const uint8_t bytes_per_pixel = is_rgbw ? 4 : 3;
        mBuffer = static_cast<uint8_t*>(calloc(max_leds, bytes_per_pixel));
    }

    void acquire_rmt() {
        assert(!mLedStrip);
        assert(!mAquired);

        wait_if_max_number_of_strips_active();

        do {
            esp_err_t err = construct_led_strip(
                mT0H, mT0L, mT1H, mT1L, mTRESET,
                mPin, mMaxLeds, mIsRgbw, mBuffer,
                &mLedStrip);

            if (err == ESP_OK) {
                // Success
                add_active_strip(this);
                break;
            }
            // ESP_ERROR_CHECK(err);
            // break;

            if (err == ESP_ERR_NOT_FOUND) {  // No free RMT channels yet.
                int active_strips = count_active_strips();
                if (active_strips == 0) {
                    // If there are no active strips and we don't have any resources then
                    // this means RMT is not supported on this platform so we just abort.
                    ESP_ERROR_CHECK(err);
                }
                // Update the total number of active strips allowed. Once this value has been
                // set then it can only decrease. This can happen if the user makes a lot of
                // rmt devices and then deletes them. We could periodically check the number
                // and revalidate the gTotalActiveStripsAllowed value if this becomes a problem.
                // The reason we aren't doing it now is that there is a momentary pause in the
                // main thread when we hit the rmt channel limit and we don't want to introduce
                // that delay until it becomes a problem.
                gTotalActiveStripsAllowed = active_strips;
                // wait for one of the strips to complete and then try again.
                wait_for_any_strip_to_complete();
                continue;
            }
            // Some other error that we can't handle.
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "construct_led_strip failed: %s", esp_err_to_name(err));
                ESP_ERROR_CHECK(err);
                continue;
            }
            break;
        } while (true);

        mAquired = true;
    }

    void release_rmt() {
        if (!mAquired) {
            return;
        }
        led_strip_wait_refresh_done(mLedStrip, -1);
        remove_active_strip(this);
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
    uint16_t mT0H = 0;
    uint16_t mT0L = 0;
    uint16_t mT1H = 0;
    uint16_t mT1L = 0;
    uint32_t mTRESET = 0;
};

static void wait_for_any_strip_to_complete() {
    for (int i = 0; i < MAX_RMT_LED_STRIPS; i++) {
        if (gAllRmtLedStrips[i]) {
            gAllRmtLedStrips[i]->wait_for_draw_complete();
        }
    }
}

IRmtLedStrip* create_rmt_led_strip(uint16_t T0H, uint16_t T0L, uint16_t T1H, uint16_t T1L, uint32_t TRESET, int pin, uint32_t max_leds, bool is_rgbw) {
    return new RmtLedStrip(T0H, T0L, T1H, T1L, TRESET, pin, max_leds, is_rgbw);
}

LED_STRIP_NAMESPACE_END

#endif  // FASTLED_RMT5

#endif  // ESP32
