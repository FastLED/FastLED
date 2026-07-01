#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

// Same gate as the header — engine compiles on LPC845 (real target) AND on
// host/stub builds for test exercise. The peripheral access lives in the
// runtime helper (`lpc_sct_dma_runtime.cpp.hpp`) which is also host-safe.
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_STUB) || defined(FASTLED_STUB_IMPL)

#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h"
#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.h"
#include "fl/stl/noexcept.h"

namespace fl {

// =============================================================================
// Timing bounds for canHandle() filtering.
//
// Match ObjectFLED's WS2812/SK6812-range convention from
// channel_engine_objectfled.cpp.hpp (1000-2500 ns total bit period). Anything
// outside is either an HD chipset (WS2816 etc.) that needs separate encoding,
// or a non-clockless chipset.
//
// Prefixed to avoid clash with the matching constants in
// `channel_engine_objectfled.cpp.hpp` — both files end up in the same
// unity-build translation unit on host.
// =============================================================================
static constexpr u32 kLpcSctMinPeriodNs = 1000;
static constexpr u32 kLpcSctMaxPeriodNs = 2500;

ChannelEngineLpcSctDma::ChannelEngineLpcSctDma() FL_NO_EXCEPT = default;
ChannelEngineLpcSctDma::~ChannelEngineLpcSctDma() = default;

bool ChannelEngineLpcSctDma::canHandle(
        const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data) {
        return false;
    }
    if (!data->isClockless()) {
        return false;
    }
    const u32 period = data->getTiming().total_period_ns();
    return period >= kLpcSctMinPeriodNs && period <= kLpcSctMaxPeriodNs;
}

void ChannelEngineLpcSctDma::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (!channelData) {
        return;
    }
    mEnqueuedChannels.push_back(channelData);
}

void ChannelEngineLpcSctDma::show() FL_NO_EXCEPT {
    // Empty queue → nothing to do, state stays READY.
    if (mEnqueuedChannels.empty()) {
        return;
    }

    // Hand the queue to the transmitting slot so poll() can clear it
    // when the DMA completes.
    mTransmittingChannels = mEnqueuedChannels;
    mEnqueuedChannels.clear();
    mTransmissionActive = true;

    // Single-strip v1: take the first entry. Multi-strip (#2879 Stage 4.4)
    // is a separate engine evolution; the parallel SCT topology needs the
    // shared timebase + per-bit OR'd masks discussed in the
    // clockless_arm_lpc_pwm_dma.h header's MULTI-CHAIN SUPPORT block.
    const ChannelDataPtr& ch = mTransmittingChannels[0];
    if (!ch) {
        return;
    }
    const ChipsetTimingConfig& t = ch->getTiming();
    const int pin = ch->getPin();
    if (pin < 0 || pin > 31) {
        // LPC845 GPIO port 0 holds PIO0_0..PIO0_31. Out-of-range pins
        // would index a bit outside the port mask — refuse silently
        // rather than corrupting other channels. (The legacy template
        // path enforces this via FastPin<DATA_PIN> at compile time.)
        return;
    }
    const fl::vector_psram<u8>& bytes = ch->getData();
    if (bytes.empty()) {
        return;
    }

    // Configure the runtime SCT/DMA helper for this channel's pin +
    // timing, then run the chunk-stream transmit loop. On host this is a
    // no-op so the engine's state machine still settles in poll().
    mTransmitter.configureForChannel(
        static_cast<u8>(pin),
        t.t1_ns, t.t2_ns, t.t3_ns);
    mTransmitter.transmit(bytes.data(), static_cast<u32>(bytes.size()),
                          ch->getPixelFormat() == ChannelPixelFormat::RGBW);
}

IChannelDriver::DriverState ChannelEngineLpcSctDma::poll() FL_NO_EXCEPT {
    if (!mTransmissionActive) {
        return DriverState::READY;
    }
    // Drive chunk progression via the runtime transmitter. On LPC845
    // this probes `DMA0->COMMON[0].ACTIVE` for the three SCT-tied
    // channels — when they drop, if bytes remain the next chunk gets
    // encoded and kicked in this same call. #3459 acceptance
    // criterion 3: no busy-wait in show(); the state machine
    // transitions READY→BUSY→DRAINING→READY are all driven from
    // here. On host the transmit was synchronous so pollAndAdvance()
    // immediately returns true.
    if (!mTransmitter.pollAndAdvance()) {
        return DriverState::DRAINING;
    }
    mTransmissionActive = false;
    mTransmittingChannels.clear();
    return DriverState::READY;
}

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL
