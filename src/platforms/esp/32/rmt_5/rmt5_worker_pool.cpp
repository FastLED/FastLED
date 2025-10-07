#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_pool.h"

#include <Arduino.h>  // ok include - needed for delayMicroseconds()

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "freertos/task.h"
#include "esp_debug_helpers.h"  // For esp_backtrace_print()

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
    , mExpectedChannels(0)
    , mCreatedChannels(0)
{
}

RmtWorkerPool::~RmtWorkerPool() {
    // Clean up double-buffer workers
    for (int i = 0; i < static_cast<int>(mDoubleBufferWorkers.size()); i++) {
        delete mDoubleBufferWorkers[i];
    }
    mDoubleBufferWorkers.clear();

    // Clean up one-shot workers
    for (int i = 0; i < static_cast<int>(mOneShotWorkers.size()); i++) {
        delete mOneShotWorkers[i];
    }
    mOneShotWorkers.clear();

    mWorkers.clear();
}

void RmtWorkerPool::initializeWorkersIfNeeded() {
    // Set log level to verbose for maximum debugging
    esp_log_level_set(RMT5_POOL_TAG, ESP_LOG_VERBOSE);

    ESP_LOGE(RMT5_POOL_TAG, "=== initializeWorkersIfNeeded() ENTRY ===");
    ESP_LOGE(RMT5_POOL_TAG, "mInitialized=%d", mInitialized);

    if (mInitialized) {
        ESP_LOGE(RMT5_POOL_TAG, "Already initialized - returning early");
        return;
    }

    int max_workers = getMaxWorkers();
    mExpectedChannels = max_workers;
    mCreatedChannels = 0;

    ESP_LOGE(RMT5_POOL_TAG, "");
    ESP_LOGE(RMT5_POOL_TAG, "========================================");
    ESP_LOGE(RMT5_POOL_TAG, "RMT WORKER POOL INITIALIZATION");
    ESP_LOGE(RMT5_POOL_TAG, "========================================");
    ESP_LOGE(RMT5_POOL_TAG, "Max workers: %d", max_workers);
    ESP_LOGE(RMT5_POOL_TAG, "Hybrid mode threshold: %d LEDs", FASTLED_ONESHOT_THRESHOLD_LEDS);
    ESP_LOGE(RMT5_POOL_TAG, "STRICT MODE: Will abort if all %d channels cannot be created", max_workers);
    ESP_LOGE(RMT5_POOL_TAG, "========================================");
    ESP_LOGE(RMT5_POOL_TAG, "");

    // Create K double-buffer workers (current default)
    // Note: We only create one type because we only have K hardware channels
    // Future: could dynamically choose worker type based on usage patterns
    ESP_LOGE(RMT5_POOL_TAG, "Starting worker creation loop...");
    for (int i = 0; i < max_workers; i++) {
        ESP_LOGE(RMT5_POOL_TAG, "");
        ESP_LOGE(RMT5_POOL_TAG, "--- Creating worker %d/%d ---", i + 1, max_workers);
        ESP_LOGE(RMT5_POOL_TAG, "Allocating RmtWorker object...");
        RmtWorker* worker = new RmtWorker();
        ESP_LOGE(RMT5_POOL_TAG, "Worker object allocated at %p", worker);

        ESP_LOGE(RMT5_POOL_TAG, "Calling worker->initialize(%d)...", i);
        if (!worker->initialize(static_cast<uint8_t>(i))) {
            ESP_LOGE(RMT5_POOL_TAG, "FAILED to initialize double-buffer worker %d - skipping", i);
            delete worker;
            continue;
        }
        ESP_LOGE(RMT5_POOL_TAG, "Worker %d initialized successfully", i);

        ESP_LOGE(RMT5_POOL_TAG, "Adding worker %d to pool vectors...", i);
        mDoubleBufferWorkers.push_back(worker);
        mWorkers.push_back(worker);  // Legacy support
        ESP_LOGE(RMT5_POOL_TAG, "Worker %d added to pool (total workers: %zu)", i, mDoubleBufferWorkers.size());
    }

    ESP_LOGE(RMT5_POOL_TAG, "");
    ESP_LOGE(RMT5_POOL_TAG, "========================================");
    ESP_LOGE(RMT5_POOL_TAG, "Worker creation complete");
    ESP_LOGE(RMT5_POOL_TAG, "Double-buffer workers: %zu", mDoubleBufferWorkers.size());
    ESP_LOGE(RMT5_POOL_TAG, "One-shot workers: %zu", mOneShotWorkers.size());
    ESP_LOGE(RMT5_POOL_TAG, "========================================");
    ESP_LOGE(RMT5_POOL_TAG, "");

    if (mDoubleBufferWorkers.size() == 0 && mOneShotWorkers.size() == 0) {
        ESP_LOGE(RMT5_POOL_TAG, "FATAL: No workers initialized successfully!");
    }

    mInitialized = true;
    ESP_LOGE(RMT5_POOL_TAG, "=== initializeWorkersIfNeeded() EXIT - mInitialized=%d ===", mInitialized);
}

IRmtWorkerBase* RmtWorkerPool::acquireWorker(
    int num_bytes,
    gpio_num_t pin,
    int t1, int t2, int t3,
    uint32_t reset_ns
) {
    ESP_LOGE(RMT5_POOL_TAG, "");
    ESP_LOGE(RMT5_POOL_TAG, "========================================");
    ESP_LOGE(RMT5_POOL_TAG, "=== acquireWorker() ENTRY ===");
    ESP_LOGE(RMT5_POOL_TAG, "========================================");
    ESP_LOGE(RMT5_POOL_TAG, "num_bytes=%d (%d LEDs)", num_bytes, num_bytes / 3);
    ESP_LOGE(RMT5_POOL_TAG, "pin=%d", (int)pin);
    ESP_LOGE(RMT5_POOL_TAG, "t1=%d, t2=%d, t3=%d", t1, t2, t3);
    ESP_LOGE(RMT5_POOL_TAG, "reset_ns=%lu", reset_ns);
    ESP_LOGE(RMT5_POOL_TAG, "");

    // Initialize workers on first use
    ESP_LOGE(RMT5_POOL_TAG, "Calling initializeWorkersIfNeeded()...");
    initializeWorkersIfNeeded();
    ESP_LOGE(RMT5_POOL_TAG, "initializeWorkersIfNeeded() returned");

    // Determine which worker type to use based on strip size
    // For now, always use double-buffer (hybrid mode for future expansion)
    bool use_oneshot = (static_cast<size_t>(num_bytes) <= ONE_SHOT_THRESHOLD_BYTES);

    ESP_LOGE(RMT5_POOL_TAG, "Worker selection:");
    ESP_LOGE(RMT5_POOL_TAG, "  use_oneshot=%d", use_oneshot);
    ESP_LOGE(RMT5_POOL_TAG, "  threshold_bytes=%zu", ONE_SHOT_THRESHOLD_BYTES);
    ESP_LOGE(RMT5_POOL_TAG, "  num_bytes=%d", num_bytes);

    // Try to get an available worker immediately
    ESP_LOGE(RMT5_POOL_TAG, "Entering critical section to search for available worker...");
    portENTER_CRITICAL(&mSpinlock);
    IRmtWorkerBase* worker = nullptr;

    ESP_LOGE(RMT5_POOL_TAG, "Available double-buffer workers: %zu", mDoubleBufferWorkers.size());
    ESP_LOGE(RMT5_POOL_TAG, "Available one-shot workers: %zu", mOneShotWorkers.size());

    if (use_oneshot && mOneShotWorkers.size() > 0) {
        ESP_LOGE(RMT5_POOL_TAG, "Searching for available one-shot worker...");
        worker = findAvailableOneShotWorker();
        if (worker) {
            ESP_LOGE(RMT5_POOL_TAG, "Found ONE-SHOT worker for %d bytes (%d LEDs)",
                     num_bytes, num_bytes / 3);
        } else {
            ESP_LOGE(RMT5_POOL_TAG, "No one-shot worker available");
        }
    }

    if (!worker) {
        ESP_LOGE(RMT5_POOL_TAG, "Searching for available double-buffer worker...");
        worker = findAvailableDoubleBufferWorker();
        if (worker) {
            if (use_oneshot) {
                ESP_LOGE(RMT5_POOL_TAG, "Using DOUBLE-BUFFER worker for %d bytes (no one-shot available)", num_bytes);
            } else {
                ESP_LOGE(RMT5_POOL_TAG, "Found DOUBLE-BUFFER worker for %d bytes", num_bytes);
            }
        } else {
            ESP_LOGE(RMT5_POOL_TAG, "No double-buffer worker available");
        }
    }

    portEXIT_CRITICAL(&mSpinlock);
    ESP_LOGE(RMT5_POOL_TAG, "Exited critical section - worker=%p", worker);

    if (worker) {
        ESP_LOGE(RMT5_POOL_TAG, "Worker found! Configuring worker...");

        // Check if this worker already has a channel (to track actual channel creation)
        bool had_channel = worker->hasChannel();
        ESP_LOGE(RMT5_POOL_TAG, "Worker already has channel: %d", had_channel);

        // Configure the worker before returning
        ESP_LOGE(RMT5_POOL_TAG, "Calling worker->configure(pin=%d, t1=%d, t2=%d, t3=%d, reset_ns=%lu)...",
                 (int)pin, t1, t2, t3, reset_ns);
        if (!worker->configure(pin, t1, t2, t3, reset_ns)) {
            ESP_LOGE(RMT5_POOL_TAG, "worker->configure() FAILED!");

            // Configuration failed (likely channel creation failed due to exhaustion)
            // STRICT MODE: Abort immediately with stack trace
            ESP_LOGE(RMT5_POOL_TAG, "");
            ESP_LOGE(RMT5_POOL_TAG, "========================================");
            ESP_LOGE(RMT5_POOL_TAG, "FATAL: RMT CHANNEL EXHAUSTION");
            ESP_LOGE(RMT5_POOL_TAG, "========================================");
            ESP_LOGE(RMT5_POOL_TAG, "Expected:  %d RMT channels", mExpectedChannels);
            ESP_LOGE(RMT5_POOL_TAG, "Created:   %d channels", mCreatedChannels);
            ESP_LOGE(RMT5_POOL_TAG, "Failed on: GPIO %d (first acquisition)", (int)pin);
            ESP_LOGE(RMT5_POOL_TAG, "========================================");
            ESP_LOGE(RMT5_POOL_TAG, "");

            // Stack trace from abort() will be printed by panic handler
            abort();
        } else {
            ESP_LOGE(RMT5_POOL_TAG, "worker->configure() SUCCESS!");

            // Successfully configured - track channel creation only if this is a NEW channel
            if (!had_channel) {
                mCreatedChannels++;
                ESP_LOGE(RMT5_POOL_TAG, "NEW channel created: %d/%d for GPIO %d",
                         mCreatedChannels, mExpectedChannels, (int)pin);
            } else {
                ESP_LOGE(RMT5_POOL_TAG, "Existing channel reconfigured for GPIO %d", (int)pin);
            }

            ESP_LOGE(RMT5_POOL_TAG, "=== acquireWorker() SUCCESS - returning worker %p ===", worker);
            return worker;
        }
    }

    // No workers available (or configuration failed) - poll until one frees up
    // This implements the N > K blocking behavior
    ESP_LOGE(RMT5_POOL_TAG, "No worker immediately available - entering polling loop...");
    uint32_t poll_count = 0;
    uint32_t config_fail_count = 0;  // Track consecutive configuration failures
    const uint32_t MAX_CONFIG_RETRIES = 10;  // Limit retries before aborting

    while (true) {
        poll_count++;
        // Short delay before retry
        delayMicroseconds(100);

        if (poll_count % 50 == 0) {
            ESP_LOGE(RMT5_POOL_TAG, "Polling iteration %u - searching for available worker...", poll_count);
        }

        portENTER_CRITICAL(&mSpinlock);

        if (use_oneshot && mOneShotWorkers.size() > 0) {
            worker = findAvailableOneShotWorker();
        }

        if (!worker) {
            worker = findAvailableDoubleBufferWorker();
        }

        portEXIT_CRITICAL(&mSpinlock);

        if (worker) {
            ESP_LOGE(RMT5_POOL_TAG, "Worker found in polling loop (iteration %u)", poll_count);

            // Check if this worker already has a channel (to track actual channel creation)
            bool had_channel = worker->hasChannel();
            ESP_LOGE(RMT5_POOL_TAG, "Worker already has channel: %d", had_channel);

            ESP_LOGE(RMT5_POOL_TAG, "Configuring worker in retry path...");
            if (worker->configure(pin, t1, t2, t3, reset_ns)) {
                ESP_LOGE(RMT5_POOL_TAG, "worker->configure() SUCCESS in retry path!");

                // Success - track channel creation only if this is a NEW channel
                if (!had_channel) {
                    mCreatedChannels++;
                    ESP_LOGE(RMT5_POOL_TAG, "NEW channel created: %d/%d for GPIO %d (retry path, iteration %u)",
                             mCreatedChannels, mExpectedChannels, (int)pin, poll_count);
                } else {
                    ESP_LOGE(RMT5_POOL_TAG, "Existing channel reconfigured for GPIO %d (retry path, iteration %u)",
                             (int)pin, poll_count);
                }
                ESP_LOGE(RMT5_POOL_TAG, "=== acquireWorker() SUCCESS (retry path) - returning worker %p ===", worker);
                return worker;
            }

            ESP_LOGE(RMT5_POOL_TAG, "worker->configure() FAILED in retry path!");

            // Configuration failed - likely RMT channel exhaustion
            config_fail_count++;

            if (config_fail_count >= MAX_CONFIG_RETRIES) {
                // STRICT MODE: Abort with stack trace after exhausting retries
                ESP_LOGE(RMT5_POOL_TAG, "");
                ESP_LOGE(RMT5_POOL_TAG, "========================================");
                ESP_LOGE(RMT5_POOL_TAG, "FATAL: RMT CHANNEL EXHAUSTION");
                ESP_LOGE(RMT5_POOL_TAG, "========================================");
                ESP_LOGE(RMT5_POOL_TAG, "Expected:  %d RMT channels", mExpectedChannels);
                ESP_LOGE(RMT5_POOL_TAG, "Created:   %d channels", mCreatedChannels);
                ESP_LOGE(RMT5_POOL_TAG, "Failed on: GPIO %d (after %u retries)", (int)pin, config_fail_count);
                ESP_LOGE(RMT5_POOL_TAG, "========================================");
                ESP_LOGE(RMT5_POOL_TAG, "");

                // Stack trace from abort() will be printed by panic handler
                abort();
            }

            ESP_LOGW(RMT5_POOL_TAG, "Worker configuration failed (attempt %u/%u) - will retry",
                     config_fail_count, MAX_CONFIG_RETRIES);
            worker = nullptr;
        }

        // Yield to FreeRTOS periodically to prevent watchdog
        if (poll_count % 50 == 0) {  // Every 5ms
            taskYIELD();
        }

        // Safety: Warn if waiting too long (more than 100ms = very unusual)
        if (poll_count % 1000 == 0) {  // Every 100ms
            ESP_LOGW(RMT5_POOL_TAG, "Still waiting for available worker after %u ms",
                static_cast<unsigned int>(poll_count / 10));
        }
    }
}

void RmtWorkerPool::releaseWorker(IRmtWorkerBase* worker) {
    FL_ASSERT(worker != nullptr, "RmtWorkerPool::releaseWorker called with null worker");

    // Worker marks itself as available after transmission completes
    // Nothing to do here - worker is automatically recycled
}

int RmtWorkerPool::getAvailableCount() const {
    portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&mSpinlock));
    int count = 0;

    // Count double-buffer workers
    for (int i = 0; i < static_cast<int>(mDoubleBufferWorkers.size()); i++) {
        if (mDoubleBufferWorkers[i]->isAvailable()) {
            count++;
        }
    }

    // Count one-shot workers
    for (int i = 0; i < static_cast<int>(mOneShotWorkers.size()); i++) {
        if (mOneShotWorkers[i]->isAvailable()) {
            count++;
        }
    }

    portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&mSpinlock));
    return count;
}

RmtWorker* RmtWorkerPool::findAvailableDoubleBufferWorker() {
    // Caller must hold mSpinlock
    ESP_LOGD(RMT5_POOL_TAG, "findAvailableDoubleBufferWorker() - searching %zu workers",
             mDoubleBufferWorkers.size());

    for (int i = 0; i < static_cast<int>(mDoubleBufferWorkers.size()); i++) {
        bool available = mDoubleBufferWorkers[i]->isAvailable();
        ESP_LOGD(RMT5_POOL_TAG, "  Worker[%d]: available=%d", i, available);
        if (available) {
            ESP_LOGE(RMT5_POOL_TAG, "findAvailableDoubleBufferWorker() - found worker[%d]", i);
            return mDoubleBufferWorkers[i];
        }
    }
    ESP_LOGD(RMT5_POOL_TAG, "findAvailableDoubleBufferWorker() - no available workers found");
    return nullptr;
}

RmtWorkerOneShot* RmtWorkerPool::findAvailableOneShotWorker() {
    // Caller must hold mSpinlock
    for (int i = 0; i < static_cast<int>(mOneShotWorkers.size()); i++) {
        if (mOneShotWorkers[i]->isAvailable()) {
            return mOneShotWorkers[i];
        }
    }
    return nullptr;
}

int RmtWorkerPool::getMaxWorkers() {
    // Platform-specific maximum worker count
    // Double-buffering uses 2 memory blocks per worker, so actual worker count
    // is half the number of RMT TX channels

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-C3/C6/H2: 2 RMT TX channels ÷ 2 (double-buffer) = 1 worker
    return 1;
#elif defined(SOC_RMT_TX_CANDIDATES_PER_GROUP)
    // Modern ESP-IDF defines this (ESP32-S3, etc.)
    // Divide by 2 for double-buffering
    return SOC_RMT_TX_CANDIDATES_PER_GROUP / 2;
#elif defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32: 8 RMT TX channels ÷ 2 (double-buffer) = 4 workers
    return 4;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S2/S3: 4 RMT TX channels ÷ 2 (double-buffer) = 2 workers
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
