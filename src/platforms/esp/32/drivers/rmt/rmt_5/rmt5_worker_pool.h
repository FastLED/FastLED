#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/stdint.h"
#include "fl/vector.h"
#include "rmt5_worker_base.h"
#include "rmt5_worker.h"

namespace fl {

/**
 * RmtWorkerPool - Thin resource pool for RMT workers
 *
 * Architecture:
 * - Manages K workers (where K = hardware channel count)
 * - Simple acquire/release interface - no configuration logic
 * - Worker configuration is handled by ChannelEngineRMT
 * - Platform-specific worker count (ESP32=4, ESP32-S3=2, ESP32-C3/C6=1)
 *
 * Usage:
 *   RmtWorkerPool& pool = RmtWorkerPool::getInstance();
 *   IRmtWorkerBase* worker = pool.acquireWorker();
 *   if (worker) {
 *       worker->configure(pin, timing, reset_ns);
 *       worker->transmit(pixel_data, num_bytes);
 *       worker->waitForCompletion();
 *       pool.releaseWorker(worker);
 *   }
 */
class RmtWorkerPool {
public:
    // Get singleton instance
    static RmtWorkerPool& getInstance();

    // Acquire an available worker (returns nullptr if none available)
    IRmtWorkerBase* acquireWorker();

    // Try to acquire a worker (same as acquireWorker, kept for compatibility)
    IRmtWorkerBase* tryAcquireWorker();

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
    RmtWorker* findAvailableWorker();

    // Get platform-specific max worker count
    static int getMaxWorkers();

    // Worker storage
    fl::vector<RmtWorker*> mWorkers;

    // Initialization flag
    bool mInitialized;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
