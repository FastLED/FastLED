
#include "led_strip/enabled.h"

#if FASTLED_RMT5

#include <assert.h>
#include "idf5_rmt.h"
#include "led_strip/led_strip.h"
#include "esp_log.h"
#include "led_strip/configure_led.h"
#include "led_strip/rmt_strip.h"

using namespace fastled_rmt51_strip;

#define TAG "idf5_rmt.cpp"

namespace {  // anonymous namespace

// TODO get rid of this
static const uint32_t TRESET = 280000; // 280us (WS2812-V5)

void convert(int T1, int T2, int T3, uint16_t* T0H, uint16_t* T0L, uint16_t* T1H, uint16_t* T1L) {
    *T0H = T1;
    *T0L = T2 + T3;
    *T1H = T1 + T2;
    *T1L = T3;
}

}  // namespace

RmtController5::RmtController5(int DATA_PIN, int T1, int T2, int T3)
        : mPin(DATA_PIN), mT1(T1), mT2(T2), mT3(T3) {
}

RmtController5::~RmtController5() {
    if (mLedStrip) {
        delete mLedStrip;
    }
}

void RmtController5::waitForDrawComplete() {
    if (mLedStrip) {
        mLedStrip->wait_for_draw_complete();
    }
}

void RmtController5::loadPixelData(PixelIterator &pixels) {
    const bool is_rgbw = pixels.get_rgbw().active();
    if (!mLedStrip) {
        uint16_t t0h, t0l, t1h, t1l;
        convert(mT1, mT2, mT3, &t0h, &t0l, &t1h, &t1l);
        mLedStrip = create_rmt_led_strip(t0h, t0l, t1h, t1l, TRESET, mPin, pixels.size(), is_rgbw);
    } else {
        assert(mLedStrip->num_pixels() == pixels.size());
    }
    mLedStrip->wait_for_draw_complete();
    if (is_rgbw) {
        uint8_t r, g, b, w;
        for (uint16_t i = 0; pixels.has(1); i++) {
            pixels.loadAndScaleRGBW(&r, &g, &b, &w);
            mLedStrip->set_pixel_rgbw(i, r, g, b, w); // Tested to be faster than memcpy of direct bytes.
            pixels.advanceData();
            pixels.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        for (uint16_t i = 0; pixels.has(1); i++) {
            pixels.loadAndScaleRGB(&r, &g, &b);
            mLedStrip->set_pixel(i, r, g, b); // Tested to be faster than memcpy of direct bytes.
            pixels.advanceData();
            pixels.stepDithering();
        }
    }

}

void RmtController5::showPixels() {
    mLedStrip->draw();
}

#endif  // FASTLED_RMT5

