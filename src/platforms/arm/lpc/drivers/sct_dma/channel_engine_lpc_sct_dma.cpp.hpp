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
#include "fl/log/log.h"
#include "fl/stl/chrono.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/lpc/lpc_dma_isr.h"

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

// How long show() waits for the previous frame before cutting it off. A
// frame always ends on its own (halt step, early halt, or DMA error), so
// this only guards against a wedged peripheral; 1000 WS2812 LEDs take 30 ms.
static constexpr u32 kLpcSctShowWaitMs = 100;

// Without the DMA ISR hub (FASTLED_LPC_DMA_ISR=0) only poll() hands chunks
// to the DMA, and nothing polls between FastLED.show() calls, so a frame
// longer than the two preloaded chunks would halt after them: 192 of 240
// bits for 10 WS2812 LEDs on silicon (#5278). Such frames are refused
// instead of cut short. LPC845 only: host builds have no DMA timing.
#if defined(FL_IS_ARM_LPC_845) && !FASTLED_LPC_DMA_ISR
static constexpr u32 kLpcSctMaxFrameBits = 2u * FASTLED_LPC_PWM_DMA_CHUNK_BITS;
#endif

// Whether `data` fits what this build can send without cutting it short.
static bool lpcSctFrameFits(const ChannelDataPtr& data) FL_NO_EXCEPT {
#if defined(FL_IS_ARM_LPC_845) && !FASTLED_LPC_DMA_ISR
    return data->getSize() * 8u <= kLpcSctMaxFrameBits;
#else
    (void)data;
    return true;
#endif
}

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
    const ChipsetTimingConfig& t = data->getTiming();
    const u32 period = t.total_period_ns();
    return period >= kLpcSctMinPeriodNs && period <= kLpcSctMaxPeriodNs &&
           LpcSctDmaTransmitter::timingFits(t.t1_ns, t.t2_ns, t.t3_ns) &&
           lpcSctFrameFits(data);
}

void ChannelEngineLpcSctDma::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (!channelData) {
        return;
    }
    mEnqueuedChannels.push_back(channelData);
}

void ChannelEngineLpcSctDma::releaseTransmitting() FL_NO_EXCEPT {
    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(false);
        }
    }
    mTransmittingChannels.clear();
    mTransmissionActive = false;
}

void ChannelEngineLpcSctDma::show() FL_NO_EXCEPT {
    // Empty queue → nothing to do, state stays READY.
    if (mEnqueuedChannels.empty()) {
        return;
    }

    // A frame still on the wire reads its bytes from mTransmittingChannels:
    // let it finish. poll() also does the chunk refill when no DMA ISR
    // does, so it is called back to back with no yield in between. Bounded
    // so a wedged peripheral can't hang show().
    const u32 wait_start = fl::millis();
    while (poll() != DriverState::READY) {
        if (fl::millis() - wait_start >= kLpcSctShowWaitMs) {
            mTransmitter.abort();
            releaseTransmitting();
            break;
        }
    }
    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    // Single-strip v1: take the first entry. Multi-strip (#2879 Stage 4.4)
    // is a separate engine evolution; the parallel SCT topology needs the
    // shared timebase + per-bit OR'd masks discussed in the
    // clockless_arm_lpc_pwm_dma.h header's MULTI-CHAIN SUPPORT block.
    const ChannelDataPtr& ch = mTransmittingChannels[0];
    // The bridge enqueues without canHandle(); timing outside the SCT
    // event layout would put EV2 past the counter limit, and the frame
    // would never reach its halt step. Drop such a frame before arming.
    if (!canHandle(ch)) {
        static bool sReportedTooLong = false;   // once, not every frame
        if (ch && !sReportedTooLong && !lpcSctFrameFits(ch)) {
            sReportedTooLong = true;
            FL_ERROR("LPC SCT+DMA: frame dropped, "
                     << ch->getSize() * 8u << " bits exceeds the "
                     << 2u * FASTLED_LPC_PWM_DMA_CHUNK_BITS
                     << "-bit limit without the DMA interrupt hub; "
                        "build with FASTLED_LPC_DMA_ISR=1");
        }
        releaseTransmitting();
        return;
    }
    const ChipsetTimingConfig& t = ch->getTiming();
    const int pin = ch->getPin();
    if (pin < 0 || pin > 31) {
        // The pin becomes an SWM PINASSIGN byte for SCT_OUT0; only
        // PIO0_0..PIO0_31 are valid there, and any other value would
        // misroute or unroute the output.
        releaseTransmitting();
        return;
    }
    const fl::vector_psram<u8>& bytes = ch->getData();
    if (bytes.empty()) {
        releaseTransmitting();
        return;
    }

    // The DMA reads these bytes until poll() sees the frame end; the bridge
    // waits on isInUse() before re-encoding into the same ChannelData.
    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(true);
        }
    }
    mTransmissionActive = true;

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
    // On LPC845 the DMA interrupt refills chunks; this reports the frame
    // done once the SCT has halted after the last bit (and refills a
    // chunk itself in builds without the DMA ISR hub). #3459 acceptance
    // criterion 3: no busy-wait in show(); the state machine
    // transitions READY→BUSY→DRAINING→READY are all driven from
    // here. On host the transmit was synchronous so pollAndAdvance()
    // immediately returns true.
    if (!mTransmitter.pollAndAdvance()) {
        return DriverState::DRAINING;
    }
    releaseTransmitting();
    return DriverState::READY;
}

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL
