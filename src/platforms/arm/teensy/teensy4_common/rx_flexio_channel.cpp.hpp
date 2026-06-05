/// @file rx_flexio_channel.cpp.hpp
/// @brief Teensy 4.x FlexIO shifter-based RX implementation skeleton
///
/// **Phase 1A — skeleton only** (FastLED#2764). This file lands the C++ class
/// structure, the factory, and the full `RxDevice` method surface so that:
///
///   1. The `RxBackend::FLEXIO` dispatcher path links cleanly on Teensy 4.
///   2. The AutoResearch JSON-RPC harness can already reference the backend
///      (`flexioRxBenchmark`, `runSingleTest` selection) without waiting for
///      the hardware programming to land.
///   3. Host-stub builds compile and unit tests can construct the object,
///      ensuring the public API surface is stable before Phase 1B touches the
///      FLEXIO1 registers.
///
/// Every method currently returns a safe placeholder (failure for `begin`,
/// empty / `WAIT_FAILED` for the data path methods). Calling code that
/// inspects return values will correctly treat the channel as inactive.
///
/// **Phase 1B (next PR in the #2764 series) wires up:**
///   - FLEXIO1 pin-mapping table (Teensy GPIO → FLEXIO1 pin index via IOMUXC)
///   - 1 shifter in RX mode + 1 timer in dual-8-bit-counter / edge-capture mode
///   - eDMA channel + DMAMUX source plumbing
///   - The actual capture buffer → edge-time decoder
///
/// Phase 1B will keep the public API identical, so consumers of this header
/// don't need to change.

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"
#include "platforms/arm/teensy/teensy4_common/rx_flexio_channel.h"

#include "fl/log/log.h"
#include "fl/stl/result.h"

namespace fl {

namespace {

// ---------------------------------------------------------------------------
// FlexIO RX implementation (Phase 1A skeleton)
// ---------------------------------------------------------------------------
class FlexIoRxChannelImpl : public FlexIoRxChannel {
  public:
    explicit FlexIoRxChannelImpl(int pin) : mPin(pin) {}
    ~FlexIoRxChannelImpl() override = default;

    bool begin(const RxConfig &config) override;
    bool finished() const override;
    RxWaitResult wait(u32 timeout_ms) override;
    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                        fl::span<u8> out) override;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset = 0) override;
    const char *name() const override { return "FLEXIO"; }
    int getPin() const override { return mPin; }
    bool injectEdges(fl::span<const EdgeTime> edges) override;

  private:
    int mPin = -1;
};

bool FlexIoRxChannelImpl::begin(const RxConfig &config) {
    (void)config;
    // Phase 1B will configure FLEXIO1 shifter + timer + DMA here.
    FL_WARN("[FlexIO RX] begin() called but Phase 1B FLEXIO1 register "
            "programming is not yet implemented (pin "
            << mPin << "). See FastLED#2764.");
    return false;
}

bool FlexIoRxChannelImpl::finished() const {
    // Skeleton: nothing is ever running, so report "not active" via
    // finished()==false (the channel never claims completion of a capture
    // it didn't perform).
    return false;
}

RxWaitResult FlexIoRxChannelImpl::wait(u32 timeout_ms) {
    (void)timeout_ms;
    // Skeleton: no hardware capture in progress yet → report TIMEOUT so
    // callers fall out of their wait loops cleanly. Phase 1B replaces this
    // with a real DMA-completion or eventfd-style wait.
    return RxWaitResult::TIMEOUT;
}

fl::result<u32, DecodeError>
FlexIoRxChannelImpl::decode(const ChipsetTiming4Phase &timing,
                            fl::span<u8> out) {
    (void)timing;
    (void)out;
    // Skeleton: no captured data yet → report no bytes decoded.
    return fl::result<u32, DecodeError>::success(0u);
}

size_t FlexIoRxChannelImpl::getRawEdgeTimes(fl::span<EdgeTime> out,
                                            size_t offset) {
    (void)out;
    (void)offset;
    // Skeleton: no captures available; subsequent Phase 1B replaces this
    // with a copy from the DMA capture buffer once the hardware is up.
    return 0u;
}

bool FlexIoRxChannelImpl::injectEdges(fl::span<const EdgeTime> edges) {
    (void)edges;
    // Skeleton: edge injection is used by host-stub tests and by the
    // bench fixture that synthesizes a known signal. Phase 1B wires this
    // through a software edge buffer that the decoder can consume.
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
//
// Phase 1A accepts any pin — the FLEXIO1 mux validation lands in Phase 1B
// alongside the pin-mapping table. Returning a real (but inert) object means
// the dispatcher won't fall back to DummyRxDevice while we iterate on the
// hardware programming.
fl::shared_ptr<FlexIoRxChannel> FlexIoRxChannel::create(int pin) {
    return fl::make_shared<FlexIoRxChannelImpl>(pin);
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
