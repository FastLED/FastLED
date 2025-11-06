#ifdef ESP32

#include "fl/compiler_control.h"

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_pool.h"

#include <Arduino.h>  // ok include - needed for delayMicroseconds()

FL_EXTERN_C_BEGIN

#include "esp_log.h"
#include "freertos/task.h"
#include "esp_debug_helpers.h"  // For esp_backtrace_print()

FL_EXTERN_C_END

#include "fl/assert.h"
#include "fl/warn.h"
#include "fl/log.h"
#include "fl/chipsets/led_timing.h"

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
    // Clear alias vector first (before deleting) to avoid dangling pointers
    // mWorkers is an alias for mDoubleBufferWorkers and contains the same pointers
    mWorkers.clear();

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
    mExpectedChannels = max_workers;
    mCreatedChannels = 0;



    // Create K double-buffer workers (current default)
    // HYBRID MODE: Currently only creating double-buffer workers
    //
    // Rationale: Each worker needs its own RMT hardware channel, and we only have K channels.
    // We can't create both double-buffer AND one-shot workers without exceeding K channels.
    //
    // Future hybrid approach could:
    // 1. Create some double-buffer + some one-shot workers (split K channels)
    // 2. Dynamically reallocate channels based on strip sizes
    // 3. Use DMA mode for large strips (ESP32-S3/C6 only)
    //
    // For now: All workers are double-buffer type, pool selects based on size at acquire time
    ESP_LOGD(RMT5_POOL_TAG, "Starting worker creation loop...");
    for (int i = 0; i < max_workers; i++) {
        ESP_LOGD(RMT5_POOL_TAG, "");
        ESP_LOGD(RMT5_POOL_TAG, "--- Creating worker %d/%d ---", i + 1, max_workers);
        ESP_LOGD(RMT5_POOL_TAG, "Allocating RmtWorker object...");
        // Pass spinlock reference to worker for unified synchronization
        RmtWorker* worker = new RmtWorker(&mSpinlock);
        ESP_LOGD(RMT5_POOL_TAG, "Worker object allocated at %p", worker);

        ESP_LOGD(RMT5_POOL_TAG, "Calling worker->initialize(%d)...", i);
        if (!worker->initialize(static_cast<uint8_t>(i))) {
            FL_WARN("FAILED to initialize double-buffer worker " << i << " - skipping");
            delete worker;
            continue;
        }
        FL_LOG_RMT("Worker " << i << " initialized successfully");

        ESP_LOGD(RMT5_POOL_TAG, "Adding worker %d to pool vectors...", i);
        mDoubleBufferWorkers.push_back(worker);
        mWorkers.push_back(worker);  // Legacy support
        ESP_LOGD(RMT5_POOL_TAG, "Worker %d added to pool (total workers: %zu)", i, mDoubleBufferWorkers.size());
    }


    if (mDoubleBufferWorkers.size() == 0 && mOneShotWorkers.size() == 0) {
        FL_WARN("FATAL: No workers initialized successfully!");
    }

    mInitialized = true;
    FL_LOG_RMT("Pool initialized with " << mDoubleBufferWorkers.size() << " workers");
}

IRmtWorkerBase* RmtWorkerPool::acquireWorker(
    int num_bytes,
    gpio_num_t pin,
    const ChipsetTiming& TIMING,
    uint32_t reset_ns
) {
    // Extract timing values from struct
    uint32_t t1 = TIMING.T1;
    uint32_t t2 = TIMING.T2;
    uint32_t t3 = TIMING.T3;



    // Initialize workers on first use
    ESP_LOGD(RMT5_POOL_TAG, "Calling initializeWorkersIfNeeded()...");
    initializeWorkersIfNeeded();
    ESP_LOGD(RMT5_POOL_TAG, "initializeWorkersIfNeeded() returned");

    // HYBRID MODE: Determine preferred worker type based on strip size
    // Small strips (<=200 LEDs / 600 bytes) would benefit from one-shot (zero flicker)
    // Large strips (>200 LEDs) use double-buffer (memory efficient)
    //
    // NOTE: Currently all workers are double-buffer type since we don't create one-shot workers
    // This logic is preserved for future hybrid mode implementation
    bool use_oneshot = (static_cast<size_t>(num_bytes) <= ONE_SHOT_THRESHOLD_BYTES);

    // Try to get an available worker immediately
    ESP_LOGD(RMT5_POOL_TAG, "Entering critical section to search for available worker...");
    portENTER_CRITICAL(&mSpinlock);
    IRmtWorkerBase* worker = nullptr;

    ESP_LOGD(RMT5_POOL_TAG, "Available double-buffer workers: %zu", mDoubleBufferWorkers.size());
    ESP_LOGD(RMT5_POOL_TAG, "Available one-shot workers: %zu", mOneShotWorkers.size());

    // Try one-shot first if preferred and available
    if (use_oneshot && mOneShotWorkers.size() > 0) {
        ESP_LOGD(RMT5_POOL_TAG, "Searching for available one-shot worker...");
        worker = findAvailableOneShotWorker();
        if (worker) {
            ESP_LOGD(RMT5_POOL_TAG, "Found ONE-SHOT worker for %d bytes (%d LEDs)",
                     num_bytes, num_bytes / 3);
        } else {
            ESP_LOGD(RMT5_POOL_TAG, "No one-shot worker available");
        }
    }

    // Fall back to double-buffer worker
    if (!worker) {
        ESP_LOGD(RMT5_POOL_TAG, "Searching for available double-buffer worker...");
        worker = findAvailableDoubleBufferWorker();
        if (worker) {
            if (use_oneshot && mOneShotWorkers.size() > 0) {
                ESP_LOGD(RMT5_POOL_TAG, "Using DOUBLE-BUFFER worker for %d bytes (no one-shot available)", num_bytes);
            } else {
                ESP_LOGD(RMT5_POOL_TAG, "Found DOUBLE-BUFFER worker for %d bytes", num_bytes);
            }
        } else {
            ESP_LOGD(RMT5_POOL_TAG, "No double-buffer worker available");
        }
    }

    // CRITICAL: Mark worker unavailable BEFORE releasing spinlock
    // This prevents race where another thread/core acquires same worker
    if (worker) {
        worker->markAsUnavailable();  // Under spinlock - atomic visibility
    }

    portEXIT_CRITICAL(&mSpinlock);
    ESP_LOGD(RMT5_POOL_TAG, "Exited critical section - worker=%p", worker);

    if (worker) {
        ESP_LOGD(RMT5_POOL_TAG, "Worker found! Configuring worker...");

        // Check if this worker already has a channel (to track actual channel creation)
        bool had_channel = worker->hasChannel();
        ESP_LOGD(RMT5_POOL_TAG, "Worker already has channel: %d", had_channel);

        // Configure the worker before returning
        ESP_LOGD(RMT5_POOL_TAG, "Calling worker->configure(pin=%d, t1=%d, t2=%d, t3=%d, reset_ns=%lu)...",
                 (int)pin, t1, t2, t3, reset_ns);
        if (!worker->configure(pin, TIMING, reset_ns)) {
            FL_WARN("worker->configure() FAILED!");

            // Configuration failed (likely channel creation failed due to exhaustion)
            // Note: Worker state leak doesn't matter since abort() terminates program
            // If this were changed to error handling, must release worker first

            // STRICT MODE: Abort immediately with stack trace
            FL_WARN("");
            FL_WARN("========================================");
            FL_WARN("FATAL: RMT CHANNEL EXHAUSTION");
            FL_WARN("========================================");
            FL_WARN("Expected:  " << mExpectedChannels << " RMT channels");
            FL_WARN("Created:   " << mCreatedChannels << " channels");
            FL_WARN("Failed on: GPIO " << (int)pin << " (first acquisition)");
            FL_WARN("========================================");
            FL_WARN("");

            // Stack trace from abort() will be printed by panic handler
            abort();
        } else {
            ESP_LOGD(RMT5_POOL_TAG, "worker->configure() SUCCESS!");

            // Successfully configured - track channel creation only if this is a NEW channel
            if (!had_channel) {
                mCreatedChannels++;
                ESP_LOGD(RMT5_POOL_TAG, "NEW channel created: %d/%d for GPIO %d",
                         mCreatedChannels, mExpectedChannels, (int)pin);
            } else {
                ESP_LOGD(RMT5_POOL_TAG, "Existing channel reconfigured for GPIO %d", (int)pin);
            }

            ESP_LOGD(RMT5_POOL_TAG, "=== acquireWorker() SUCCESS - returning worker %p ===", worker);
            return worker;
        }
    }

    // No workers available (or configuration failed) - poll until one frees up
    // This implements the N > K blocking behavior
    ESP_LOGD(RMT5_POOL_TAG, "No worker immediately available - entering polling loop...");
    uint32_t poll_count = 0;
    uint32_t config_fail_count = 0;  // Track consecutive configuration failures
    const uint32_t MAX_CONFIG_RETRIES = 10;  // Limit retries before aborting
    const uint32_t MAX_POLL_ITERATIONS = 50000;  // 5 seconds at 100µs per iteration

    while (poll_count < MAX_POLL_ITERATIONS) {
        poll_count++;
        // Short delay before retry
        delayMicroseconds(100);

        if (poll_count % 50 == 0) {
            ESP_LOGD(RMT5_POOL_TAG, "Polling iteration %u - searching for available worker...", poll_count);
        }

        portENTER_CRITICAL(&mSpinlock);

        if (use_oneshot && mOneShotWorkers.size() > 0) {
            worker = findAvailableOneShotWorker();
        }

        if (!worker) {
            worker = findAvailableDoubleBufferWorker();
        }

        // CRITICAL: Mark unavailable BEFORE releasing spinlock
        if (worker) {
            worker->markAsUnavailable();
        }

        portEXIT_CRITICAL(&mSpinlock);

        if (worker) {
            ESP_LOGD(RMT5_POOL_TAG, "Worker found in polling loop (iteration %u)", poll_count);

            // Check if this worker already has a channel (to track actual channel creation)
            bool had_channel = worker->hasChannel();
            ESP_LOGD(RMT5_POOL_TAG, "Worker already has channel: %d", had_channel);

            ESP_LOGD(RMT5_POOL_TAG, "Configuring worker in retry path...");
            if (worker->configure(pin, TIMING, reset_ns)) {
                ESP_LOGD(RMT5_POOL_TAG, "worker->configure() SUCCESS in retry path!");

                // Success - track channel creation only if this is a NEW channel
                if (!had_channel) {
                    mCreatedChannels++;
                    ESP_LOGD(RMT5_POOL_TAG, "NEW channel created: %d/%d for GPIO %d (retry path, iteration %u)",
                             mCreatedChannels, mExpectedChannels, (int)pin, poll_count);
                } else {
                    ESP_LOGD(RMT5_POOL_TAG, "Existing channel reconfigured for GPIO %d (retry path, iteration %u)",
                             (int)pin, poll_count);
                }
                ESP_LOGD(RMT5_POOL_TAG, "=== acquireWorker() SUCCESS (retry path) - returning worker %p ===", worker);
                return worker;
            }

            ESP_LOGD(RMT5_POOL_TAG, "worker->configure() FAILED in retry path!");

            // Configuration failed - likely RMT channel exhaustion
            config_fail_count++;

            if (config_fail_count >= MAX_CONFIG_RETRIES) {
                // STRICT MODE: Abort with stack trace after exhausting retries
                FL_WARN("");
                FL_WARN("========================================");
                FL_WARN("FATAL: RMT CHANNEL EXHAUSTION");
                FL_WARN("========================================");
                FL_WARN("Expected:  " << mExpectedChannels << " RMT channels");
                FL_WARN("Created:   " << mCreatedChannels << " channels");
                FL_WARN("Failed on: GPIO " << (int)pin << " (after " << config_fail_count << " retries)");
                FL_WARN("========================================");
                FL_WARN("");

                // Stack trace from abort() will be printed by panic handler
                abort();
            }

            FL_WARN("Worker configuration failed (attempt " << config_fail_count << "/" << MAX_CONFIG_RETRIES << ") - will retry");
            worker = nullptr;
        }

        // Yield to FreeRTOS periodically to prevent watchdog
        if (poll_count % 50 == 0) {  // Every 5ms
            taskYIELD();
        }

        // Safety: Warn if waiting too long (more than 100ms = very unusual)
        if (poll_count % 1000 == 0) {  // Every 100ms
            FL_WARN("Still waiting for available worker after " << (poll_count / 10) << " ms");
        }
    }

    // Timeout reached - no worker became available
    FL_WARN("");
    FL_WARN("========================================");
    FL_WARN("WORKER ACQUISITION TIMEOUT");
    FL_WARN("========================================");
    FL_WARN("Failed to acquire worker for GPIO " << (int)pin << " after " << (poll_count / 10) << " ms");
    FL_WARN("Available workers: 0 (all " << (mDoubleBufferWorkers.size() + mOneShotWorkers.size()) << " workers are busy)");
    FL_WARN("This usually indicates too many LED strips for available RMT channels");
    FL_WARN("========================================");
    FL_WARN("");

    return nullptr;
}

void RmtWorkerPool::releaseWorker(IRmtWorkerBase* worker) {
    FL_ASSERT(worker != nullptr, "RmtWorkerPool::releaseWorker called with null worker");

    // CRITICAL: Mark worker available under spinlock for atomic visibility
    // This fixes the race condition where ISR completes but worker isn't yet available
    portENTER_CRITICAL(&mSpinlock);
    worker->markAsAvailable();  // Sets mAvailable = true under lock
    portEXIT_CRITICAL(&mSpinlock);

    ESP_LOGD(RMT5_POOL_TAG, "Worker released and marked available");
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
            ESP_LOGD(RMT5_POOL_TAG, "findAvailableDoubleBufferWorker() - found worker[%d]", i);
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
