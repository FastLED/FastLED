#ifdef ESP32

#include "fl/compiler_control.h"

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_pool.h"

FL_EXTERN_C_BEGIN

#include "esp_log.h"

FL_EXTERN_C_END

#include "fl/assert.h"
#include "fl/warn.h"
#include "fl/log.h"

#define RMT5_POOL_TAG "rmt5_worker_pool"

namespace fl {

// Singleton instance
RmtWorkerPool& RmtWorkerPool::getInstance() {
    static RmtWorkerPool instance;
    return instance;
}

RmtWorkerPool::RmtWorkerPool()
    : mInitialized(false)
{
}

RmtWorkerPool::~RmtWorkerPool() {
    // Clean up workers
    for (int i = 0; i < static_cast<int>(mWorkers.size()); i++) {
        delete mWorkers[i];
    }
    mWorkers.clear();
}

void RmtWorkerPool::initializeWorkersIfNeeded() {
    // Set log level based on build type
#ifdef NDEBUG
    // Release build - use INFO level
    esp_log_level_set(RMT5_POOL_TAG, ESP_LOG_INFO);
#else
    // Debug build - use VERBOSE level for detailed tracing
    esp_log_level_set(RMT5_POOL_TAG, ESP_LOG_VERBOSE);
#endif

    if (mInitialized) {
        return;
    }

    int max_workers = getMaxWorkers();

    // Create K workers
    ESP_LOGD(RMT5_POOL_TAG, "Creating %d workers...", max_workers);
    for (int i = 0; i < max_workers; i++) {
        ESP_LOGD(RMT5_POOL_TAG, "Creating worker %d/%d", i + 1, max_workers);
        RmtWorker* worker = new RmtWorker();

        ESP_LOGD(RMT5_POOL_TAG, "Initializing worker %d", i);
        if (!worker->initialize(static_cast<uint8_t>(i))) {
            FL_WARN("Failed to initialize worker " << i << " - skipping");
            delete worker;
            continue;
        }
        FL_LOG_RMT("Worker " << i << " initialized successfully");

        mWorkers.push_back(worker);
    }

    if (mWorkers.size() == 0) {
        FL_WARN("FATAL: No workers initialized successfully!");
    }

    mInitialized = true;
    FL_LOG_RMT("Pool initialized with " << mWorkers.size() << " workers");
}

IRmtWorkerBase* RmtWorkerPool::acquireWorker() {
    // Initialize workers on first use
    ESP_LOGD(RMT5_POOL_TAG, "acquireWorker() called");
    initializeWorkersIfNeeded();

    // Find an available worker
    IRmtWorkerBase* worker = findAvailableWorker();

    if (worker) {
        ESP_LOGD(RMT5_POOL_TAG, "Found available worker %p", worker);
        // Mark worker as unavailable (acquired by caller)
        worker->markAsUnavailable();
        ESP_LOGD(RMT5_POOL_TAG, "Worker marked as unavailable");
        return worker;
    }

    ESP_LOGD(RMT5_POOL_TAG, "No worker available");
    return nullptr;
}

IRmtWorkerBase* RmtWorkerPool::tryAcquireWorker() {
    // Initialize workers on first use
    initializeWorkersIfNeeded();

    // Try to get an available worker (non-blocking, same as acquireWorker for now)
    IRmtWorkerBase* worker = findAvailableWorker();
    if (worker) {
        worker->markAsUnavailable();
    }
    return worker;
}

void RmtWorkerPool::releaseWorker(IRmtWorkerBase* worker) {
    FL_ASSERT(worker != nullptr, "RmtWorkerPool::releaseWorker called with null worker");

    // Mark worker as available
    // Note: ISR already sets mAvailable=true at end of handleDoneInterrupt(),
    // but this redundant write serves as defensive programming to ensure
    // the worker is always available after waitForCompletion() returns.
    worker->markAsAvailable();

    ESP_LOGD(RMT5_POOL_TAG, "Worker released and marked available");
}

int RmtWorkerPool::getAvailableCount() const {
    int count = 0;

    // Count available workers
    for (int i = 0; i < static_cast<int>(mWorkers.size()); i++) {
        if (mWorkers[i]->isAvailable()) {
            count++;
        }
    }

    return count;
}

RmtWorker* RmtWorkerPool::findAvailableWorker() {
    ESP_LOGD(RMT5_POOL_TAG, "Searching %zu workers for available worker",
             mWorkers.size());

    for (int i = 0; i < static_cast<int>(mWorkers.size()); i++) {
        if (mWorkers[i]->isAvailable()) {
            ESP_LOGD(RMT5_POOL_TAG, "Found available worker[%d]", i);
            return mWorkers[i];
        }
    }
    ESP_LOGD(RMT5_POOL_TAG, "No available workers found");
    return nullptr;
}

int RmtWorkerPool::getMaxWorkers() {
    // Platform-specific maximum worker count
    // Workers use 2 memory blocks each, so actual worker count
    // is half the number of RMT TX channels

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-C3/C6/H2: 2 RMT TX channels ÷ 2 = 1 worker
    return 1;
#elif defined(SOC_RMT_TX_CANDIDATES_PER_GROUP)
    // Modern ESP-IDF defines this (ESP32-S3, etc.)
    // Divide by 2 (2 memory blocks per worker)
    return SOC_RMT_TX_CANDIDATES_PER_GROUP / 2;
#elif defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32: 8 RMT TX channels ÷ 2 = 4 workers
    return 4;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S2/S3: 4 RMT TX channels ÷ 2 = 2 workers
    return 2;
#else
    // Conservative default
    #warning "Unknown ESP32 variant - defaulting to 1 RMT worker"
    return 1;
#endif
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
