/// @file lpc_sct_dma_runtime.h
/// @brief Runtime form of the LPC845 SCT + 3-DMA-channel clockless engine
///        from `clockless_arm_lpc_pwm_dma.h`. Used by the channels-API
///        driver `ChannelEngineLpcSctDma` (#3459).
///
/// The legacy `ClocklessController` template in `clockless_arm_lpc_pwm_dma.h`
/// bakes the chipset timing and data pin into the type at compile time
/// (via `TIMING::T1/T2/T3` and `FastPin<DATA_PIN>`). The channels-API
/// dispatcher operates on runtime values from `ChannelData`, so the
/// SCT/DMA programming sequence needs a runtime form. That is this file.
///
/// **Scope of this lift (#3459):**
///   1. `lpc_sct_ticks(u32 ns)` — runtime form of `LpcSctTicks<NS>::value`.
///   2. `class LpcSctDmaTransmitter` — non-template wrapper around
///      `configureSct()` / `configureDma()` / `encodeChunk()` /
///      `startDmaChunk()` / `waitDmaChunk()` from the legacy template,
///      with a **per-instance** working buffer (closes one of the
///      `TODO(2842)` markers — the static `sChannelBuf` shared across
///      instances).
///   3. Host build is a no-op compile target so the channels-API engine
///      can be exercised by the host tests in
///      `tests/fl/channels/lpc_sct_dma_engine.cpp` (#3464) without
///      requiring silicon.
///
/// **What's NOT covered yet:**
///   - Bench validation. The TX path inherits every `TODO(2842)` marker
///     from the legacy template — see `clockless_arm_lpc_pwm_dma.h`.
///   - Multi-strip parallel output (#2879 Stage 4.4).
///   - LPC804 port — the LPC804's 4-channel DMA limit forbids the
///     3-channel topology used here.

#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

// Same gate convention as the channel engine — compiles on LPC845 (the
// real target) AND on host/stub builds so `ChannelEngineLpcSctDma` can
// hold an instance unconditionally.
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_STUB) || defined(FASTLED_STUB_IMPL)

#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

// Chunk size matches the legacy template driver's
// `FASTLED_LPC_PWM_DMA_CHUNK_BITS` so memory budgets are unchanged.
// Default 64 bits = 64 words × 3 streams × 4 B = 768 B per instance.
// Override via `-DFASTLED_LPC_PWM_DMA_CHUNK_BITS=N` (same macro the
// legacy driver honors).
#ifndef FASTLED_LPC_PWM_DMA_CHUNK_BITS
#define FASTLED_LPC_PWM_DMA_CHUNK_BITS 64u
#endif

namespace fl {

/// @brief Convert nanoseconds to SCT counter ticks at the active F_CPU.
///
/// Runtime form of the template helper `LpcSctTicks<NS>::value` from
/// `clockless_arm_lpc_pwm_dma.h`. Rounded to nearest.
///
/// On host / non-LPC builds `F_CPU` is whatever the host config defines;
/// the math still compiles but the resulting tick count is meaningless —
/// the transmitter's host methods don't consume it. Inline so the LPC
/// build picks up the compile-time `F_CPU` directly.
inline u32 lpc_sct_ticks(u32 ns) FL_NO_EXCEPT {
#if defined(F_CPU)
    return (ns * (F_CPU / 1000000UL) + 500UL) / 1000UL;
#else
    return ns;  // host placeholder — never read on host
#endif
}

/// @brief Runtime SCT+DMA transmitter for the LPC845 clockless engine.
///
/// One instance per `ChannelEngineLpcSctDma`. Owns:
///   - A per-instance 3×CHUNK_BITS u32 buffer for the encoded streams
///     (this is the "closes one TODO(2842)" piece — the legacy template
///     keeps it static, sharing across instances).
///   - The runtime pin mask + SCT tick values for the active channel.
///
/// Methods are no-ops on host/stub builds; the LPC845 build does the
/// real SCT/DMA register sequencing.
class LpcSctDmaTransmitter {
public:
    LpcSctDmaTransmitter() FL_NO_EXCEPT;

    /// @brief One-time hardware init: power up SCT + DMA clocks and
    ///        configure the SCT skeleton (UNIFY counter, EV bindings,
    ///        DMA channel CFG). Idempotent — calling twice is safe.
    void init() FL_NO_EXCEPT;

    /// @brief Configure for a specific channel: latch the SCT match
    ///        values for the chipset timing and capture the GPIO pin
    ///        mask. Cheap; safe to call once per `show()`.
    /// @param pin    GPIO pin (PIO0_n on LPC845 — port 0 only).
    /// @param t1_ns  First WS2812 phase length, nanoseconds (e.g. 350).
    /// @param t2_ns  Second phase length, nanoseconds (e.g. 450).
    /// @param t3_ns  Third phase length, nanoseconds (e.g. 450).
    void configureForChannel(u8 pin, u32 t1_ns, u32 t2_ns, u32 t3_ns) FL_NO_EXCEPT;

    /// @brief Start a chunked SCT+DMA transmission of a raw byte stream.
    ///
    /// **Async semantics (LPC845):** encodes and kicks the first chunk,
    /// then returns immediately. The caller drives chunk progression by
    /// calling `pollAndAdvance()` until it returns true. Closes #3459
    /// acceptance criterion 3 — no busy-wait in `show()`; the state
    /// machine transitions `READY → BUSY → DRAINING → READY` are entirely
    /// driven from the engine's `poll()` cycle.
    ///
    /// **Sync semantics (host / stub):** there's no DMA to wait on, so
    /// the entire byte stream is copied into `mTxCapture` and
    /// transmission is complete on return. `pollAndAdvance()` immediately
    /// returns true.
    ///
    /// @param bytes  Raw RGB / RGBW bytes (already chipset-encoded —
    ///               `ChannelData` stores them that way).
    /// @param len    Byte count.
    /// @param is_rgbw  When true, the byte stream is 4 bytes per LED.
    void transmit(const u8* bytes, u32 len, bool is_rgbw) FL_NO_EXCEPT;

    /// @brief Advance the chunk-stream state machine by one step.
    ///
    /// Call from the engine's `poll()`. Semantics:
    ///
    ///   - Returns `true` when transmission is fully complete (all chunks
    ///     have finished). The engine can then transition to `READY`.
    ///   - Returns `false` while a chunk's DMA is still active OR another
    ///     chunk has just been kicked. The engine stays in `DRAINING`.
    ///
    /// On LPC845 this probes `DMA0->COMMON[0].ACTIVE` for the three
    /// SCT-tied channels. When they drop, if bytes remain the next chunk
    /// is encoded and kicked in this same call; if bytes are exhausted
    /// the transmission is marked idle.
    ///
    /// On host the transmit was fully synchronous, so this always
    /// returns true.
    bool pollAndAdvance() FL_NO_EXCEPT;

    /// @brief Legacy done-flag probe. Prefer `pollAndAdvance()`.
    ///
    /// Returns true when no transmission is in flight. Kept for
    /// backwards compatibility with callers that don't want to drive
    /// chunk progression themselves — those callers pay the cost that
    /// their poll() loop can't advance chunks (multi-chunk frames stall
    /// after chunk 1).
    bool isDone() const FL_NO_EXCEPT;

    /// @brief Return the byte stream captured by the most recent
    ///        `transmit()` call.
    ///
    /// **Host / stub builds only.** On LPC845 silicon there is no
    /// capture buffer — the transmit path writes bytes into the SCT +
    /// DMA hardware and the wire is the observation surface. On host
    /// the transmit path has no peripheral to drive, so it records the
    /// byte stream so tests (and #3468's `--pwm-dma-cl` AutoResearch
    /// harness in its host-simulation form) can round-trip the exact
    /// bytes through the LPC RX device's decoder — this is the
    /// TX→RX byte-flow readback contract that closes the LPC clockless
    /// bring-up loop.
    ///
    /// Returns an empty span on LPC845 silicon (the capture member
    /// stays empty on the real target — see `transmit()` body).
    fl::span<const u8> getCapturedTxBytes() const FL_NO_EXCEPT {
        return fl::span<const u8>(mTxCapture);
    }

    /// @brief Drop the capture buffer. Call before a fresh `transmit()`
    ///        when the caller wants to isolate one frame's output.
    void clearCapture() FL_NO_EXCEPT {
        mTxCapture.clear();
    }

private:
    // Per-instance encoding buffer. 3 streams × CHUNK_BITS words. The
    // layout matches the legacy template:
    //   [0 .. CHUNK_BITS)               = RISE stream
    //   [CHUNK_BITS .. 2*CHUNK_BITS)    = T0_FALL stream
    //   [2*CHUNK_BITS .. 3*CHUNK_BITS)  = T1_FALL stream
    u32 mChannelBuf[3 * FASTLED_LPC_PWM_DMA_CHUNK_BITS];

    // GPIO pin mask written by DMA into SET[0] / CLR[0].
    u32 mPinMask = 0;

    // SCT tick counts for the configured timing.
    u32 mT0Rise = 0;
    u32 mT0Fall = 0;
    u32 mT1Fall = 0;
    u32 mBitEnd = 0;

    bool mInitialized = false;

    // Chunk-progression state for async transmission (LPC845). On host
    // these members exist too but transmit() completes synchronously so
    // they remain in the "idle" state (mTransmitInProgress == false)
    // immediately after transmit() returns.
    const u8* mCurrentBytes = nullptr;
    u32 mCurrentLen = 0;
    u32 mBytesSent = 0;
    bool mTransmitInProgress = false;

    // Host-only TX byte capture. Populated by `transmit()` on host/stub
    // builds so tests can round-trip the byte stream through the LPC RX
    // device's decoder. Left empty on LPC845 silicon — the real target
    // observes the output on the actual wire.
    fl::vector<u8> mTxCapture;
};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL
