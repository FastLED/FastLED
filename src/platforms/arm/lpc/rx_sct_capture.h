/// @file platforms/arm/lpc/rx_sct_capture.h
/// @brief LPC8xx SCT input-capture + DMA RX driver for WS2812-like signals.
///
/// **Status:** scaffold + decoder. The register-level SCT/DMA capture path
/// in `begin()` is intentionally a documented TODO — the LPC845 SCT input
/// capture matrix (UM11029 §16) needs hardware bench validation that this
/// PR does not have access to. What ships here:
///
///   * `injectEdges()` — fully functional. Tests (and follow-up firmware
///     work that has its own capture front-end) can push a known edge
///     stream into the device and have it round-trip through `decode()`.
///   * `decode()` — fully functional. Delegates to the shared 4-phase
///     WS2812 decoder in `src/fl/channels/rx/decode_ws2812.h`, shared
///     with `NativeRxDevice`. FastLED #3035 Phase 3 (decoder
///     consolidation + LPC sketch retirement) is **shipped** via
///     #3037 (decoder split into .h/.cpp.hpp) and #3041 (LPC sketch
///     folded into the main AutoResearch low-memory mode). What remains
///     in #3035 is the Phase-1 / 2a / 2b hardware bench validation,
///     tracked alongside the #2880 hardware sign-off checklist.
///   * `getRawEdgeTimes()` / `finished()` / `wait()` / `name()` /
///     `getPin()` — fully functional against the in-RAM edge buffer.
///   * `begin()` — stores config, clears the buffer, returns `true`. The
///     actual SCT input-capture + DMA-to-RAM setup is sketched in detail
///     in the implementation file as a register-by-register block comment
///     pointing at UM11029, so a follow-up bench session can fill it in.
///
/// **Why a skeleton and not a full driver?** LPC845 has no FastLED-side
/// register definitions for SCT (the Arduino core's `LPC845.h` only ships
/// the IRQ vector list). Writing the register-level setup blind, without
/// a scope or LA on a real LPC845-BRK, would either work by luck or
/// silently mis-program the capture mux. Splitting the contract (this
/// PR) from the bench work (follow-up) lets the contract land first so
/// `fl::RxBackend::LPC_SCT_CAPTURE` is referenceable from #3015's
/// loopback test wiring while the actual capture path is being brought
/// up against silicon.
///
/// **Pin coverage when the capture path lands:** Any LPC845 GPIO that
/// the SCT input mux (`SCT->INPUTSEL` via the SWM input-route registers)
/// can route to one of the four SCT capture events. LPC845's SWM is
/// fully flexible — any of the 30 digital pins can be routed to any SCT
/// CTIN[0..3] event input, so the pin set is effectively unrestricted
/// once the SCT setup arrives.

#pragma once

// IWYU pragma: private

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC)

#include "fl/channels/rx.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief LPC8xx SCT + DMA RX device
class LpcSctRxChannel : public RxDevice {
public:
    /// Factory method
    /// @param pin GPIO pin number for receiving signals
    /// @return Shared pointer to LpcSctRxChannel
    static fl::shared_ptr<LpcSctRxChannel> create(int pin) FL_NOEXCEPT;

    // RxDevice interface
    bool begin(const RxConfig& config) FL_NOEXCEPT override;
    bool finished() const FL_NOEXCEPT override;
    RxWaitResult wait(u32 timeout_ms) FL_NOEXCEPT override;
    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase& timing,
                                        fl::span<u8> out) FL_NOEXCEPT override;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset = 0) FL_NOEXCEPT override;
    const char* name() const FL_NOEXCEPT override;
    int getPin() const FL_NOEXCEPT override;
    bool injectEdges(fl::span<const EdgeTime> edges) FL_NOEXCEPT override;

    /// @brief Drain pending SCT capture events into the edge buffer.
    /// @return Number of new EdgeTime entries pushed.
    ///
    /// Used by the FastLED #3021 pin-toggle test to interleave bit-bang
    /// TX with capture-buffer drains on a single-threaded M0+ — without
    /// it the SCT CAP[0..1] registers overwrite each prior capture
    /// before `wait()` gets a chance to read them. With the SCT→DMA
    /// upgrade (Phase 2 of #3021) the DMA fills the ring autonomously
    /// and this method becomes a no-op fast-path. On non-LPC builds /
    /// builds without `FASTLED_LPC_RX_SCT` it is a no-op stub.
    fl::size pollOnce() FL_NOEXCEPT;

    ~LpcSctRxChannel() FL_NOEXCEPT override;

private:
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args) FL_NOEXCEPT;

    explicit LpcSctRxChannel(int pin) FL_NOEXCEPT;

    int                   mPin;
    bool                  mFinished;
    size_t                mCapacity;  ///< Cap on mEdges growth set by begin(config.buffer_size)
    fl::u32               mPrevTick;  ///< Last SCT capture tick (driver-internal state for pollOnce)
    bool                  mPrevSeen;  ///< Whether mPrevTick is valid
    bool                  mLastRising;///< Direction of the last edge — used to label phase HIGH/LOW
    fl::vector<EdgeTime>  mEdges;     ///< RAM-backed capture buffer (DMA target in follow-up)
};

} // namespace fl

#endif // FL_IS_ARM_LPC
