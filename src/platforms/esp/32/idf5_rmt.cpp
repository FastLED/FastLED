#include "led_strip/enabled.h"

#if FASTLED_RMT51

#warning "Work in progress: ESP32 ClocklessController for IDF 5.1 - this is still a prototype"

#include <assert.h>
#include "idf5_rmt.h"
#include "led_strip/led_strip.h"
#include "esp_log.h"
#include "led_strip/configure_led.h"
#include "led_strip/rmt_strip.h"

USING_NAMESPACE_LED_STRIP

#define TAG "idf5_rmt.cpp"

// WS2812 timings.
const uint16_t T0H = 300; // 0.3us
const uint16_t T0L = 900; // 0.9us
const uint16_t T1H = 600; // 0.6us
const uint16_t T1L = 600; // 0.6us
const uint32_t TRESET = 280000; // 280us (WS2812-V5)


RmtController5::RmtController5(int DATA_PIN, int T1, int T2, int T3) {
    // Stub implementation
    ESP_LOGI(TAG, "RmtController5 constructor called");
    mPin = DATA_PIN;
}

RmtController5::~RmtController5() {
    // Stub implementation
    ESP_LOGI(TAG, "RmtController5 destructor called");
    if (mLedStrip) {
        delete mLedStrip;
    }
}

void RmtController5::showPixels(PixelIterator &pixels) {
    ESP_LOGI(TAG, "showPixels called");
    const bool is_rgbw = pixels.get_rgbw().active();
    if (!mLedStrip) {
        mLedStrip = create_rmt_led_strip(T0H, T0L, T1H, T1L, TRESET, mPin, pixels.size(), is_rgbw);
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
    mLedStrip->draw();
}

#endif  // FASTLED_RMT51

