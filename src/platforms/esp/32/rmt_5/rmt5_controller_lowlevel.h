#pragma once

#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "pixel_iterator.h"
#include "fl/stdint.h"
#include "fl/namespace.h"
#include "rmt5_worker_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"

#ifdef __cplusplus
}
#endif

namespace fl {

class IRmtWorkerBase;

/**
 * RmtController5LowLevel - Lightweight FastLED controller using worker pool
 *
 * Architecture:
 * - Owns persistent pixel data buffer (not hardware resources)
 * - Borrows workers from RmtWorkerPool during transmission
 * - Integrates with FastLED via onBeforeShow()/onEndShow() hooks
 * - Supports N > K strips through worker pooling
 *
 * Lifecycle:
 * 1. Constructor: Allocate pixel buffer
 * 2. loadPixelData(): Copy pixel data to buffer
 * 3. onBeforeShow(): Wait for previous transmission (called by FastLED.show())
 * 4. onEndShow(): Acquire worker and start transmission (called by FastLED.show())
 * 5. Next frame: Repeat from step 2
 *
 * Memory Model:
 * - Controller owns pixel data buffer (persistent)
 * - Worker uses pointer to controller's buffer (no allocation churn)
 * - Worker is borrowed temporarily, then released
 */
class RmtController5LowLevel {
public:
    // Constructor
    RmtController5LowLevel(
        int DATA_PIN,
        int T1, int T2, int T3,
        int RESET_US = 280  // WS2812 default reset time
    );

    ~RmtController5LowLevel();

    // FastLED interface
    void loadPixelData(PixelIterator& pixels);
    void showPixels();

    // Optional: FastLED synchronization hooks (future use)
    void onBeforeShow();
    void onEndShow();

private:
    // Configuration (not hardware resources!)
    gpio_num_t mPin;
    int mT1, mT2, mT3;
    uint32_t mResetNs;  // Reset time in nanoseconds

    // Pixel data buffer (owned by controller)
    uint8_t* mPixelData;
    int mPixelDataSize;
    int mPixelDataCapacity;

    // Current worker assignment (temporary)
    IRmtWorkerBase* mCurrentWorker;

    // Wait for previous transmission to complete
    void waitForPreviousTransmission();

    // Allocate or resize pixel buffer
    void ensurePixelBufferCapacity(int required_bytes);
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
