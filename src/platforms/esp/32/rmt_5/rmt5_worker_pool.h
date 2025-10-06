#pragma once

#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/stdint.h"
#include "fl/namespace.h"
#include "fl/vector.h"
#include "rmt5_worker.h"

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

/**
 * RmtWorkerPool - Singleton pool manager for RMT workers
 *
 * Architecture:
 * - Manages K workers (where K = hardware channel count)
 * - Supports N > K controllers through worker recycling
 * - Thread-safe worker acquisition/release
 * - Platform-specific worker count (ESP32=8, ESP32-S3=4, ESP32-C3/C6=2)
 *
 * Usage:
 *   RmtWorkerPool& pool = RmtWorkerPool::getInstance();
 *   RmtWorker* worker = pool.acquireWorker();  // May block if N > K
 *   worker->configure(pin, t1, t2, t3, reset_ns);
 *   worker->transmit(pixel_data, num_bytes);
 *   // ... later ...
 *   worker->waitForCompletion();
 *   pool.releaseWorker(worker);
 */
class RmtWorkerPool {
public:
    // Get singleton instance
    static RmtWorkerPool& getInstance();

    // Acquire a worker (blocks if all workers busy and N > K)
    RmtWorker* acquireWorker();

    // Release a worker back to the pool
    void releaseWorker(RmtWorker* worker);

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

    // Find an available worker (caller must hold spinlock)
    RmtWorker* findAvailableWorker();

    // Get platform-specific max worker count
    static int getMaxWorkers();

    // Worker storage
    fl::vector<RmtWorker*> mWorkers;

    // Initialization flag
    bool mInitialized;

    // Spinlock for thread-safe access
    portMUX_TYPE mSpinlock;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
