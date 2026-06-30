// IWYU pragma: private

#ifndef CHANNEL_ENGINE_LPC_SCT_DMA_CPP_HPP_
#define CHANNEL_ENGINE_LPC_SCT_DMA_CPP_HPP_

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

// Same gate as the header — engine compiles on LPC845 (real target) AND on
// host/stub builds for test exercise. The scaffold's show()/poll() do not
// touch any peripheral registers; the real SCT/DMA work is deferred per
// TODO(3459).
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_STUB) || defined(FASTLED_STUB_IMPL)

#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h"
#include "fl/stl/noexcept.h"

namespace fl {

// =============================================================================
// Timing bounds for canHandle() filtering.
//
// Match ObjectFLED's WS2812/SK6812-range convention from
// channel_engine_objectfled.cpp.hpp (1000-2500 ns total bit period). Anything
// outside is either an HD chipset (WS2816 etc.) that needs separate encoding,
// or a non-clockless chipset.
// =============================================================================
// Prefixed to avoid clash with the matching constants in
// `channel_engine_objectfled.cpp.hpp` — both files end up in the same
// unity-build translation unit on host.
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
    // TODO(3459): lift the chunk-encode + 3-channel DMA stream kick from
    // `clockless_arm_lpc_pwm_dma.h::showRGBInternal()`. The legacy template
    // engine works compile-time off `TIMING::T1/T2/T3`; the channels-API
    // engine here needs to take those values from
    // `ChannelDataPtr::getTiming()` and program SCT->MATCH at runtime.
    //
    // The structural moves required:
    //   1. Lift `LpcSctTicks<NS>::value` from a constexpr template to a
    //      runtime helper `lpc_sct_ticks(u32 ns)`.
    //   2. Lift `configureSct()` / `configureDma()` / `startDmaChunk()` /
    //      `waitDmaChunk()` from the templated `ClocklessController` into
    //      a non-template helper struct owned by this engine.
    //   3. Iterate `mEnqueuedChannels`, dispatching each through the
    //      lifted helper. v1 single-strip just takes the first entry;
    //      multi-strip parallel output is #2879.
    //
    // For the scaffold, transition into BUSY so the state machine looks
    // alive but do not actually program SCT/DMA. `poll()` will immediately
    // flip back to READY. This is the same scaffold convention as
    // `rx_sct_capture.h::begin()` (#3015) and the SPI DMA driver from
    // #3454 — channels-API surface in, peripheral wiring in a follow-up.
    if (mEnqueuedChannels.empty()) {
        return;
    }

    mTransmittingChannels = mEnqueuedChannels;
    mEnqueuedChannels.clear();
    mTransmissionActive = true;
}

IChannelDriver::DriverState ChannelEngineLpcSctDma::poll() FL_NO_EXCEPT {
    if (!mTransmissionActive) {
        return DriverState::READY;
    }

    // TODO(3459): probe the real DMA0 channel ACTIVE flag for the three
    // SCT-tied channels, matching the existing busy-wait in
    // `clockless_arm_lpc_pwm_dma.h::waitDmaChunk()`:
    //
    //   const u32 mask = (7UL << FASTLED_LPC_PWM_DMA_BASECH);
    //   return (DMA0->COMMON[0].ACTIVE & mask) != 0
    //              ? DriverState::DRAINING
    //              : DriverState::READY;
    //
    // For the v1 scaffold the show() body is a no-op, so flip to READY
    // immediately. Real DMA-driven implementation will follow the
    // BUSY → DRAINING → READY shape per `IChannelDriver`'s contract.
    mTransmissionActive = false;
    mTransmittingChannels.clear();
    return DriverState::READY;
}

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL

#endif  // CHANNEL_ENGINE_LPC_SCT_DMA_CPP_HPP_
