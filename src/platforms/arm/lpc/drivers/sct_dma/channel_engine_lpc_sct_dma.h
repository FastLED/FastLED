/// @file channel_engine_lpc_sct_dma.h
/// @brief LPC845 SCT + DMA0 clockless channel engine (channels-API form
///        of `clockless_arm_lpc_pwm_dma.h`).
///
/// Lifts the existing legacy-template DMA clockless driver onto
/// `IChannelDriver` so LPC845 can take part in runtime channels-API
/// dispatch (`addLeds<>` → `BusTraits<Bus::BIT_BANG>::instancePtr()` →
/// `ChannelManager::show`). Reference engine pattern:
/// `src/platforms/arm/teensy/teensy4_common/drivers/objectfled/`.
///
/// **Status:** scaffold + interface. The channels-API surface (canHandle,
/// enqueue, show, poll, capabilities) is in. The actual SCT-encode +
/// chunk-stream-kick body of `show()` is intentionally a documented
/// `TODO(3459)` that lifts the chunked encoder from
/// `clockless_arm_lpc_pwm_dma.h`. Same scaffold-then-bench pattern this
/// repo uses for `rx_sct_capture.h::begin()` (#3015) and the SPI DMA
/// driver from #3453 / PR #3454.
///
/// **Parallel-IO peripheral rule (`src/fl/channels/README.md`).**
/// LPC's SCT is parallel-IO-capable but does NOT drive SPI — the LPC845
/// SPI block is a separate peripheral (SPI0 @ 0x40058000, SPI1 @
/// 0x4005C000) with its own DMA engine landing under
/// `src/platforms/arm/lpc/spi_arm_lpc_dma.h` (PR #3454). This is the
/// rule's documented "different-peripheral-same-protocol" exception, so
/// `ChannelEngineLpcSctDma` returns `Capabilities(true, false)` —
/// clockless only — and the SPI DMA engine stays a sibling.
///
/// State machine: `READY → BUSY → DRAINING → READY`, same shape as
/// `ChannelEngineObjectFLED`.

#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

// Engine compiles on LPC845 (the target) AND on host/stub builds so the
// channels-API contract — canHandle filtering, state machine, capabilities,
// name — can be exercised by `tests/fl/channels/lpc_sct_dma_engine.cpp`
// without requiring silicon. Same gate convention as `ChannelEngineObjectFLED`
// which compiles under `FASTLED_STUB_IMPL` for its tests in
// `tests/platforms/arm/teensy/teensy4_common/drivers/objectfled/`.
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_STUB) || defined(FASTLED_STUB_IMPL)

#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.h"

namespace fl {

/// @brief LPC845 SCT + DMA0 clockless channel engine
///
/// Wraps the SCT match-event + 3-DMA-channel pulse-shape engine
/// established in `clockless_arm_lpc_pwm_dma.h` and exposes it through
/// the channels API. Strict chipset filtering: only handles clockless
/// chipsets in the standard WS2812 / SK6812 total-period range
/// (1000-2500 ns), matching `ChannelEngineObjectFLED`'s convention.
///
/// **Single-strip in v1.** Multi-strip parallel SCT output is the #2879
/// Stage 4.4 work and ships as a separate engine evolution (or
/// `BusInstanceCount` bump) after single-strip is bench-validated.
class ChannelEngineLpcSctDma : public IChannelDriver {
public:
    ChannelEngineLpcSctDma() FL_NO_EXCEPT;
    ~ChannelEngineLpcSctDma() override;

    /// @brief Filter channels to clockless WS2812-range chipsets.
    /// @return true for clockless chipsets with total bit period in
    ///         [1000, 2500] ns. Rejects SPI (handled by the sibling
    ///         `ARMHardwareSPIOutputDMA` driver) and HD chipsets like
    ///         WS2816 that need different encoding.
    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;

    /// @brief Store channel data for later show().
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;

    /// @brief Encode enqueued channels and kick the SCT-driven DMA stream.
    ///
    /// On LPC845 with `-DFASTLED_LPC_PWM_DMA=1`, picks the first enqueued
    /// channel, sets up the `LpcSctDmaTransmitter` for its pin + timing,
    /// and runs the chunk-stream encode/transmit loop. Single-strip only
    /// in v1 — multi-strip parallel output is the #2879 Stage 4.4
    /// follow-up. On host/stub builds the transmitter is a no-op so the
    /// state-machine flow remains valid for the host tests.
    void show() FL_NO_EXCEPT override;

    /// @brief Query engine state; returns BUSY/DRAINING while the SCT-driven
    ///        DMA stream is active, READY otherwise.
    DriverState poll() FL_NO_EXCEPT override;

    /// @brief Engine name for affinity binding / diagnostic logging.
    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("LPC_SCT_DMA");
    }

    /// @brief Clockless-only capabilities — LPC SCT does not drive SPI;
    ///        SPI is handled by the sibling spi_arm_lpc_dma.h engine.
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(/*clockless=*/true, /*spi=*/false);
    }

    /// @brief Access the underlying SCT+DMA transmitter.
    ///
    /// Test-facing accessor added for the #3468 TX→RX readback
    /// contract: on host the transmitter records the byte stream the
    /// engine handed it, so `tests/fl/channels/lpc_sct_dma_engine.cpp`
    /// can round-trip the exact bytes through the LPC RX device's
    /// decoder without bypassing the engine's `show()` path.
    LpcSctDmaTransmitter& transmitter() FL_NO_EXCEPT { return mTransmitter; }
    const LpcSctDmaTransmitter& transmitter() const FL_NO_EXCEPT { return mTransmitter; }

private:
    /// @brief Pending channels (set by enqueue, consumed by show).
    fl::vector<ChannelDataPtr> mEnqueuedChannels;

    /// @brief Currently transmitting channels (cleared when poll
    ///        observes DMA-done).
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    /// @brief Runtime SCT+DMA transmitter — owns the per-instance
    ///        encoding buffer + does the actual peripheral programming
    ///        on LPC845. Host-build methods are no-ops.
    ///
    /// Per-instance ownership of the encoding buffer closes one of the
    /// `TODO(2842)` markers from `clockless_arm_lpc_pwm_dma.h` (the
    /// shared-static `sChannelBuf`).
    LpcSctDmaTransmitter mTransmitter;

    /// @brief Transmission-in-flight flag toggled by show()/poll().
    bool mTransmissionActive = false;
};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL
