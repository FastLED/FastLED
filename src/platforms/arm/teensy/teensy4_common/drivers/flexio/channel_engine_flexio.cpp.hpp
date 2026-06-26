// IWYU pragma: private

#ifndef CHANNEL_ENGINE_FLEXIO_CPP_HPP_
#define CHANNEL_ENGINE_FLEXIO_CPP_HPP_

#include "platforms/arm/teensy/is_teensy.h"

// Channel engine compiles on all platforms (uses IFlexIOPeripheral abstraction)
// Only the real peripheral implementation is Teensy-specific

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/iflexio_peripheral.h"

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

#if defined(FL_IS_TEENSY_4X)
// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep
#endif

namespace fl {

// Timing period bounds for canHandle() filtering.
// Same range as ObjectFLED - standard clockless protocols only.
static constexpr u32 kFlexIOMinPeriodNs = 1000;
static constexpr u32 kFlexIOMaxPeriodNs = 2500;

// ============================================================================
// ChannelEngineFlexIO Implementation
// ============================================================================

ChannelEngineFlexIO::ChannelEngineFlexIO()
 FL_NO_EXCEPT : mPeripheral(IFlexIOPeripheral::create()),
      mHwInitialized(false), mCurrentPin(0xFF) {
    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: created");
}

ChannelEngineFlexIO::ChannelEngineFlexIO(fl::shared_ptr<IFlexIOPeripheral> peripheral)
 FL_NO_EXCEPT : mPeripheral(fl::move(peripheral)),
      mHwInitialized(false), mCurrentPin(0xFF) {
    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: created (injected peripheral)");
}

ChannelEngineFlexIO::~ChannelEngineFlexIO() {
    if (mPeripheral) {
        mPeripheral->deinit();
    }
    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: destroyed");
}

bool ChannelEngineFlexIO::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data || !data->isClockless()) {
        return false;
    }

    // Check timing range
    const u32 period = data->getTiming().total_period_ns();
    if (period < kFlexIOMinPeriodNs || period > kFlexIOMaxPeriodNs) {
        return false;
    }

    // Check if pin has FlexIO2 mapping
    if (!mPeripheral) {
        return false;
    }
    const u8 pin = static_cast<u8>(data->getPin());
    return mPeripheral->canHandlePin(pin);
}

void ChannelEngineFlexIO::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        channelData->setInUse(true);
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

void ChannelEngineFlexIO::show() FL_NO_EXCEPT {
    if (!mPeripheral) {
        return;
    }

    // Wait for any previous transmission (per DMA Wait Pattern). Bounded
    // wait so a stuck FlexIO from a previous show cannot hang subsequent
    // shows -- if poll never becomes READY, force-drop the previous channel
    // set and proceed. 100 ms is plenty for any reasonable WS2812 frame.
#if defined(FL_IS_TEENSY_4X)
    const u32 spin_start = millis();
    while (poll() != DriverState::READY) {
        if ((u32)(millis() - spin_start) >= 100) {
            // Previous transmission stuck -- force-clear so we can proceed.
            for (auto& ch : mTransmittingChannels) {
                ch->setInUse(false);
            }
            mTransmittingChannels.clear();
            break;
        }
    }
#else
    while (poll() != DriverState::READY) {
        // Spin until FlexIO DMA completes
    }
#endif

    // Move enqueued → transmitting
    mTransmittingChannels.swap(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    if (mTransmittingChannels.empty()) {
        return;
    }

    // FlexIO supports one strip at a time (4 timers + 1 shifter per strip).
    // Transmit each channel sequentially.
    for (auto& ch : mTransmittingChannels) {
        const u8 pin = static_cast<u8>(ch->getPin());
        const ChipsetTimingConfig& timing = ch->getTiming();

        // Check if we need to reinitialize hardware (pin or timing changed)
        if (!mHwInitialized || pin != mCurrentPin || timing != mCurrentTiming) {
            mPeripheral->deinit();

            if (!mPeripheral->canHandlePin(pin)) {
                FL_LOG_FLEXIO_F("ChannelEngineFlexIO: Pin %s not FlexIO2-capable", (int)pin);
                continue;
            }

            // T0H = t1_ns, T1H = t1_ns + t2_ns, period = t1+t2+t3
            u32 t0h = timing.t1_ns;
            u32 t1h = timing.t1_ns + timing.t2_ns;
            u32 period = timing.total_period_ns();

            if (!mPeripheral->init(pin, t0h, t1h, period, timing.reset_us)) {
                FL_LOG_FLEXIO_F("ChannelEngineFlexIO: Failed to init FlexIO for pin %s", (int)pin);
                continue;
            }

            mCurrentPin = pin;
            mCurrentTiming = timing;
            mHwInitialized = true;
        }

        // Transmit pixel data
        const auto& data = ch->getData();
        if (!data.empty()) {
            mPeripheral->show(data.data(), static_cast<u32>(data.size()));

            // Wait for this strip to complete before moving to next
            // (FlexIO can only do one strip at a time)
            mPeripheral->wait();
        }
    }

    // Clear isInUse flags
    for (auto& ch : mTransmittingChannels) {
        ch->setInUse(false);
    }
    mTransmittingChannels.clear();
}

IChannelDriver::DriverState ChannelEngineFlexIO::poll() FL_NO_EXCEPT {
    if (!mTransmittingChannels.empty()) {
        if (mPeripheral && mPeripheral->isDone()) {
            // Transfer complete — clean up
            for (auto& ch : mTransmittingChannels) {
                ch->setInUse(false);
            }
            mTransmittingChannels.clear();
            return DriverState::READY;
        }
        return DriverState::BUSY;
    }
    return DriverState::READY;
}

} // namespace fl

#endif // CHANNEL_ENGINE_FLEXIO_CPP_HPP_
