
#ifdef ESP32
#ifndef FASTLED_ESP32_I2S


#include "led_strip/enabled.h"

#if !FASTLED_RMT5

#define FASTLED_INTERNAL

#include "idf4_rmt.h"
#include "FastLED.h"
#include "idf4_rmt_impl.h"

void RmtController::init(int pin, bool built_in_driver) {
    gpio_num_t _pin = static_cast<gpio_num_t>(pin);
    ESP32RMTController::init(_pin, built_in_driver);
}

RmtController::RmtController(int DATA_PIN, int T1, int T2, int T3,
                             int maxChannel, bool built_in_driver) {
    pImpl = new ESP32RMTController(DATA_PIN, T1, T2, T3, maxChannel,
                                   built_in_driver);
}
RmtController::~RmtController() { delete pImpl; }

void RmtController::showPixels() { pImpl->showPixels(); }

void RmtController::ingest(uint8_t val) { pImpl->ingest(val); }

uint8_t *RmtController::getPixelBuffer(int size_in_bytes) {
    return pImpl->getPixelBuffer(size_in_bytes);
}

bool RmtController::built_in_driver() { return pImpl->mBuiltInDriver; }

void RmtController::initPulseBuffer(int size_in_bytes) {
    pImpl->initPulseBuffer(size_in_bytes);
}

void RmtController::showPixels(PixelIterator &pixels) {
    if (built_in_driver()) {
        loadAllPixelsToRmtSymbolData(pixels);
    } else {
        loadPixelDataForStreamEncoding(pixels);
    }
    showPixels();
}

void RmtController::loadAllPixelsToRmtSymbolData(PixelIterator &pixels) {
    // -- Make sure the data buffer is allocated
    const bool is_rgbw = pixels.get_rgbw().active();
    const int bytes_per_pixel = is_rgbw ? 4 : 3;
    const int size = pixels.size() * bytes_per_pixel;
    initPulseBuffer(size);

    // -- Cycle through the R,G, B (and W) values in the right order,
    //    storing the pulses in the big buffer
    if (!is_rgbw) {
        while (pixels.has(1)) {
            uint8_t r, g, b;
            pixels.loadAndScaleRGB(&r, &g, &b);
            ingest(r);
            ingest(g);
            ingest(b);
            pixels.advanceData();
            pixels.stepDithering();
        }
    } else {
        while (pixels.has(1)) {
            uint8_t r, g, b, w;
            pixels.loadAndScaleRGBW(&r, &g, &b, &w);
            ingest(r);
            ingest(g);
            ingest(b);
            ingest(w);
            pixels.advanceData();
            pixels.stepDithering();
        }
    }
}

void RmtController::loadPixelDataForStreamEncoding(PixelIterator &pixels) {
    // -- Make sure the buffer is allocated
    const bool is_rgbw = pixels.get_rgbw().active();
    const int size_per_pixel = is_rgbw ? 4 : 3;
    const int size_in_bytes = pixels.size() * size_per_pixel;
    uint8_t *pData = getPixelBuffer(size_in_bytes);

    if (!is_rgbw) {
        while (pixels.has(1)) {
            pixels.loadAndScaleRGB(pData, pData + 1, pData + 2);
            pData += 3;
            pixels.advanceData();
            pixels.stepDithering();
        }
    } else {
        while (pixels.has(1)) {
            pixels.loadAndScaleRGBW(pData, pData + 1, pData + 2, pData + 3);
            pData += 4;
            pixels.advanceData();
            pixels.stepDithering();
        }
    }
}

#endif // FASTLED_RMT5

#endif // ! FASTLED_ESP32_I2S

#endif // ESP32
