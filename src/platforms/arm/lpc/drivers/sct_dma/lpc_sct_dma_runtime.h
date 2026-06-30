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
#include "fl/stl/stdint.h"

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

    /// @brief Encode and transmit a raw byte stream as a chunked SCT+DMA
    ///        stream. Blocks at chunk boundaries until each chunk's DMA
    ///        completes (matches the legacy template's per-chunk
    ///        synchronous wait — TODO(2842): wire the SCT MATCH_END
    ///        interrupt for true async).
    /// @param bytes  Raw RGB / RGBW bytes (already chipset-encoded —
    ///               `ChannelData` stores them that way).
    /// @param len    Byte count.
    /// @param is_rgbw  When true, the byte stream is 4 bytes per LED.
    void transmit(const u8* bytes, u32 len, bool is_rgbw) FL_NO_EXCEPT;

    /// @brief True once the most recent chunk's DMA channels have all
    ///        cleared their `ACTIVE` flag. Used by `poll()` to advance
    ///        the engine's state machine to `READY`.
    bool isDone() const FL_NO_EXCEPT;

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
};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL
