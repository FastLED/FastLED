#pragma once

#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/stdint.h"
#include "fl/namespace.h"
#include "fl/vector.h"
#include "rmt5_worker_base.h"
#include "rmt5_worker.h"
#include "rmt5_worker_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
}
#endif

namespace fl {

// Hybrid mode configuration
#ifndef FASTLED_ONESHOT_THRESHOLD_LEDS
#define FASTLED_ONESHOT_THRESHOLD_LEDS 200
#endif

/**
 * RmtWorkerPool - Singleton pool manager for RMT workers
 *
 * Architecture:
 * - Manages K workers (where K = hardware channel count)
 * - Supports both double-buffer and one-shot workers
 * - Hybrid mode: automatic selection based on strip size
 * - Supports N > K controllers through worker recycling
 * - Thread-safe worker acquisition/release
 * - Platform-specific worker count (ESP32=8, ESP32-S3=4, ESP32-C3/C6=2)
 *
 * Hybrid Mode (default):
 * - Strip <= 200 LEDs → One-shot worker (zero flicker, higher memory)
 * - Strip > 200 LEDs → Double-buffer worker (low flicker, efficient)
 *
 * Usage:
 *   RmtWorkerPool& pool = RmtWorkerPool::getInstance();
 *   IRmtWorkerBase* worker = pool.acquireWorker(num_bytes, pin, ...);
 *   worker->transmit(pixel_data, num_bytes);
 *   // ... later ...
 *   worker->waitForCompletion();
 *   pool.releaseWorker(worker);
 */
class RmtWorkerPool {
public:
    // Get singleton instance
    static RmtWorkerPool& getInstance();

    // Acquire a worker with hybrid mode selection (blocks if all workers busy and N > K)
    IRmtWorkerBase* acquireWorker(
        int num_bytes,
        gpio_num_t pin,
        int t1, int t2, int t3,
        uint32_t reset_ns
    );

    // Release a worker back to the pool
    void releaseWorker(IRmtWorkerBase* worker);

    // Get total number of workers in pool
    int getWorkerCount() const { return static_cast<int>(mWorkers.size()); }

    // Get number of available workers
    int getAvailableCount() const;

private:
    // Private constructor (singleton pattern)
    RmtWorkerPool();
    ~RmtWorkerPool();

    // Prevent copying
    RmtWorkerPool(const RmtWorkerPool&) = delete;
    RmtWorkerPool& operator=(const RmtWorkerPool&) = delete;

    // Initialize workers on first use
    void initializeWorkersIfNeeded();

    // Find an available worker of specific type (caller must hold spinlock)
    RmtWorker* findAvailableDoubleBufferWorker();
    RmtWorkerOneShot* findAvailableOneShotWorker();

    // Get platform-specific max worker count
    static int getMaxWorkers();

    // Threshold for hybrid mode (in bytes)
    static constexpr size_t ONE_SHOT_THRESHOLD_BYTES = FASTLED_ONESHOT_THRESHOLD_LEDS * 3;

    // Worker storage (separate pools for each type)
    fl::vector<RmtWorker*> mDoubleBufferWorkers;
    fl::vector<RmtWorkerOneShot*> mOneShotWorkers;

    // Legacy interface support
    fl::vector<RmtWorker*> mWorkers;  // Alias for double-buffer workers

    // Initialization flag
    bool mInitialized;

    // Spinlock for thread-safe access
    portMUX_TYPE mSpinlock;

    // Channel accounting (for strict verification)
    int mExpectedChannels;  // Number of channels we expect to create
    int mCreatedChannels;   // Number of channels successfully created
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
