#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_controller_lowlevel.h"
#include "rmt5_worker_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
}
#endif

#include "fl/assert.h"
#include "fl/warn.h"

#define RMT5_CONTROLLER_TAG "rmt5_controller_lowlevel"

namespace fl {

RmtController5LowLevel::RmtController5LowLevel(
    int DATA_PIN,
    int T1, int T2, int T3,
    int RESET_US
)
    : mPin(static_cast<gpio_num_t>(DATA_PIN))
    , mT1(T1)
    , mT2(T2)
    , mT3(T3)
    , mResetNs(RESET_US * 1000)  // Convert microseconds to nanoseconds
    , mPixelData(nullptr)
    , mPixelDataSize(0)
    , mPixelDataCapacity(0)
    , mCurrentWorker(nullptr)
{
}

RmtController5LowLevel::~RmtController5LowLevel() {
    // Wait for any pending transmission
    waitForPreviousTransmission();

    // Free pixel buffer
    if (mPixelData) {
        free(mPixelData);
        mPixelData = nullptr;
    }
}

void RmtController5LowLevel::loadPixelData(PixelIterator& pixels) {
    // Wait for previous transmission to complete before overwriting buffer
    waitForPreviousTransmission();

    // Determine if RGBW or RGB
    const bool is_rgbw = pixels.get_rgbw().active();
    const fl::size bytes_per_pixel = is_rgbw ? 4 : 3;
    const fl::size num_pixels = pixels.size();
    const fl::size required_bytes = num_pixels * bytes_per_pixel;

    // Ensure buffer capacity
    ensurePixelBufferCapacity(required_bytes);

    // Copy pixel data to buffer
    if (is_rgbw) {
        uint8_t r, g, b, w;
        int offset = 0;
        for (int i = 0; pixels.has(1); i++) {
            pixels.loadAndScaleRGBW(&r, &g, &b, &w);
            mPixelData[offset++] = r;
            mPixelData[offset++] = g;
            mPixelData[offset++] = b;
            mPixelData[offset++] = w;
            pixels.advanceData();
            pixels.stepDithering();
        }
        mPixelDataSize = offset;
    } else {
        uint8_t r, g, b;
        int offset = 0;
        for (int i = 0; pixels.has(1); i++) {
            pixels.loadAndScaleRGB(&r, &g, &b);
            mPixelData[offset++] = r;
            mPixelData[offset++] = g;
            mPixelData[offset++] = b;
            pixels.advanceData();
            pixels.stepDithering();
        }
        mPixelDataSize = offset;
    }
}

void RmtController5LowLevel::showPixels() {
    // This method starts the transmission
    // It's called by FastLED after loadPixelData()
    onEndShow();
}

void RmtController5LowLevel::onBeforeShow() {
    // Wait for previous transmission to complete
    // This is called by FastLED.show() before loading new pixel data
    waitForPreviousTransmission();
}

void RmtController5LowLevel::onEndShow() {
    // Acquire worker with hybrid mode selection (may block if N > K and all workers busy)
    // Worker is pre-configured based on strip size and timing parameters
    IRmtWorkerBase* worker = RmtWorkerPool::getInstance().acquireWorker(
        mPixelDataSize,
        mPin,
        mT1, mT2, mT3,
        mResetNs
    );

    if (!worker) {
        ESP_LOGW(RMT5_CONTROLLER_TAG, "Failed to acquire worker for pin %d", mPin);
        return;
    }

    // Start transmission (async - returns immediately after transmission STARTS)
    worker->transmit(mPixelData, mPixelDataSize);

    // Store worker reference for completion waiting
    mCurrentWorker = worker;
}

void RmtController5LowLevel::waitForPreviousTransmission() {
    if (mCurrentWorker) {
        // Wait for transmission to complete
        mCurrentWorker->waitForCompletion();

        // Release worker back to pool
        RmtWorkerPool::getInstance().releaseWorker(mCurrentWorker);
        mCurrentWorker = nullptr;
    }
}

void RmtController5LowLevel::ensurePixelBufferCapacity(int required_bytes) {
    if (required_bytes <= mPixelDataCapacity) {
        return;  // Sufficient capacity
    }

    // Allocate new buffer with extra headroom to reduce reallocations
    int new_capacity = required_bytes + (required_bytes / 4);  // 25% headroom

    uint8_t* new_buffer = static_cast<uint8_t*>(malloc(new_capacity));
    if (!new_buffer) {
        ESP_LOGE(RMT5_CONTROLLER_TAG, "Failed to allocate pixel buffer (%u bytes)",
            static_cast<unsigned int>(new_capacity));
        return;
    }

    // Copy existing data if any
    if (mPixelData && mPixelDataSize > 0) {
        memcpy(new_buffer, mPixelData, mPixelDataSize);
    }

    // Free old buffer
    if (mPixelData) {
        free(mPixelData);
    }

    // Update buffer
    mPixelData = new_buffer;
    mPixelDataCapacity = new_capacity;
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
