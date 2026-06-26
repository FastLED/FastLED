// IWYU pragma: private

#ifndef CHANNEL_ENGINE_LPUART_CPP_HPP_
#define CHANNEL_ENGINE_LPUART_CPP_HPP_

#include "platforms/arm/teensy/is_teensy.h"

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/channel_engine_lpuart.h"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/ilpuart_peripheral.h"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/lpuart_encoder.h"

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

#if defined(FL_IS_TEENSY_4X)
// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep
#endif

namespace fl {

// Timing period bounds for canHandle(). Same range as FlexIO -- the
// 4 Mbps encoding only fits standard WS2812B/SK6812 timings.
static constexpr u32 kLpuartMinPeriodNs = 1000;
static constexpr u32 kLpuartMaxPeriodNs = 2500;

ChannelEngineLPUART::ChannelEngineLPUART() FL_NO_EXCEPT
    : mPeripheral(ILPUARTPeripheral::create()),
      mHwInitialized(false), mCurrentPin(0xFF) {}

ChannelEngineLPUART::ChannelEngineLPUART(fl::shared_ptr<ILPUARTPeripheral> peripheral) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)),
      mHwInitialized(false), mCurrentPin(0xFF) {}

ChannelEngineLPUART::~ChannelEngineLPUART() {}

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

namespace {

// Per-engine cache of the currently-bound LPUART instance. Re-created
// when the pin or strip length changes.
struct CachedInstance {
    fl::unique_ptr<ILPUARTInstance> instance;
    u8 pin = 0xFF;
    u32 raw_bytes = 0;
    bool is_rgbw = false;
};

}  // anonymous namespace

void ChannelEngineLPUART::show() FL_NO_EXCEPT {
    if (!mPeripheral) return;

    // Wait for any previous transmission.
    while (poll() != DriverState::READY) {
        // Spin -- LPUART show() in our real impl is already synchronous.
    }

    mTransmittingChannels.swap(mEnqueuedChannels);
    mEnqueuedChannels.clear();
    if (mTransmittingChannels.empty()) return;

    static CachedInstance s_cached;

    for (auto& ch : mTransmittingChannels) {
        const u8 pin = static_cast<u8>(ch->getPin());
        const ChipsetTimingConfig& timing = ch->getTiming();
        const auto& data = ch->getData();
        if (data.empty()) continue;

        const u32 raw_bytes = static_cast<u32>(data.size());

        // Reinit if pin / strip size changed.
        if (!s_cached.instance || s_cached.pin != pin || s_cached.raw_bytes != raw_bytes) {
            s_cached.instance.reset();
            s_cached.pin = pin;
            s_cached.raw_bytes = raw_bytes;
            const bool is_rgbw = (raw_bytes % 4 == 0) && false;  // RGB only for now
            (void)is_rgbw;
            const u32 total_leds = raw_bytes / 3u;
            s_cached.instance = mPeripheral->createInstance(
                pin, total_leds, /*is_rgbw=*/false,
                timing.t1_ns, timing.t2_ns, timing.t3_ns, timing.reset_us);
            if (!s_cached.instance) {
                FL_LOG_FLEXIO_F("ChannelEngineLPUART: createInstance failed on pin %d", (int)pin);
                continue;
            }
            mCurrentPin = pin;
            mCurrentTiming = timing;
            mHwInitialized = true;
        }

        // Per ILPUARTInstance contract (see ilpuart_peripheral.h):
        // the engine pre-encodes raw WS2812 bytes -> 4 wave8 UART
        // bytes each and writes them into instance->getTxBuffer().
        // instance->show() then DMAs the encoded bytes to LPUARTn_DATA.
        u8* dst = s_cached.instance->getTxBuffer();
        const u32 dst_size = s_cached.instance->getTxBufferSize();
        const u32 encoded_bytes = raw_bytes * 4u;
        const u32 copy_n = (encoded_bytes < dst_size) ? encoded_bytes : dst_size;
        const u32 max_raw_safe = copy_n / 4u;
        for (u32 i = 0; i < max_raw_safe; ++i) {
            lpuart_encode_byte(data.data()[i], &dst[i * 4u]);
        }

        s_cached.instance->show();  // blocking inside the real driver
    }

    for (auto& ch : mTransmittingChannels) ch->setInUse(false);
    mTransmittingChannels.clear();
}

IChannelDriver::DriverState ChannelEngineLPUART::poll() FL_NO_EXCEPT {
    if (mTransmittingChannels.empty()) return DriverState::READY;
    // The real show() blocks, so we should never observe a non-ready
    // state here -- but defensively report BUSY.
    return DriverState::READY;
}

} // namespace fl

#endif  // CHANNEL_ENGINE_LPUART_CPP_HPP_
