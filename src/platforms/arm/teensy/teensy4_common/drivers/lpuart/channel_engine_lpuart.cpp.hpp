// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"
#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/channel_engine_lpuart.h"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/bus_traits.h"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/ilpuart_peripheral.h"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/lpuart_encoder.h"

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"


// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep


namespace fl {

static constexpr u32 kLpuartMinPeriodNs = 1000;
static constexpr u32 kLpuartMaxPeriodNs = 2500;

ChannelEngineLPUART::ChannelEngineLPUART() FL_NO_EXCEPT
    : mPeripheral(ILPUARTPeripheral::create()),
      mHwInitialized(false), mCurrentPin(0xFF) {}

ChannelEngineLPUART::ChannelEngineLPUART(fl::shared_ptr<ILPUARTPeripheral> peripheral) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)),
      mHwInitialized(false), mCurrentPin(0xFF) {}

ChannelEngineLPUART::~ChannelEngineLPUART() {}

fl::string ChannelEngineLPUART::getName() const FL_NO_EXCEPT {
    return fl::string::from_literal("LPUART");
}

IChannelDriver::Capabilities ChannelEngineLPUART::getCapabilities() const FL_NO_EXCEPT {
    return Capabilities(true, false);
}

bool ChannelEngineLPUART::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data || !data->isClockless()) return false;
    const u32 period = data->getTiming().total_period_ns();
    if (period < kLpuartMinPeriodNs || period > kLpuartMaxPeriodNs) return false;
    if (!mPeripheral) return false;
    const u8 pin = static_cast<u8>(data->getPin());
    return mPeripheral->validatePin(pin).valid;
}

void ChannelEngineLPUART::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        channelData->setInUse(true);
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

// Member-owned instance and current-binding state (#3023 / LPUART
// goal): mirrors ChannelEngineFlexIO's mHwInitialized + mCurrentPin
// + mCurrentTiming triple. Replaces the previous function-local
// static CachedInstance which made cross-pattern teardown awkward.
//
// `mInstance` is the live ILPUARTInstance bound to the current pin /
// strip-length. It is reset to nullptr when:
//   - the pin changes, or
//   - the strip length (raw_bytes) changes, or
//   - the engine is reconstructed.
// `mCurrentRawBytes` caches the raw-byte length so we can detect a
// size change without re-calling createInstance().

void ChannelEngineLPUART::show() FL_NO_EXCEPT {
    if (!mPeripheral) return;

    // Wait for any previous transmission. Bounded spin so a stuck
    // peripheral can't hang subsequent shows -- if poll never becomes
    // READY, force-clear and proceed (mirrors ChannelEngineFlexIO).
    const u32 spin_start = millis();
    while (poll() != DriverState::READY) {
        if ((u32)(millis() - spin_start) >= 100) {
            for (auto& ch : mTransmittingChannels) ch->setInUse(false);
            mTransmittingChannels.clear();
            break;
        }
    }

    mTransmittingChannels.swap(mEnqueuedChannels);
    mEnqueuedChannels.clear();
    if (mTransmittingChannels.empty()) return;

    for (auto& ch : mTransmittingChannels) {
        const u8 pin = static_cast<u8>(ch->getPin());
        const ChipsetTimingConfig& timing = ch->getTiming();
        const auto& data = ch->getData();
        if (data.empty()) continue;

        const u32 raw_bytes = static_cast<u32>(data.size());

        // Reinit if pin / strip size / timing changed.
        if (!mHwInitialized || pin != mCurrentPin
                || timing != mCurrentTiming
                || raw_bytes != mCurrentRawBytes) {
            // CodeRabbit-flagged: clear hardware-binding state BEFORE
            // attempting createInstance(). If create fails we leave
            // mHwInitialized=false so the next channel forces a fresh
            // reinit instead of inheriting the half-torn-down state.
            mInstance.reset();
            mHwInitialized = false;
            mCurrentPin = 0xFF;
            mCurrentRawBytes = 0;

            const u32 total_leds = (raw_bytes + 2u) / 3u;  // ceil for RGB
            auto candidate = mPeripheral->createInstance(
                pin, total_leds, /*is_rgbw=*/false,
                timing.t1_ns, timing.t2_ns, timing.t3_ns, timing.reset_us);
            if (!candidate) {
                FL_LOG_FLEXIO_F("ChannelEngineLPUART: createInstance failed on pin %d", (int)pin);
                continue;
            }
            mInstance = fl::move(candidate);
            mCurrentPin = pin;
            mCurrentTiming = timing;
            mCurrentRawBytes = raw_bytes;
            mHwInitialized = true;
        }

        // Pre-encode raw WS2812 bytes -> 4 wave8 UART bytes each into
        // the instance's TX buffer (per ILPUARTInstance contract).
        u8* dst = mInstance->getTxBuffer();
        const u32 dst_size = mInstance->getTxBufferSize();
        const u32 encoded_bytes = raw_bytes * 4u;
        const u32 max_raw_safe = (encoded_bytes < dst_size ? encoded_bytes : dst_size) / 4u;
        for (u32 i = 0; i < max_raw_safe; ++i) {
            lpuart_encode_byte(data.data()[i], fl::span<u8, 4>{&dst[i * 4u], 4});
        }

        mInstance->show();  // blocking inside the real driver
    }

    for (auto& ch : mTransmittingChannels) ch->setInUse(false);
    mTransmittingChannels.clear();
    yield();
}

IChannelDriver::DriverState ChannelEngineLPUART::poll() FL_NO_EXCEPT {
    if (!mTransmittingChannels.empty()) {
        // show() is fully blocking inside the real LPUART driver
        // (DMA major-loop ISR + LPUART STAT.TC wait), so by the time
        // any external poll() runs, transmission is complete.
        for (auto& ch : mTransmittingChannels) ch->setInUse(false);
        mTransmittingChannels.clear();
    }
    return DriverState::READY;
}

// CodeRabbit: move BusTraits<Bus::UART>::instancePtr() out of the
// header. Static storage lives here in the TU.
#if defined(FL_IS_TEENSY_4X)
fl::shared_ptr<ChannelEngineLPUART> BusTraits<Bus::UART>::instancePtr() FL_NO_EXCEPT {
    static fl::shared_ptr<ChannelEngineLPUART> gHolder =
        fl::make_shared<ChannelEngineLPUART>();
    return gHolder;
}
#endif

} // namespace fl

#endif // FL_IS_TEENSY_4X
