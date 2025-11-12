/// @file channel_engine_rmt.cpp
/// @brief RMT5 ChannelEngine implementation

#ifdef ESP32

#include "fl/compiler_control.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "channel_engine_rmt.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/chipsets/led_timing.h"
#include "fl/algorithm.h"
#include "fl/time.h"
#include "fl/assert.h"
#include "fl/delay.h"
#include "fl/log.h"

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "esp_log.h"
FL_EXTERN_C_END

#define RMT_ENGINE_TAG "channel_engine_rmt"

namespace fl {

ChannelEngineRMT::ChannelEngineRMT()
    : mInitialized(false)
{
    // Register as listener for end frame events
    EngineEvents::addListener(this);
}

ChannelEngineRMT::~ChannelEngineRMT() {
    // Unregister from events
    EngineEvents::removeListener(this);

    while (true) {
        auto state = pollDerived();
        bool is_done = state == EngineState::READY || state == EngineState::ERROR;
        if (is_done) {
            break;
        }
        // Spin until ready
        fl::delayMicroseconds(100);
    }

    // Clean up workers
    for (int i = 0; i < static_cast<int>(mWorkers.size()); i++) {
        delete mWorkers[i];
    }
    mWorkers.clear();
}

void ChannelEngineRMT::initializeWorkersIfNeeded() {
    // Set log level based on build type
#ifdef NDEBUG
    // Release build - use INFO level
    esp_log_level_set(RMT_ENGINE_TAG, ESP_LOG_INFO);
#else
    // Debug build - use VERBOSE level for detailed tracing
    esp_log_level_set(RMT_ENGINE_TAG, ESP_LOG_VERBOSE);
#endif

    if (mInitialized) {
        return;
    }

    // Allocate workers until the RMT system gives us an error
    // This dynamically discovers how many channels are available
    ESP_LOGD(RMT_ENGINE_TAG, "Creating workers until hardware limit...");

    int worker_id = 0;
    while (true) {
        ESP_LOGD(RMT_ENGINE_TAG, "Attempting to create worker %d", worker_id);
        RmtWorker* worker = new RmtWorker();

        ESP_LOGD(RMT_ENGINE_TAG, "Initializing worker %d", worker_id);
        if (!worker->initialize(static_cast<uint8_t>(worker_id))) {
            // Failed to initialize - we've hit the hardware limit
            ESP_LOGD(RMT_ENGINE_TAG, "Worker %d initialization failed - hardware limit reached", worker_id);
            delete worker;
            break;
        }

        FL_LOG_RMT("Worker " << worker_id << " initialized successfully");
        mWorkers.push_back(worker);
        worker_id++;

        // Safety limit to prevent infinite loop
        if (worker_id >= 16) {
            ESP_LOGW(RMT_ENGINE_TAG, "Reached safety limit of 16 workers");
            break;
        }
    }

    if (mWorkers.size() == 0) {
        FL_WARN("FATAL: No workers initialized successfully!");
    }

    mInitialized = true;
    FL_LOG_RMT("Engine initialized with " << mWorkers.size() << " workers");
}

RmtWorker* ChannelEngineRMT::findAvailableWorker() {
    ESP_LOGD(RMT_ENGINE_TAG, "Searching %zu workers for available worker",
             mWorkers.size());

    for (int i = 0; i < static_cast<int>(mWorkers.size()); i++) {
        if (mWorkers[i]->isAvailable()) {
            ESP_LOGD(RMT_ENGINE_TAG, "Found available worker[%d]", i);
            return mWorkers[i];
        }
    }
    ESP_LOGD(RMT_ENGINE_TAG, "No available workers found");
    return nullptr;
}

void ChannelEngineRMT::releaseWorker(IRmtWorkerBase* worker) {
    FL_ASSERT(worker != nullptr, "ChannelEngineRMT::releaseWorker called with null worker");

    // Mark worker as available
    // Note: ISR already sets mAvailable=true at end of handleDoneInterrupt(),
    // but this redundant write serves as defensive programming to ensure
    // the worker is always available after waitForCompletion() returns.
    worker->markAsAvailable();

    ESP_LOGD(RMT_ENGINE_TAG, "Worker released and marked available");
}

void ChannelEngineRMT::onEndFrame() {
    while (true) {
        auto state = pollDerived();
        if (state != EngineState::BUSY) {
            break;
        }
        // Spin-wait until ready
        fl::delayMicroseconds(100);
    }
}

ChannelEngine::EngineState ChannelEngineRMT::pollDerived() {
    // Process pending channels if workers have become available
    processPendingChannels();

    // If no active workers and no pending channels, we're ready
    if (mActiveWorkers.empty() && mPendingChannels.empty()) {
        return EngineState::READY;
    }

    // Separate workers into still-active vs completed
    fl::vector_inlined<WorkerInfo, 16> justRetired;

    // Iterate forward and collect indices to remove
    for (size_t i = 0; i < mActiveWorkers.size(); ) {
        const WorkerInfo& info = mActiveWorkers[i];
        if (info.worker->isAvailable()) {  // Worker has completed transmission
            justRetired.push_back(info);
            mAvailableWorkers.push_back(info);
            // Remove this element by swapping with last and popping
            if (i < mActiveWorkers.size() - 1) {
                mActiveWorkers[i] = mActiveWorkers[mActiveWorkers.size() - 1];
            }
            mActiveWorkers.pop_back();
            // Don't increment i since we just moved a new element to this position
        } else {
            ++i;
        }
    }
    // Release completed workers
    for (const WorkerInfo& info : justRetired) {
        releaseWorker(info.worker);
    }
    return EngineState::BUSY;
}

void ChannelEngineRMT::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    // Clear previous workers
    if (!mActiveWorkers.empty()) {
        fl::string errMsg = "Previous workers not cleared";
        FL_ASSERT(false, errMsg);
        setLastError(errMsg);
        return;
    }

    // Re-order so that strips with the smallest payloads go first, this helps
    // async transmissions since we have to wait until all channels are at least
    // in the draining state before drawing can release.
    fl::vector_inlined<ChannelDataPtr, 16> channels_reordered;
    for (const auto& data : channelData) {
        channels_reordered.push_back(data);
    }
    fl::sort(channels_reordered.begin(), channels_reordered.end(), [](const ChannelDataPtr& a, const ChannelDataPtr& b) {
        // Reverse order so that we can pop elements from the back without
        // invalidating the iterator.
        return a->getSize() > b->getSize();
    });

    // Acquire and transmit for each channel
    for (const auto& data : channels_reordered) {
        int pin = data->getPin();
        // Convert ChipsetTimingConfig to ChipsetTiming
        const auto& timingConfig = data->getTiming();
        ChipsetTiming timing = {
            timingConfig.t1_ns,
            timingConfig.t2_ns,
            timingConfig.t3_ns,
            timingConfig.reset_us,
            timingConfig.name
        };

        // Initialize workers on first use
        initializeWorkersIfNeeded();

        // Try to start transmission for this channel
        PendingChannel pending{data, pin, timing, timingConfig.reset_us};
        if (!tryStartPendingChannel(pending)) {
            // No worker available - queue it for later
            ESP_LOGD(RMT_ENGINE_TAG, "No worker available for pin %d - queuing", pin);
            mPendingChannels.push_back(pending);
        }
    }
}

bool ChannelEngineRMT::tryStartPendingChannel(const PendingChannel& pending) {
    // Initialize workers on first use
    initializeWorkersIfNeeded();

    // Acquire worker from engine's worker pool
    IRmtWorkerBase* worker = findAvailableWorker();

    if (!worker) {
        return false;  // No worker available
    }

    // Mark worker as unavailable (acquired)
    worker->markAsUnavailable();

    // Configure the worker with pin and timing
    if (!worker->configure(static_cast<gpio_num_t>(pending.pin), pending.timing)) {
        FL_WARN("Failed to configure RMT worker for pin " << pending.pin);
        releaseWorker(worker);
        return false;
    }

    // Start transmission
    worker->transmit(pending.data->getData().data(), pending.data->getSize());

    // Track this worker with its pin and reset time
    mActiveWorkers.push_back(WorkerInfo{worker, pending.pin, pending.reset_us});

    ESP_LOGD(RMT_ENGINE_TAG, "Started transmission for pin %d", pending.pin);
    return true;
}

void ChannelEngineRMT::processPendingChannels() {
    if (mPendingChannels.empty()) {
        return;
    }

    // Try to start pending channels
    for (size_t i = 0; i < mPendingChannels.size(); ) {
        const PendingChannel& pending = mPendingChannels[i];
        if (tryStartPendingChannel(pending)) {
            // Started successfully - remove from pending list
            if (i < mPendingChannels.size() - 1) {
                mPendingChannels[i] = mPendingChannels[mPendingChannels.size() - 1];
            }
            mPendingChannels.pop_back();
            // Don't increment i since we just moved a new element to this position
        } else {
            // Still no worker available - keep in pending list and try next iteration
            ++i;
        }
    }
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
