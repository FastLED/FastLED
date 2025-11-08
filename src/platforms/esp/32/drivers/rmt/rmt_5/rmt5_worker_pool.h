#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/stdint.h"
#include "fl/vector.h"
#include "fl/chipsets/led_timing.h"
#include "rmt5_worker_base.h"
#include "rmt5_worker.h"

FL_EXTERN_C_BEGIN

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "soc/soc_caps.h"

FL_EXTERN_C_END

namespace fl {

/**
 * RmtWorkerPool - Singleton pool manager for RMT workers
 *
 * Architecture:
 * - Manages K workers (where K = hardware channel count)
 * - Double-buffer workers for efficient memory usage
 * - Supports N > K controllers through worker recycling
 * - Platform-specific worker count (ESP32=8, ESP32-S3=4, ESP32-C3/C6=2)
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

    // Acquire a double-buffer worker (blocks if all workers busy and N > K)
    IRmtWorkerBase* acquireWorker(
        int num_bytes,
        gpio_num_t pin,
        const ChipsetTiming& TIMING,
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

    // Find an available worker
    RmtWorker* findAvailableDoubleBufferWorker();

    // Get platform-specific max worker count
    static int getMaxWorkers();


    // Worker storage
    fl::vector<RmtWorker*> mDoubleBufferWorkers;

    // Legacy interface support
    fl::vector<RmtWorker*> mWorkers;  // Alias for double-buffer workers

    // Initialization flag
    bool mInitialized;

    // Channel accounting (for strict verification)
    int mExpectedChannels;  // Number of channels we expect to create
    int mCreatedChannels;   // Number of channels successfully created
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
