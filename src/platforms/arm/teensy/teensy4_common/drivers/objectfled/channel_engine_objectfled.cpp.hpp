// IWYU pragma: private

#ifndef CHANNEL_ENGINE_OBJECTFLED_CPP_HPP_
#define CHANNEL_ENGINE_OBJECTFLED_CPP_HPP_

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/iobjectfled_peripheral.h"

#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"
#include "fl/gfx/rectangular_draw_buffer.h"

namespace fl {

// Timing period bounds for canHandle() filtering.
// ObjectFLED uses 3:1 DMA bit transposition which only works with
// standard clockless protocols (WS2812, SK6812, etc).
static constexpr u32 kMinPeriodNs = 1000;
static constexpr u32 kMaxPeriodNs = 2500;

// ============================================================================
// TimingGroup: one ObjectFLED instance per unique timing configuration
// ============================================================================

struct TimingGroup {
    ChipsetTimingConfig timing;
    fl::vector<ChannelDataPtr> channels;
    RectangularDrawBuffer drawBuffer;
    fl::unique_ptr<IObjectFLEDInstance> instance;

    TimingGroup() = default;
    ~TimingGroup() = default;

    // Non-copyable
    TimingGroup(const TimingGroup&) = delete;
    TimingGroup& operator=(const TimingGroup&) = delete;
};

// ============================================================================
// ChannelEngineObjectFLED Implementation
// ============================================================================

ChannelEngineObjectFLED::ChannelEngineObjectFLED()
    : mPeripheral(IObjectFLEDPeripheral::create()) {
    FL_LOG_OBJECTFLED("ChannelEngineObjectFLED: created");
}

ChannelEngineObjectFLED::ChannelEngineObjectFLED(
        fl::shared_ptr<IObjectFLEDPeripheral> peripheral)
    : mPeripheral(fl::move(peripheral)) {
    FL_LOG_OBJECTFLED("ChannelEngineObjectFLED: created with injected peripheral");
}

ChannelEngineObjectFLED::~ChannelEngineObjectFLED() {
    FL_LOG_OBJECTFLED("ChannelEngineObjectFLED: destroyed");
}

bool ChannelEngineObjectFLED::canHandle(const ChannelDataPtr& data) const {
    if (!data || !data->isClockless()) {
        return false;
    }
    const u32 period = data->getTiming().total_period_ns();
    return period >= kMinPeriodNs && period <= kMaxPeriodNs;
}

void ChannelEngineObjectFLED::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        channelData->setInUse(true);
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

void ChannelEngineObjectFLED::show() {
    // Wait for any previous transmission (per DMA Wait Pattern)
    while (poll() != DriverState::READY) {
        // ObjectFLED show() is synchronous, so this should be instant
    }

    // Move enqueued -> transmitting
    mTransmittingChannels.swap(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    if (mTransmittingChannels.empty()) {
        return;
    }

    // ================================================================
    // Group channels by timing configuration, reusing persistent groups
    // ================================================================

    // Clear channel lists from previous frame but keep the groups
    for (auto& g : mTimingGroups) {
        g->channels.clear();
    }

    // Assign channels to existing groups or create new ones
    for (auto& ch : mTransmittingChannels) {
        const ChipsetTimingConfig& timing = ch->getTiming();
        TimingGroup* found = nullptr;
        for (auto& g : mTimingGroups) {
            if (g->timing == timing) {
                found = g.get();
                break;
            }
        }
        if (!found) {
            auto newGroup = fl::make_unique<TimingGroup>();
            newGroup->timing = timing;
            found = newGroup.get();
            mTimingGroups.push_back(fl::move(newGroup));
        }
        found->channels.push_back(ch);
    }

    // ================================================================
    // Transmit each timing group that has channels this frame
    // ================================================================
    for (auto& group : mTimingGroups) {
        if (group->channels.empty()) {
            continue;  // No channels this frame for this timing group
        }

        RectangularDrawBuffer& drawBuf = group->drawBuffer;
        drawBuf.onQueuingStart();

        // Queue all channels in this group
        for (auto& ch : group->channels) {
            const u8 pin = static_cast<u8>(ch->getPin());

            // Validate pin
            auto validation = mPeripheral->validatePin(pin);
            if (!validation.valid) {
                FL_LOG_OBJECTFLED("ChannelEngineObjectFLED: Pin " << (int)pin
                        << " invalid: " << validation.error_message);
                continue;
            }

            // Determine bytes per LED from data size and pin count
            const auto& data = ch->getData();
            const size_t dataSize = data.size();

            // Determine if RGBW: data size must be divisible by 4 but not 3,
            // or divisible by both but 4 gives an integer LED count matching 3's
            // Simple heuristic: if dataSize % 4 == 0 and dataSize % 3 != 0 -> RGBW
            // Otherwise RGB
            bool isRgbw = (dataSize % 4 == 0) && (dataSize % 3 != 0);
            int bytesPerLed = isRgbw ? 4 : 3;
            u16 numLeds = static_cast<u16>(dataSize / bytesPerLed);

            drawBuf.queue(DrawItem(pin, numLeds, isRgbw));
        }
        drawBuf.onQueuingDone();

        // Copy raw RGB bytes from ChannelData into the draw buffer
        for (auto& ch : group->channels) {
            const u8 pin = static_cast<u8>(ch->getPin());
            auto validation = mPeripheral->validatePin(pin);
            if (!validation.valid) {
                continue;
            }

            fl::span<u8> stripBytes = drawBuf.getLedsBufferBytesForPin(pin, true);
            const auto& srcData = ch->getData();
            const size_t copySize = (srcData.size() < stripBytes.size())
                                      ? srcData.size()
                                      : stripBytes.size();
            if (copySize > 0) {
                fl::memcpy(stripBytes.data(), srcData.data(), copySize);
            }
        }

        // Build/rebuild ObjectFLED instance only when draw list changes
        bool drawListChanged = drawBuf.mDrawListChangedThisFrame;
        if (drawListChanged || !group->instance) {
            group->instance.reset();

            // Build pin list
            fl::FixedVector<u8, 50> pinList;
            bool hasRgbw = false;
            for (const auto& item : drawBuf.mDrawList) {
                pinList.push_back(item.mPin);
                if (item.mIsRgbw) {
                    hasRgbw = true;
                }
            }

            if (pinList.empty()) {
                continue;  // All pins invalid, skip this group
            }

            u32 num_strips = 0;
            u32 bytes_per_strip = 0;
            u32 total_bytes = 0;
            drawBuf.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);

            int bytesPerLed = hasRgbw ? 4 : 3;
            int totalLeds = total_bytes / bytesPerLed;

            group->instance = mPeripheral->createInstance(
                totalLeds, hasRgbw, pinList.size(), pinList.data(),
                group->timing.t1_ns, group->timing.t2_ns,
                group->timing.t3_ns, group->timing.reset_us
            );
        }

        if (!group->instance) {
            continue;  // createInstance failed
        }

        // Copy draw buffer into instance's frame buffer
        u32 totalBytes = drawBuf.getTotalBytes();
        u32 frameBufferSize = group->instance->getFrameBufferSize();
        u32 copyBytes = totalBytes < frameBufferSize ? totalBytes : frameBufferSize;
        if (copyBytes > 0) {
            fl::memcpy(
                group->instance->getFrameBuffer(),
                drawBuf.mAllLedsBufferUint8.get(),
                copyBytes
            );
        }

        // Transmit! (synchronous - blocks until DMA completes)
        group->instance->show();
    }

    // Remove stale timing groups that had no channels this frame
    // (channels were destroyed, e.g., FastLED.clear(ClearFlags::CHANNELS))
    for (size_t i = mTimingGroups.size(); i > 0; --i) {
        if (mTimingGroups[i - 1]->channels.empty()) {
            mTimingGroups.erase(mTimingGroups.begin() + (i - 1));
        }
    }

    // Clear isInUse flags on all transmitted channels
    for (auto& ch : mTransmittingChannels) {
        ch->setInUse(false);
    }
    mTransmittingChannels.clear();
}

IChannelDriver::DriverState ChannelEngineObjectFLED::poll() {
    // ObjectFLED show() is synchronous - always READY after show() returns
    return DriverState::READY;
}

} // namespace fl

#endif // CHANNEL_ENGINE_OBJECTFLED_CPP_HPP_
