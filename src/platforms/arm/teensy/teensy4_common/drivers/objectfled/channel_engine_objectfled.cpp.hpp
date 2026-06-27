// IWYU pragma: private

#ifndef CHANNEL_ENGINE_OBJECTFLED_CPP_HPP_
#define CHANNEL_ENGINE_OBJECTFLED_CPP_HPP_

// #3416 OF-LOW-6: ChannelEngineObjectFLED serialises multi-timing-group
// frames with an implicit ~LATCH_DELAY (300 us) idle between groups.
// poll() services one group at a time; if poll() isn't called quickly
// enough between groups, the second group waits indefinitely (singleton
// DMA manager's acquire() blocks). This is by design but worth noting
// for users who yield to other code in their show() loop.

#include "platforms/arm/teensy/is_teensy.h"

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/iobjectfled_peripheral.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_spi_mode.h"
#include "platforms/arm/teensy/teensy4_common/clockless_objectfled.h"

// #3416 FX-LOW-3 mirror: duplicate include removed.
#include "fl/log/log.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

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
    ChannelPixelFormat pixelFormat = ChannelPixelFormat::RGB;
    fl::vector<ChannelDataPtr> channels;
    fl::unique_ptr<IObjectFLEDInstance> instance;
    fl::FixedVector<u8, 50> pinList;
    u32 bytesPerStrip = 0;
    bool isRgbw = false;

    TimingGroup() = default;
    ~TimingGroup() = default;

    // Non-copyable
    TimingGroup(const TimingGroup&) = delete;
    TimingGroup& operator=(const TimingGroup&) = delete;
};

static bool objectFledSupportsPixelFormat(ChannelPixelFormat format) FL_NO_EXCEPT {
    return format == ChannelPixelFormat::RGB ||
           format == ChannelPixelFormat::RGBW;
}

static bool pinsEqual(const fl::FixedVector<u8, 50>& a,
                      const fl::FixedVector<u8, 50>& b) FL_NO_EXCEPT {
    if (a.size() != b.size()) {
        return false;
    }
    for (fl::size i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// ChannelEngineObjectFLED Implementation
// ============================================================================

ChannelEngineObjectFLED::ChannelEngineObjectFLED()
 FL_NO_EXCEPT : mPeripheral(IObjectFLEDPeripheral::create()) {
    FL_LOG_OBJECTFLED_F("ChannelEngineObjectFLED: created");
}

ChannelEngineObjectFLED::ChannelEngineObjectFLED(
        fl::shared_ptr<IObjectFLEDPeripheral> peripheral)
 FL_NO_EXCEPT : mPeripheral(fl::move(peripheral)) {
    FL_LOG_OBJECTFLED_F("ChannelEngineObjectFLED: created with injected peripheral");
}

ChannelEngineObjectFLED::~ChannelEngineObjectFLED() {
    FL_LOG_OBJECTFLED_F("ChannelEngineObjectFLED: destroyed");
}

bool ChannelEngineObjectFLED::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data) {
        return false;
    }

    if (data->isClockless()) {
        if (!objectFledSupportsPixelFormat(data->getPixelFormat())) {
            return false;
        }
        const u32 period = data->getTiming().total_period_ns();
        return period >= kMinPeriodNs && period <= kMaxPeriodNs;
    }

#if defined(FL_IS_TEENSY_4X) && defined(FL_OBJECTFLED_SPI_HARDWARE_ENABLE)
    if (data->isSpi()) {
        // #3428: SPI-mode dispatch on the SAME peripheral (unified engine).
        // The (MOSI, SCLK) pin pair must both be GPIO6-resident (so one
        // DMA write to GPIO6_DR can flip both bits together).
        //
        // This branch is gated by FL_OBJECTFLED_SPI_HARDWARE_ENABLE -- the
        // ObjectFLED-SPI hardware bring-up is incomplete (the FlexPWM2
        // register sequence hard-faults on first show; needs scope debug).
        // Until then, canHandle returns false for SPI and channels fall
        // through to the next bus (e.g. FlexIO-SPI from PR #3431, or a
        // user-explicit LPSPI selection). The architectural slot is in
        // place via getCapabilities()+BusSupports so flipping the flag is
        // a one-line enable once bring-up is verified.
        const auto* spi = data->getChipset().ptr<SpiChipsetConfig>();
        if (!spi || spi->dataPin < 0 || spi->clockPin < 0) {
            return false;
        }
        ObjectFLEDSPIPinInfo info;
        return objectfled_spi_lookup_pins(static_cast<u8>(spi->dataPin),
                                          static_cast<u8>(spi->clockPin), &info);
    }
#endif

    return false;
}

void ChannelEngineObjectFLED::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        channelData->setInUse(true);
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

void ChannelEngineObjectFLED::show() FL_NO_EXCEPT {
    if (mEnqueuedChannels.empty()) {
        return;
    }

    waitForReady();

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
        const ChannelPixelFormat pixelFormat = ch->getPixelFormat();
        TimingGroup* found = nullptr;
        for (auto& g : mTimingGroups) {
            if (g->timing == timing && g->pixelFormat == pixelFormat) {
                found = g.get();
                break;
            }
        }
        if (!found) {
            auto newGroup = fl::make_unique<TimingGroup>();
            newGroup->timing = timing;
            newGroup->pixelFormat = pixelFormat;
            found = newGroup.get();
            mTimingGroups.push_back(fl::move(newGroup));
        }
        found->channels.push_back(ch);
    }

    mCurrentGroupIndex = 0;
    if (!startNextTimingGroup()) {
        finishTransmission();
    }
}

bool ChannelEngineObjectFLED::startNextTimingGroup() FL_NO_EXCEPT {
    while (mCurrentGroupIndex < mTimingGroups.size()) {
        TimingGroup& group = *mTimingGroups[mCurrentGroupIndex];
        if (group.channels.empty()) {
            ++mCurrentGroupIndex;
            continue;
        }
        if (startTimingGroup(group)) {
            return true;
        }
        ++mCurrentGroupIndex;
    }
    return false;
}

// autoresearch-runtime-output-lint: begin
bool ChannelEngineObjectFLED::startTimingGroup(TimingGroup& group) FL_NO_EXCEPT {
    if (!objectFledSupportsPixelFormat(group.pixelFormat)) {
        return false;
    }

    const bool isRgbw = group.pixelFormat == ChannelPixelFormat::RGBW;
    const u32 bytesPerLed = objectFledBytesPerLed(isRgbw);
    fl::FixedVector<u8, 50> pinList;
    fl::vector<ChannelDataPtr> validChannels;
    u32 maxDataBytes = 0;

    // Collect valid channels and compute the raw rectangular byte stride.
    for (auto& ch : group.channels) {
        const u8 pin = static_cast<u8>(ch->getPin());

        // Validate pin
        auto validation = mPeripheral->validatePin(pin);
        if (!validation.valid) {
            FL_LOG_OBJECTFLED_F("ChannelEngineObjectFLED: Pin %s invalid: %s", (int)pin, validation.error_message);
            continue;
        }

        const auto& data = ch->getData();
        const size_t dataSize = data.size();
        if (dataSize > maxDataBytes) {
            maxDataBytes = static_cast<u32>(dataSize);
        }
        if (pinList.size() < pinList.capacity()) {
            pinList.push_back(pin);
            validChannels.push_back(ch);
        }
    }

    if (pinList.empty()) {
        return false;
    }

    const u32 ledsPerStrip =
            objectFledLedsPerStripForRectangularBytes(maxDataBytes, isRgbw);
    const u32 bytesPerStrip = ledsPerStrip * bytesPerLed;
    const bool layoutChanged =
            !group.instance ||
            group.bytesPerStrip != bytesPerStrip ||
            group.isRgbw != isRgbw ||
            !pinsEqual(group.pinList, pinList);

    if (layoutChanged) {
        group.instance.reset();

        int totalLeds = static_cast<int>(
            ledsPerStrip * static_cast<u32>(pinList.size()));

        group.instance = mPeripheral->createInstance(
            totalLeds, isRgbw, pinList.size(), pinList.data(),
            group.timing.t1_ns, group.timing.t2_ns,
            group.timing.t3_ns, group.timing.reset_us
        );
        if (group.instance) {
            group.pinList = pinList;
            group.bytesPerStrip = bytesPerStrip;
            group.isRgbw = isRgbw;
        }
    }

    if (!group.instance) {
        return false;  // createInstance failed
    }

    u32 frameBufferSize = group.instance->getFrameBufferSize();
    u8* frameBuffer = group.instance->getFrameBuffer();
    if (frameBuffer && frameBufferSize > 0) {
        fl::memset(frameBuffer, 0, frameBufferSize);
        for (fl::size i = 0; i < validChannels.size(); i++) {
            const auto& srcData = validChannels[i]->getData();
            const u32 dstOffset = static_cast<u32>(i) * bytesPerStrip;
            if (dstOffset >= frameBufferSize) {
                continue;
            }
            const u32 dstRemaining = frameBufferSize - dstOffset;
            const u32 copyBytes =
                    static_cast<u32>(srcData.size()) < dstRemaining
                            ? static_cast<u32>(srcData.size())
                            : dstRemaining;
            if (copyBytes > 0) {
                fl::memcpy(frameBuffer + dstOffset, srcData.data(), copyBytes);
            }
        }
    }

    group.instance->show();
    return true;
}
// autoresearch-runtime-output-lint: end

IChannelDriver::DriverState ChannelEngineObjectFLED::poll() FL_NO_EXCEPT {
    if (mTransmittingChannels.empty()) {
        return DriverState::READY;
    }

    while (mCurrentGroupIndex < mTimingGroups.size()) {
        TimingGroup& group = *mTimingGroups[mCurrentGroupIndex];
        if (group.channels.empty() || !group.instance) {
            ++mCurrentGroupIndex;
            continue;
        }

        if (group.instance->isBusy()) {
            return DriverState::DRAINING;
        }

        ++mCurrentGroupIndex;
        if (startNextTimingGroup()) {
            return DriverState::BUSY;
        }
    }

    finishTransmission();
    return DriverState::READY;
}

void ChannelEngineObjectFLED::finishTransmission() FL_NO_EXCEPT {
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
    mCurrentGroupIndex = 0;
}

} // namespace fl

#endif // CHANNEL_ENGINE_OBJECTFLED_CPP_HPP_
