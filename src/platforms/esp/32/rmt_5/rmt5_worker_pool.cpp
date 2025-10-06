#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "freertos/task.h"

#ifdef __cplusplus
}
#endif

#include "fl/assert.h"
#include "fl/warn.h"

#define RMT5_POOL_TAG "rmt5_worker_pool"

namespace fl {

// Singleton instance
RmtWorkerPool& RmtWorkerPool::getInstance() {
    static RmtWorkerPool instance;
    return instance;
}

RmtWorkerPool::RmtWorkerPool()
    : mInitialized(false)
    , mSpinlock(portMUX_INITIALIZER_UNLOCKED)
{
}

RmtWorkerPool::~RmtWorkerPool() {
    // Clean up workers
    for (fl::size i = 0; i < mWorkers.size(); i++) {
        delete mWorkers[i];
    }
    mWorkers.clear();
}

void RmtWorkerPool::initializeWorkersIfNeeded() {
    if (mInitialized) {
        return;
    }

    fl::size max_workers = getMaxWorkers();

    FL_WARN("RmtWorkerPool: Initializing %u workers", static_cast<unsigned int>(max_workers));

    for (fl::size i = 0; i < max_workers; i++) {
        RmtWorker* worker = new RmtWorker();
        if (!worker->initialize(static_cast<uint8_t>(i))) {
            FL_WARN("RmtWorkerPool: Failed to initialize worker %u", static_cast<unsigned int>(i));
            delete worker;
            continue;
        }
        mWorkers.push_back(worker);
    }

    if (mWorkers.size() == 0) {
        FL_WARN("RmtWorkerPool: No workers initialized successfully!");
    } else {
        FL_WARN("RmtWorkerPool: Successfully initialized %u/%u workers",
            static_cast<unsigned int>(mWorkers.size()),
            static_cast<unsigned int>(max_workers));
    }

    mInitialized = true;
}

RmtWorker* RmtWorkerPool::acquireWorker() {
    // Initialize workers on first use
    initializeWorkersIfNeeded();

    // Try to get an available worker immediately
    portENTER_CRITICAL(&mSpinlock);
    RmtWorker* worker = findAvailableWorker();
    portEXIT_CRITICAL(&mSpinlock);

    if (worker) {
        return worker;
    }

    // No workers available - poll until one frees up
    // This implements the N > K blocking behavior
    uint32_t poll_count = 0;
    while (true) {
        // Short delay before retry
        delayMicroseconds(100);

        portENTER_CRITICAL(&mSpinlock);
        worker = findAvailableWorker();
        portEXIT_CRITICAL(&mSpinlock);

        if (worker) {
            return worker;
        }

        // Yield to FreeRTOS periodically to prevent watchdog
        poll_count++;
        if (poll_count % 50 == 0) {  // Every 5ms
            taskYIELD();
        }

        // Safety: Warn if waiting too long (more than 100ms = very unusual)
        if (poll_count % 1000 == 0) {  // Every 100ms
            FL_WARN("RmtWorkerPool: Still waiting for available worker after %u ms",
                static_cast<unsigned int>(poll_count / 10));
        }
    }
}

void RmtWorkerPool::releaseWorker(RmtWorker* worker) {
    FL_ASSERT(worker != nullptr, "RmtWorkerPool::releaseWorker called with null worker");

    // Worker marks itself as available after transmission completes
    // Nothing to do here - worker is automatically recycled
}

fl::size RmtWorkerPool::getAvailableCount() const {
    portENTER_CRITICAL(&mSpinlock);
    fl::size count = 0;
    for (fl::size i = 0; i < mWorkers.size(); i++) {
        if (mWorkers[i]->isAvailable()) {
            count++;
        }
    }
    portEXIT_CRITICAL(&mSpinlock);
    return count;
}

RmtWorker* RmtWorkerPool::findAvailableWorker() {
    // Caller must hold mSpinlock
    for (fl::size i = 0; i < mWorkers.size(); i++) {
        if (mWorkers[i]->isAvailable()) {
            return mWorkers[i];
        }
    }
    return nullptr;
}

fl::size RmtWorkerPool::getMaxWorkers() {
    // Platform-specific maximum worker count
    // Based on SOC RMT TX channel capabilities

#if defined(SOC_RMT_TX_CANDIDATES_PER_GROUP)
    // Modern ESP-IDF defines this (ESP32-S3, ESP32-C3, ESP32-C6, etc.)
    return SOC_RMT_TX_CANDIDATES_PER_GROUP;
#elif defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32 has 8 RMT TX channels
    return 8;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S2/S3 have 4 RMT TX channels
    return 4;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-C3/C6/H2 have 2 RMT TX channels
    return 2;
#else
    // Conservative default
    #warning "Unknown ESP32 variant - defaulting to 2 RMT TX channels"
    return 2;
#endif
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
