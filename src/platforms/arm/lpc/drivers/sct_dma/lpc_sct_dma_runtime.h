/// @file lpc_sct_dma_runtime.h
/// @brief Runtime SCT + DMA clockless transmitter for the LPC845, used by
///        the channels-API driver `ChannelEngineLpcSctDma` (#3459).
///
/// **How the waveform is made (FastLED #5265).** The SCT drives the data
/// pin itself: SCT output 0 is routed to the pin through the switch matrix
/// (SWM `SCT_OUT0`). In one bit period the unified counter fires
///   EV0 (MATCH0) -> output SET            (rising edge)
///   EV1 (MATCH1) -> output CLR if bit = 0 (T0H)
///   EV2 (MATCH2) -> output CLR            (T1H)
///   EV3 (MATCH3) -> counter limit         (bit period)
/// Whether EV1 clears the output is the only per-bit data, so one DMA
/// channel, triggered by EV2 through SCT_DMA0, writes the next bit's
/// clear-event mask into `SCT0->OUT[0].CLR`. EV2 leaves 800+ ns until the
/// next EV1, so the write always lands in time. The frame's last transfer
/// is a linked descriptor that writes HALT into `SCT0->CTRL`, so the SCT
/// stops on the last bit's EV2 and no stale DMA request is left behind.
///
/// The earlier form had the DMA write the pin mask into GPIO SET/CLR. That
/// cannot work on this part: the GPIO port sits on the Cortex-M0+ IOP bus
/// (UM11029 ch. 2), which the DMA cannot reach. Measured on silicon: a DMA
/// write to `GPIO->SET[0]` raises ERRINT and leaves the pin unchanged, while
/// the same write to RAM or to an SCT register lands.
///
/// **Chunking.** Frames stream in `FASTLED_LPC_PWM_DMA_CHUNK_BITS` chunks
/// through two buffers and two linked descriptors that reload each other,
/// so the DMA and the SCT never stop mid-frame. Each chunk's descriptor
/// raises INTA when it runs out; the handler encodes the chunk after next
/// into the buffer just freed and points the free descriptor at it (or at
/// the halt step). That must happen within one chunk time (120 us at the
/// default 96 bits): it takes ~60 us from the DMA ISR hub
/// (`FASTLED_LPC_DMA_ISR`, on by default with the UART-DMA driver). Without
/// the hub the refill happens in `poll()`, and a frame longer than two
/// chunks is cut short if `poll()` comes later than that. A refill that
/// comes too late ends the frame early (every consumed descriptor is reset
/// to the halt step, and a free one is rewritten only while the playing
/// chunk has bits to spare); it never replays old data or hangs.
///
/// **Not covered:** multi-strip output (#2879 Stage 4.4) and the LPC804
/// (no DMA).

#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

// Same gate convention as the channel engine — compiles on LPC845 (the
// real target) AND on host/stub builds so `ChannelEngineLpcSctDma` can
// hold an instance unconditionally.
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_STUB) || defined(FASTLED_STUB_IMPL)

#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

// Bits per chunk. Two chunk buffers of one u32 per bit: the default 96
// bits (4 RGB LEDs) keeps the 768 B the transmitter has always used. One
// chunk is one DMA descriptor, so it may not exceed 1024 transfers.
#ifndef FASTLED_LPC_PWM_DMA_CHUNK_BITS
#define FASTLED_LPC_PWM_DMA_CHUNK_BITS 96u
#endif
// The one DMA channel the transmitter uses. Its ITRIG input mux is pointed
// at SCT_DMA0, so any channel works. The SCT RX capture also wants channel
// 0 but can't run alongside anyway: both need the one SCT.
#ifndef FASTLED_LPC_PWM_DMA_BASECH
#define FASTLED_LPC_PWM_DMA_BASECH 0u
#endif

FL_STATIC_ASSERT(FASTLED_LPC_PWM_DMA_CHUNK_BITS >= 8u &&
                     FASTLED_LPC_PWM_DMA_CHUNK_BITS <= 1024u,
                 "FASTLED_LPC_PWM_DMA_CHUNK_BITS must be 8..1024 (whole bytes "
                 "in chunk 0; one DMA descriptor)");

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

/// SCT OUT0 clear-event masks: a '0' bit clears at EV1 (T0H) and EV2, a '1'
/// bit only at EV2 (T1H).
constexpr u32 kLpcSctClrMaskBit0 = (1u << 1) | (1u << 2);
constexpr u32 kLpcSctClrMaskBit1 = (1u << 2);

FL_STATIC_ASSERT(kLpcSctClrMaskBit0 - kLpcSctClrMaskBit1 == 2u,
                 "the encoder computes a mask as Bit0 - 2 * bit");

/// @brief Encode `nbits` bits of `bytes`, starting at bit `first_bit`
///        (MSB first), into SCT OUT0 clear masks, one u32 per bit.
///
/// This runs while the previous chunk plays, so it must beat the wire
/// (1.25 us per bit, 30 cycles at 24 MHz): whole bytes go through a
/// branchless, unrolled path.
inline void lpcSctDmaEncodeClrMasks(const u8* bytes, u32 first_bit, u32 nbits,
                                    u32* out) FL_NO_EXCEPT {
    u32 bit = first_bit;
    const u32 end = first_bit + nbits;
    // Leading bits up to a byte boundary.
    for (; bit < end && (bit & 7u) != 0u; ++bit) {
        *out++ = kLpcSctClrMaskBit0 - (((bytes[bit >> 3] >> (7u - (bit & 7u))) & 1u) << 1);
    }
    // Whole bytes.
    for (; bit + 8u <= end; bit += 8u) {
        const u32 b = bytes[bit >> 3];
        out[0] = kLpcSctClrMaskBit0 - ((b >> 6) & 2u);
        out[1] = kLpcSctClrMaskBit0 - ((b >> 5) & 2u);
        out[2] = kLpcSctClrMaskBit0 - ((b >> 4) & 2u);
        out[3] = kLpcSctClrMaskBit0 - ((b >> 3) & 2u);
        out[4] = kLpcSctClrMaskBit0 - ((b >> 2) & 2u);
        out[5] = kLpcSctClrMaskBit0 - ((b >> 1) & 2u);
        out[6] = kLpcSctClrMaskBit0 - (b & 2u);
        out[7] = kLpcSctClrMaskBit0 - ((b << 1) & 2u);
        out += 8;
    }
    // Trailing bits.
    for (; bit < end; ++bit) {
        *out++ = kLpcSctClrMaskBit0 - (((bytes[bit >> 3] >> (7u - (bit & 7u))) & 1u) << 1);
    }
}

/// @brief Runtime SCT+DMA transmitter for the LPC845 clockless engine.
///
/// One instance per `ChannelEngineLpcSctDma` (the SCT, its DMA channel and
/// the halt descriptor are chip-wide, so there is one per image). Owns the
/// two chunk buffers and the SCT tick values for the active channel.
///
/// On host/stub builds there are no registers; transmit() records the
/// bytes instead.
class LpcSctDmaTransmitter {
public:
    LpcSctDmaTransmitter() FL_NO_EXCEPT;

    /// @brief One-time hardware init: clock the SCT, DMA and SWM, reset
    ///        the SCT (only the SCT: DMA0 and SWM are shared, #4676), and
    ///        set up the event/output skeleton and the DMA channel's
    ///        SCT_DMA0 trigger. Idempotent.
    void init() FL_NO_EXCEPT;

    /// @brief Configure for a specific channel: latch the SCT match
    ///        values for the chipset timing and route SCT_OUT0 to `pin`
    ///        through the SWM. Cheap; safe to call once per `show()`.
    /// @param pin    GPIO pin (PIO0_n on LPC845 — port 0 only).
    /// @param t1_ns  First WS2812 phase length, nanoseconds (e.g. 350).
    /// @param t2_ns  Second phase length, nanoseconds (e.g. 450).
    /// @param t3_ns  Third phase length, nanoseconds (e.g. 450).
    void configureForChannel(u8 pin, u32 t1_ns, u32 t2_ns, u32 t3_ns) FL_NO_EXCEPT;

    /// @brief True when the timing fits the SCT event layout: rise at tick
    ///        1, T0H fall before T1H fall, and T1H fall before the period's
    ///        last tick. Otherwise EV2 never fires and a chunk never ends.
    static bool timingFits(u32 t1_ns, u32 t2_ns, u32 t3_ns) FL_NO_EXCEPT {
        const u32 t0h = lpc_sct_ticks(t1_ns);
        const u32 t1h = lpc_sct_ticks(t1_ns + t2_ns);
        const u32 period = lpc_sct_ticks(t1_ns + t2_ns + t3_ns);
        return t0h >= 1u && t1h > t0h && 1u + t1h < period - 1u;
    }

    /// @brief Stop a transmission in flight (SCT halted, DMA channel
    ///        disabled and aborted) and drop its state. Safe when idle.
    void abort() FL_NO_EXCEPT;

    /// @brief Start a chunked SCT+DMA transmission of a raw byte stream.
    ///
    /// **Async semantics (LPC845):** encodes the first two chunks, starts
    /// the stream, and returns. Later chunks are refilled from the DMA
    /// interrupt (or from `pollAndAdvance()`); the caller polls
    /// `pollAndAdvance()` until it returns true. No busy-wait in `show()`
    /// (#3459 criterion 3).
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
    /// Call from the engine's `poll()`. Returns `true` once the frame is
    /// over, `false` while it plays. Services a pending chunk-end INTA
    /// (builds without the DMA ISR hub rely on this). The frame is over
    /// when the DMA channel has left ACTIVE0 and the SCT has halted (the
    /// halt step ran). The XFERCFG CFGVALID bit stays set after a
    /// descriptor runs out (#4676), so it is not used.
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
    // True when the DMA channel has left ACTIVE0 and the SCT has halted.
    bool isStopped() const FL_NO_EXCEPT;
    // One chunk's descriptor ran out: refill the freed buffer and slot
    // with the chunk after next, or the halt step. Runs in the DMA ISR or
    // with interrupts off.
    void onChunkEnd() FL_NO_EXCEPT;
    // DMA ISR hub callback for a chunk's INTA.
    static void onDmaIsr(void* ctx) FL_NO_EXCEPT;
    // Encode the chunk starting at mBitsEncoded into mChunkBuf[buf];
    // returns its bit count (0 when the frame is exhausted).
    u32 encodeChunk(u32 buf) FL_NO_EXCEPT;

    // Two chunk buffers of SCT OUT0 clear masks, one u32 per bit.
    u32 mChunkBuf[2][FASTLED_LPC_PWM_DMA_CHUNK_BITS];

    // Data pin routed to SCT_OUT0 (0xFF = none yet).
    u8 mPin = 0xFF;

    // SCT tick counts for the configured timing.
    u32 mT0h = 0;     // '0' bit high time
    u32 mT1h = 0;     // '1' bit high time
    u32 mPeriod = 0;  // bit period

    bool mInitialized = false;

    // Chunk-progression state for async transmission (LPC845). On host
    // these members exist too but transmit() completes synchronously so
    // they remain in the "idle" state (mTransmitInProgress == false)
    // immediately after transmit() returns.
    const u8* mCurrentBytes = nullptr;
    u32 mTotalBits = 0;
    u32 mBitsEncoded = 0;   // encode cursor: bits encoded so far
    u32 mLastChunk = 0;     // index of the frame's last chunk
    // Shared with the DMA ISR; other contexts touch them with IRQs off.
    volatile u32 mPlayingChunk = 0;   // index of the chunk on the wire
    volatile bool mTransmitInProgress = false;

    // Host-only TX byte capture. Populated by `transmit()` on host/stub
    // builds so tests can round-trip the byte stream through the LPC RX
    // device's decoder. Left empty on LPC845 silicon — the real target
    // observes the output on the actual wire.
    fl::vector<u8> mTxCapture;
};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 || FL_IS_STUB || FASTLED_STUB_IMPL
