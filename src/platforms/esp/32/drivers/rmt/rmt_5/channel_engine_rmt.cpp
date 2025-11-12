/// @file channel_engine_rmt.cpp
/// @brief RMT5 ChannelEngine implementation

#ifdef ESP32

#include "fl/compiler_control.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "channel_engine_rmt.h"
#include "rmt5_worker_pool.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/chipsets/led_timing.h"
#include "fl/algorithm.h"
#include "fl/time.h"
#include "fl/assert.h"
#include "fl/delay.h"

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
FL_EXTERN_C_END

namespace fl {

ChannelEngineRMT::ChannelEngineRMT() {
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
    // If no active workers, check if all pin resets are complete
    if (mActiveWorkers.empty()) {
        // Check if any pins are still in reset period
        uint32_t now = ::micros();
        bool anyPinInReset = false;
        for (auto it = mPinResetTimers.begin(); it != mPinResetTimers.end(); ++it) {
            if (!(*it).second.done(now)) {
                anyPinInReset = true;
                break;
            }
        }
        if (!anyPinInReset) {
            mPinResetTimers.clear();
        }
        return anyPinInReset ? EngineState::BUSY : EngineState::READY;
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
    // Release completed workers and start their reset timers
    uint32_t now = ::micros();
    for (const WorkerInfo& info : justRetired) {
        RmtWorkerPool::getInstance().releaseWorker(info.worker);
        // Mark the reset timer for this pin
        if (info.reset_us > 0) {
            mPinResetTimers[info.pin] = Timeout(now, info.reset_us);
        }
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

    // wait for all reset periods to complete
    while (true) {
        uint32_t now = ::micros();
        bool allDone = true;
        for (auto it = mPinResetTimers.begin(); it != mPinResetTimers.end(); ++it) {
            if (!(*it).second.done(now)) {
                allDone = false;
                break;
            }
        }
        // All reset periods are done
        if (allDone) {
            break;
        }
        // Spin-wait for reset period to complete
        fl::delayMicroseconds(10);
    }

    mPinResetTimers.clear();

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

        // Acquire worker from pool
        auto& pool = RmtWorkerPool::getInstance();
        IRmtWorkerBase* worker = pool.tryAcquireWorker(
            data->getSize(),
            static_cast<gpio_num_t>(pin),
            timing,
            timingConfig.reset_us * 1000  // Convert microseconds to nanoseconds
        );

        if (!worker) {
            FL_WARN("Failed to acquire RMT worker for pin " << pin);
            continue;
        }

        // Start transmission
        worker->transmit(data->getData().data(), data->getSize());

        // Track this worker with its pin and reset time
        mActiveWorkers.push_back(WorkerInfo{worker, pin, timingConfig.reset_us});
        mPinResetTimers[pin] = Timeout(::micros(), timingConfig.reset_us);
    }
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
