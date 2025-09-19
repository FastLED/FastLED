#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#ifdef FASTLED_RMT_BUILTIN_DRIVER
#warning "FASTLED_RMT_BUILTIN_DRIVER is not supported in RMT5 and will be ignored."
#endif

#include "idf5_rmt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fl/assert.h"
#include "fl/convert.h"  // for convert_fastled_timings_to_timedeltas(...)
#include "fl/namespace.h"
#include "strip_rmt.h"


#define IDF5_RMT_TAG "idf5_rmt.cpp"

namespace fl {


RmtController5::RmtController5(int DATA_PIN, int T1, int T2, int T3, RmtController5::DmaMode dma_mode)
        : mPin(DATA_PIN), mT1(T1), mT2(T2), mT3(T3), mDmaMode(dma_mode) {
}

RmtController5::~RmtController5() {
    if (mLedStrip) {
        delete mLedStrip;
    }
}

static IRmtStrip::DmaMode convertDmaMode(RmtController5::DmaMode dma_mode) {
    switch (dma_mode) {
        case RmtController5::DMA_AUTO:
            return IRmtStrip::DMA_AUTO;
        case RmtController5::DMA_ENABLED:
            return IRmtStrip::DMA_ENABLED;
        case RmtController5::DMA_DISABLED:
            return IRmtStrip::DMA_DISABLED;
        default:
            FL_ASSERT(false, "Invalid DMA mode");
            return IRmtStrip::DMA_AUTO;
    }
}

void RmtController5::loadPixelData(PixelIterator &pixels) {
    const bool is_rgbw = pixels.get_rgbw().active();
    if (!mLedStrip) {
        uint16_t t0h, t0l, t1h, t1l;
        convert_fastled_timings_to_timedeltas(mT1, mT2, mT3, &t0h, &t0l, &t1h, &t1l);
        mLedStrip = IRmtStrip::Create(
            mPin, pixels.size(),
            is_rgbw, t0h, t0l, t1h, t1l, 280,
            convertDmaMode(mDmaMode));
        
    } else {
        FASTLED_ASSERT(
            mLedStrip->numPixels() == pixels.size(),
            "mLedStrip->numPixels() (" << mLedStrip->numPixels() << ") != pixels.size() (" << pixels.size() << ")");
    }
    if (is_rgbw) {
        uint8_t r, g, b, w;
        for (uint16_t i = 0; pixels.has(1); i++) {
            pixels.loadAndScaleRGBW(&r, &g, &b, &w);
            mLedStrip->setPixelRGBW(i, r, g, b, w); // Tested to be faster than memcpy of direct bytes.
            pixels.advanceData();
            pixels.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        for (uint16_t i = 0; pixels.has(1); i++) {
            pixels.loadAndScaleRGB(&r, &g, &b);
            mLedStrip->setPixel(i, r, g, b); // Tested to be faster than memcpy of direct bytes.
            pixels.advanceData();
            pixels.stepDithering();
        }
    }

}

void RmtController5::showPixels() {
    mLedStrip->drawAsync();
}

} // namespace fl

#endif  // FASTLED_RMT5

#endif  // ESP32
