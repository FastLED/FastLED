#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#ifdef FASTLED_RMT_BUILTIN_DRIVER
#warning "FASTLED_RMT_BUILTIN_DRIVER is not supported in RMT5 and will be ignored."
#endif

#include "idf5_rmt.h"
#include "rmt_worker_pool.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fl/assert.h"
#include "fl/convert.h"  // for convert_fastled_timings_to_timedeltas(...)
#include "fl/namespace.h"
#include "strip_rmt.h"
#include "platforms/esp/32/esp_log_control.h"
#include "esp_log.h"


#define IDF5_RMT_TAG "idf5_rmt.cpp"

// Worker pool configuration
// Enable worker pool by default - can be disabled via compile flag
#ifndef FASTLED_RMT5_DISABLE_WORKER_POOL
    #define FASTLED_RMT5_USE_WORKER_POOL 1
#else
    #define FASTLED_RMT5_USE_WORKER_POOL 0
#endif

// Runtime worker pool control (can be overridden in constructor)
#ifndef FASTLED_RMT5_FORCE_LEGACY_MODE
    #define FASTLED_RMT5_FORCE_LEGACY_MODE 0
#endif

namespace fl {

RmtController5::RmtController5(int DATA_PIN, int T1, int T2, int T3, RmtController5::DmaMode dma_mode)
        : mPin(DATA_PIN), mT1(T1), mT2(T2), mT3(T3), mDmaMode(dma_mode)
        , mWorkerConfig(nullptr)
        , mRegisteredWithPool(false)
        , mUseWorkerPool(FASTLED_RMT5_USE_WORKER_POOL && !FASTLED_RMT5_FORCE_LEGACY_MODE) {
    
    if (mUseWorkerPool) {
        // Register with worker pool
        RmtWorkerPool::getInstance().registerController(this);
        mRegisteredWithPool = true;
        FASTLED_ESP_LOGD(IDF5_RMT_TAG, "RmtController5 registered with worker pool (pin %d)", mPin);
    } else {
        FASTLED_ESP_LOGD(IDF5_RMT_TAG, "RmtController5 using legacy mode (pin %d)", mPin);
    }
}

RmtController5::~RmtController5() {
    if (mRegisteredWithPool) {
        RmtWorkerPool::getInstance().unregisterController(this);
    }
    
    if (mLedStrip) {
        delete mLedStrip;
    }
    
    if (mWorkerConfig) {
        delete mWorkerConfig;
    }
}

IRmtStrip::DmaMode RmtController5::convertDmaMode(DmaMode dma_mode) {
    switch (dma_mode) {
        case DMA_AUTO:
            return IRmtStrip::DMA_AUTO;
        case DMA_ENABLED:
            return IRmtStrip::DMA_ENABLED;
        case DMA_DISABLED:
            return IRmtStrip::DMA_DISABLED;
        default:
            FL_ASSERT(false, "Invalid DMA mode");
            return IRmtStrip::DMA_AUTO;
    }
}

void RmtController5::loadPixelData(PixelIterator &pixels) {
    if (mUseWorkerPool) {
        // Worker pool mode - store pixel data in persistent buffer
        storePixelData(pixels);
    } else {
        // Legacy mode - use direct RMT strip
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
                mLedStrip->setPixelRGBW(i, r, g, b, w);
                pixels.advanceData();
                pixels.stepDithering();
            }
        } else {
            uint8_t r, g, b;
            for (uint16_t i = 0; pixels.has(1); i++) {
                pixels.loadAndScaleRGB(&r, &g, &b);
                mLedStrip->setPixel(i, r, g, b);
                pixels.advanceData();
                pixels.stepDithering();
            }
        }
    }
}

void RmtController5::showPixels() {
    if (mUseWorkerPool) {
        // Worker pool mode - execute coordinated draw cycle
        executeWithWorkerPool();
    } else {
        // Legacy mode - direct async draw
        FL_ASSERT(mLedStrip != nullptr, "RMT strip not initialized");
        mLedStrip->drawAsync();
    }
}

void RmtController5::storePixelData(PixelIterator& pixels) {
    const bool is_rgbw = pixels.get_rgbw().active();
    const int bytesPerPixel = is_rgbw ? 4 : 3;
    const int bufferSize = pixels.size() * bytesPerPixel;
    
    // Resize buffer if needed
    mPixelBuffer.resize(bufferSize);
    
    // Update worker config with current pixel data info
    initializeWorkerConfig();
    mWorkerConfig->ledCount = pixels.size();
    mWorkerConfig->isRgbw = is_rgbw;
    
    // Copy pixel data to persistent buffer
    uint8_t* bufPtr = mPixelBuffer.data();
    if (is_rgbw) {
        while (pixels.has(1)) {
            uint8_t r, g, b, w;
            pixels.loadAndScaleRGBW(&r, &g, &b, &w);
            *bufPtr++ = r; 
            *bufPtr++ = g; 
            *bufPtr++ = b; 
            *bufPtr++ = w;
            pixels.advanceData();
            pixels.stepDithering();
        }
    } else {
        while (pixels.has(1)) {
            uint8_t r, g, b;
            pixels.loadAndScaleRGB(&r, &g, &b);
            *bufPtr++ = r; 
            *bufPtr++ = g; 
            *bufPtr++ = b;
            pixels.advanceData();
            pixels.stepDithering();
        }
    }
}

void RmtController5::executeWithWorkerPool() {
    RmtWorkerPool& pool = RmtWorkerPool::getInstance();
    
    if (pool.canStartImmediately(this)) {
        // Async path - return immediately
        pool.startControllerImmediate(this);
    } else {
        // This controller is queued - must wait for worker
        pool.startControllerQueued(this);
    }
}

void RmtController5::initializeWorkerConfig() const {
    if (!mWorkerConfig) {
        mWorkerConfig = new RmtWorkerConfig();
        
        // Convert FastLED timings to time deltas
        uint16_t t0h, t0l, t1h, t1l;
        convert_fastled_timings_to_timedeltas(mT1, mT2, mT3, &t0h, &t0l, &t1h, &t1l);
        
        // Initialize configuration
        mWorkerConfig->pin = mPin;
        mWorkerConfig->ledCount = 0;  // Will be set in storePixelData
        mWorkerConfig->isRgbw = false;  // Will be set in storePixelData
        mWorkerConfig->t0h = t0h;
        mWorkerConfig->t0l = t0l;
        mWorkerConfig->t1h = t1h;
        mWorkerConfig->t1l = t1l;
        mWorkerConfig->reset = 280;
        mWorkerConfig->dmaMode = convertDmaMode(mDmaMode);
        mWorkerConfig->interruptPriority = 3;
    }
}

const RmtWorkerConfig& RmtController5::getWorkerConfig() const {
    initializeWorkerConfig();
    return *mWorkerConfig;
}

const uint8_t* RmtController5::getPixelBuffer() const {
    return mPixelBuffer.data();
}

size_t RmtController5::getBufferSize() const {
    return mPixelBuffer.size();
}

} // namespace fl

#endif  // FASTLED_RMT5

#endif  // ESP32
