
#include "led_strip/enabled.h"

#if FASTLED_RMT5

#ifdef FASTLED_RMT_BUILTIN_DRIVER
#warning "FASTLED_RMT_BUILTIN_DRIVER is not supported in RMT5 and will be ignored."
#endif

#include <assert.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "idf5_rmt.h"
#include "led_strip/led_strip.h"
#include "esp_log.h"
#include "led_strip/configure_led.h"
#include "led_strip/rmt_strip.h"

using namespace fastled_rmt51_strip;

#define TAG "idf5_rmt.cpp"

#ifndef FASTLED_RMT5_EXTRA_WAIT_MS
#define FASTLED_RMT5_EXTRA_WAIT_MS 0
#endif

namespace {  // anonymous namespace

// TODO get rid of this
static const uint32_t TRESET = 280000; // 280us (WS2812-V5)

void convert(int T1, int T2, int T3, uint16_t* T0H, uint16_t* T0L, uint16_t* T1H, uint16_t* T1L) {
    *T0H = T1;
    *T0L = T2 + T3;
    *T1H = T1 + T2;
    *T1L = T3;
}


void do_extra_wait() {
    if (FASTLED_RMT5_EXTRA_WAIT_MS > 0) {
        vTaskDelay(FASTLED_RMT5_EXTRA_WAIT_MS / portTICK_PERIOD_MS);
    }
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
    do_extra_wait();
    mLedStrip->draw();
}

#endif  // FASTLED_RMT5

