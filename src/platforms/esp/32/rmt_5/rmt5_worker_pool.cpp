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
    if (mInitialized) {
        return;
    }

    int max_workers = getMaxWorkers();
    mExpectedChannels = max_workers;
    mCreatedChannels = 0;

    ESP_LOGI(RMT5_POOL_TAG, "Initializing %d workers (hybrid mode: threshold=%d LEDs)",
             max_workers, FASTLED_ONESHOT_THRESHOLD_LEDS);
    ESP_LOGI(RMT5_POOL_TAG, "RMT worker pool initialization starting - max_workers=%d", max_workers);
    ESP_LOGI(RMT5_POOL_TAG, "STRICT MODE: Will abort if all %d channels cannot be created", max_workers);

    // Create K double-buffer workers (current default)
    // Note: We only create one type because we only have K hardware channels
    // Future: could dynamically choose worker type based on usage patterns
    for (int i = 0; i < max_workers; i++) {
        ESP_LOGI(RMT5_POOL_TAG, "Creating double-buffer worker %d/%d", i, max_workers);
        RmtWorker* worker = new RmtWorker();
        if (!worker->initialize(static_cast<uint8_t>(i))) {
            ESP_LOGE(RMT5_POOL_TAG, "Failed to initialize double-buffer worker %d - skipping", i);
            delete worker;
            continue;
        }
        ESP_LOGI(RMT5_POOL_TAG, "Successfully initialized double-buffer worker %d", i);
        mDoubleBufferWorkers.push_back(worker);
        mWorkers.push_back(worker);  // Legacy support
    }

    ESP_LOGI(RMT5_POOL_TAG, "Initialized %zu double-buffer workers (one-shot infrastructure ready)",
             mDoubleBufferWorkers.size());

    if (mDoubleBufferWorkers.size() == 0 && mOneShotWorkers.size() == 0) {
        ESP_LOGE(RMT5_POOL_TAG, "No workers initialized successfully!");
    }

    mInitialized = true;
}

IRmtWorkerBase* RmtWorkerPool::acquireWorker(
    int num_bytes,
    gpio_num_t pin,
    int t1, int t2, int t3,
    uint32_t reset_ns
) {
    ESP_LOGI(RMT5_POOL_TAG, "acquireWorker called: num_bytes=%d, pin=%d, t1=%d, t2=%d, t3=%d",
             num_bytes, (int)pin, t1, t2, t3);

    // Initialize workers on first use
    initializeWorkersIfNeeded();

    // Determine which worker type to use based on strip size
    // For now, always use double-buffer (hybrid mode for future expansion)
    bool use_oneshot = (static_cast<size_t>(num_bytes) <= ONE_SHOT_THRESHOLD_BYTES);

    ESP_LOGI(RMT5_POOL_TAG, "Worker selection: use_oneshot=%d, threshold_bytes=%zu",
             use_oneshot, ONE_SHOT_THRESHOLD_BYTES);

    // Try to get an available worker immediately
    portENTER_CRITICAL(&mSpinlock);
    IRmtWorkerBase* worker = nullptr;

    if (use_oneshot && mOneShotWorkers.size() > 0) {
        worker = findAvailableOneShotWorker();
        if (worker) {
            ESP_LOGD(RMT5_POOL_TAG, "Using ONE-SHOT worker for %d bytes (%d LEDs)",
                     num_bytes, num_bytes / 3);
        }
    }

    if (!worker) {
        // Fall back to double-buffer (either by choice or if no one-shot available)
        worker = findAvailableDoubleBufferWorker();
        if (worker && use_oneshot) {
            ESP_LOGD(RMT5_POOL_TAG, "Using DOUBLE-BUFFER worker for %d bytes (no one-shot available)",
                     num_bytes);
        }
    }

    portEXIT_CRITICAL(&mSpinlock);

    if (worker) {
        // Configure the worker before returning
        if (!worker->configure(pin, t1, t2, t3, reset_ns)) {
            // Configuration failed (likely channel creation failed due to exhaustion)
            // STRICT MODE: Abort immediately with stack trace
            ESP_LOGE(RMT5_POOL_TAG, "FATAL: Failed to configure worker on first acquisition attempt!");
            ESP_LOGE(RMT5_POOL_TAG, "Expected %d RMT channels for this %s variant, but channel creation failed",
                     mExpectedChannels,
#if defined(CONFIG_IDF_TARGET_ESP32)
                     "ESP32"
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
                     "ESP32-S2"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
                     "ESP32-S3"
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
                     "ESP32-C2"
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
                     "ESP32-C3"
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
                     "ESP32-C6"
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
                     "ESP32-H2"
#else
                     "Unknown ESP32"
#endif
            );
            ESP_LOGE(RMT5_POOL_TAG, "Pin: GPIO %d, Worker type: %s",
                     (int)pin,
                     use_oneshot ? "ONE-SHOT" : "DOUBLE-BUFFER");
            ESP_LOGE(RMT5_POOL_TAG, "This indicates a bug in RMT channel management - dumping stack trace:");

            // Print stack trace
            esp_backtrace_print(100);

            // Abort to trigger core dump
            ESP_LOGE(RMT5_POOL_TAG, "ABORTING due to RMT channel exhaustion on first acquisition");
            abort();
        } else {
            // Successfully configured - track channel creation
            mCreatedChannels++;
            ESP_LOGI(RMT5_POOL_TAG, "Successfully created RMT channel %d/%d for GPIO %d",
                     mCreatedChannels, mExpectedChannels, (int)pin);
            return worker;
        }
    }

    // No workers available (or configuration failed) - poll until one frees up
    // This implements the N > K blocking behavior
    uint32_t poll_count = 0;
    uint32_t config_fail_count = 0;  // Track consecutive configuration failures
    const uint32_t MAX_CONFIG_RETRIES = 10;  // Limit retries before aborting

    while (true) {
        // Short delay before retry
        delayMicroseconds(100);

        portENTER_CRITICAL(&mSpinlock);

        if (use_oneshot && mOneShotWorkers.size() > 0) {
            worker = findAvailableOneShotWorker();
        }

        if (!worker) {
            worker = findAvailableDoubleBufferWorker();
        }

        portEXIT_CRITICAL(&mSpinlock);

        if (worker) {
            if (worker->configure(pin, t1, t2, t3, reset_ns)) {
                // Success - track channel creation if this is a new channel
                mCreatedChannels++;
                ESP_LOGI(RMT5_POOL_TAG, "Successfully configured RMT channel for GPIO %d (retry path)",
                         (int)pin);
                return worker;
            }
            // Configuration failed - likely RMT channel exhaustion
            config_fail_count++;

            if (config_fail_count >= MAX_CONFIG_RETRIES) {
                // STRICT MODE: Abort with stack trace after exhausting retries
                ESP_LOGE(RMT5_POOL_TAG, "FATAL: Failed to configure worker after %u retries!",
                         config_fail_count);
                ESP_LOGE(RMT5_POOL_TAG, "Expected %d RMT channels for this %s variant",
                         mExpectedChannels,
#if defined(CONFIG_IDF_TARGET_ESP32)
                         "ESP32"
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
                         "ESP32-S2"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
                         "ESP32-S3"
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
                         "ESP32-C2"
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
                         "ESP32-C3"
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
                         "ESP32-C6"
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
                         "ESP32-H2"
#else
                         "Unknown ESP32"
#endif
                );
                ESP_LOGE(RMT5_POOL_TAG, "Successfully created: %d channels", mCreatedChannels);
                ESP_LOGE(RMT5_POOL_TAG, "Failed channel - Pin: GPIO %d, Worker type: %s",
                         (int)pin,
                         use_oneshot ? "ONE-SHOT" : "DOUBLE-BUFFER");
                ESP_LOGE(RMT5_POOL_TAG, "This indicates RMT channels are exhausted - dumping stack trace:");

                // Print stack trace
                esp_backtrace_print(100);

                // Abort to trigger core dump
                ESP_LOGE(RMT5_POOL_TAG, "ABORTING due to RMT channel exhaustion after retries");
                abort();
            }

            ESP_LOGW(RMT5_POOL_TAG, "Worker configuration failed (attempt %u/%u) - will retry",
                     config_fail_count, MAX_CONFIG_RETRIES);
            worker = nullptr;
        }

        // Yield to FreeRTOS periodically to prevent watchdog
        poll_count++;
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
    for (int i = 0; i < static_cast<int>(mDoubleBufferWorkers.size()); i++) {
        if (mDoubleBufferWorkers[i]->isAvailable()) {
            return mDoubleBufferWorkers[i];
        }
    }
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
