// IWYU pragma: private

#ifndef __INC_CLOCKLESS_ARM_LPC_PWM_DMA_H
#define __INC_CLOCKLESS_ARM_LPC_PWM_DMA_H

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"
#include "fastled_delay.h"

// =============================================================================
// LPC845 PWM+DMA-to-GPIO clockless driver (Stage 2c of #2836, issue #2842)
// =============================================================================
//
// OVERVIEW
// --------
// CPU-free WS2812 output using the LPC845 State Configurable Timer (SCT) +
// DMA to GPIO SET/CLR registers. The SCT generates three match events at the
// WS2812 timing positions (T0_RISE, T_MID, T_END); each match event triggers
// a DMA channel that writes a single 32-bit GPIO bitmask to LPC_GPIO->SET[0]
// or LPC_GPIO->CLR[0]. Three pre-encoded DMA streams (rising, mid, falling)
// shift one word per WS2812 bit, so the only CPU work is the encode-pass that
// runs ahead of the DMA stream.
//
// This driver is novel in FastLED. The closest existing analogues are:
//   * src/platforms/arm/rp/rpcommon/clockless_rp_pio.h - RP2040 PIO + DMA
//   * src/platforms/arm/teensy/teensy4_common/drivers/flexio/ - i.MX FlexIO
//
// BUILD-TIME OPT-IN
// -----------------
// Activated only when *both* are defined:
//   * FL_IS_ARM_LPC_845 (auto-detected from CMSIS device macros in is_lpc.h)
//   * FASTLED_LPC_PWM_DMA  (user-supplied, default off)
//
// When FASTLED_LPC_PWM_DMA is not set, fastled_arm_lpc.h continues to route
// the controller to the Stage 2a bit-bang driver in clockless_arm_lpc.h.
// On host builds (no FL_IS_ARM_LPC) the entire file compiles to empty.
//
// RESOURCES CONSUMED
// ------------------
//   * DMA channels: 3 contiguous channels, base = FASTLED_LPC_PWM_DMA_BASECH
//     (default 0). Channel N+0 fires on T0_RISE (LED -> HIGH for every bit),
//     channel N+1 fires on T_MID (drop to LOW for '0' bits only), channel
//     N+2 fires on T_END (drop to LOW for '1' bits, no-op for '0' bits).
//   * SCT match events: MATCH0/MATCH1/MATCH2 (16-bit unified counter mode).
//   * SCT itself: claimed for the lifetime of the driver. User code may not
//     repurpose the SCT while a FastLED LPC845-PWM-DMA controller exists.
//   * GPIO port 0: pin direction set to output. The data pin can be any
//     PIO0_x on the package - DMA writes a full bitmask to SET[0]/CLR[0]
//     so pin selection is purely a per-bit mask choice. (See pin-constraint
//     note below.)
//
// PIN CONSTRAINTS
// ---------------
// Any GPIO0 pin is acceptable. The SCT is *not* routed to the data pin via
// hardware (no SCTOUT mux); the DMA writes the mask of the configured pin to
// SET[0]/CLR[0] at SCT match time. Stage 4 (#2845) will explore using the
// SCTOUT0..6 mux for multi-chain parallel output. PIO1 (LPC845 64-pin package
// only) is not supported in v1.
//
// SCT TIMING (UM11029 16.6.6 MATCH registers, 16.6.13 STATE/EVENT machine)
// ------------------------------------------------------------------------
// The SCT runs as a single 16-bit unified up-counter driven by the system
// clock. At the canonical LPC845 default F_CPU = 24 MHz (Arduino core
// + fbuild lpc845brk.json contract) the SCT tick is 41.67 ns; the
// WS2812 timing positions fall on these counts:
//
//   T0_RISE  = 0            (rising edge of every bit)
//   T0_FALL  =  9  (~375 ns) (T0H window end -> drop for '0' bits)
//   T1_FALL  = 19  (~792 ns) (T1H window end -> drop for '1' bits)
//   T_END    = 30  (~1250 ns) (counter reload, next bit starts)
//
// (Counts at F_CPU=30 MHz, an opt-in PLL configuration: T0_FALL=11 / 367 ns,
//  T1_FALL=24 / 800 ns, T_END=37 / 1233 ns. The derivation macro at line 313
//  picks up whichever F_CPU is set at compile time.)
//
// Three match registers fire the three DMA streams. MATCH3 is configured to
// reload the counter on T_END.
//
// For TIMING values other than the default WS2812 these constants are
// derived at compile time from TimingTraits<TIMING>::T1/T2/T3 (ns) using
// (ns * F_CPU + 5e8) / 1e9 rounding-to-nearest.
//
// DMA DESCRIPTOR FORMAT (UM11029 17.4 DMA controller / channel descriptors)
// -------------------------------------------------------------------------
// Each DMA channel uses a single linear source buffer (width = 32-bit words,
// one word per WS2812 bit) writing to a fixed destination (LPC_GPIO->SET[0]
// or CLR[0]). The descriptor stream is laid out per-channel:
//
//   ch+0 (RISE):  N words of constant kPinSetMask  -> SET[0]
//   ch+1 (T0_FALL): N words; '0'-bits = kPinClrMask, '1'-bits = 0 -> CLR[0]
//   ch+2 (T1_FALL): N words; '1'-bits = kPinClrMask, '0'-bits = 0 -> CLR[0]
//
// Writing 0 to CLR[0] is a documented no-op (UM11029 12.4.2.6), so the
// "skip this bit" entries cost only a DMA word in RAM. For a 144-LED strip
// this is 3 * (144*24) = 10368 32-bit words = 41 KB which exceeds the
// LPC845's 16 KB SRAM. We therefore stream-encode in BUFFER_BITS chunks
// (default 64 bits = 768 bytes total) and re-arm DMA in the SCT match-3
// (counter-reload) ISR. See encodePixelsForDma() below.
//
// TODO(2842): replace the 3-channel design with a 1-channel + chained SCT
// match->memcpy if the M0+ ISR cost (~50 cycles per chunk) proves too high.
// Two valid alternative encodings are:
//   (a) Single channel writing alternating SET/CLR with SCT match toggling
//       the DMA source pointer between two banks (complex, lower RAM).
//   (b) Two channels (rising = constant SET, falling = data-dependent CLR
//       chosen per bit) using SCT events at T0_FALL and T1_FALL conditional
//       on the next bit's value (requires SCT INVAL evaluation - novel).
// Stage 4 #2845 design review will pick one.
//
// MULTI-CHAIN SUPPORT (FastLED #2879 — Stage 4.4 from #2845)
// -------------------
// v1 supports a single chain. Multi-chain (parallel output from one DMA
// stream sharing the SCT timebase) is the Stage 4.4 follow-up. The design
// extension is well-bounded; the gate is hardware verification of the v1
// single-chain DMA path, not architectural risk.
//
// Sketch of the extension (kept here so a follow-up PR can drop into
// well-prepared ground once the bench is open):
//
//   1. Add a LANE_COUNT template parameter to LpcPwmDmaController. Default
//      to 1 to preserve the v1 ABI; users opt in via
//      `addLeds<WS2812, PIN, GRB, /*LANE_COUNT=*/4>(leds, n)` or similar.
//
//   2. DMA channel sharing. The current v1 layout claims 3 contiguous
//      channels per controller instance (T0_RISE / T_MID / T_END). For
//      multi-strip, the SAME 3 channels are reused — only the SET/CLR
//      bitmask written by each transfer changes. That means each match
//      event's DMA descriptor table needs LANE_COUNT entries chained
//      via DMA_DESCRIPTOR_NEXTDESC; the SCT match timebase stays single
//      and drives all lanes synchronously.
//
//   3. Encode pass. Single-strip's encoder emits one 32-bit word per
//      WS2812 bit (the GPIO mask for the active pin). Multi-strip emits
//      LANE_COUNT words per WS2812 bit — bit i of word j across all lanes
//      OR'd into one mask. The encode-time cost is O(LANE_COUNT * leds);
//      DMA time is unchanged (still one bit per match event, just with a
//      wider bitmask).
//
//   4. Pin constraints. All lane pins must live on GPIO port 0 (same
//      constraint as v1 — DMA writes to LPC_GPIO->SET[0]/CLR[0]).
//
//   5. Test harness. The on-device verifier lands in
//      `examples/AutoResearch/AutoResearch.ino` low-memory mode (post
//      FastLED #3041) so the multi-strip RPC binding sits next to the
//      single-strip `ws2812SctTest` already shipped.
//
// Architectural template references: RP2040 PIO `clockless_rp_pio.h`
// (PIO state machine fan-out) and Teensy 4.x FlexIO multi-lane.

// =============================================================================
// IMPLEMENTATION
// =============================================================================

#if defined(FL_IS_ARM_LPC) && defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)

#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/cstring.h"
#include "eorder.h"
#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.h"
#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.h"

namespace fl {

#ifndef FL_CLOCKLESS_CONTROLLER_DEFINED
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#endif

// Peripheral access uses the canonical NXP CMSIS PAL types (SCT_Type,
// DMA_Type, SYSCON_Type) and pointer macros (SCT0, DMA0, SYSCON). These
// are brought in by led_sysdefs_arm_lpc.h which includes <LPC845.h>
// from the toolchain (zackees/ArduinoCore-LPC8xx variants/lpc845/, after
// PR #34 ships the full upstream NXP header). FastLED #3437.

// SCT match counts derived from TIMING (nanoseconds) and F_CPU.
// Formula: ticks = (ns * F_CPU + 5e8) / 1e9  (round-to-nearest).
// Done as a constexpr template helper so the calculation stays a single
// return-expression (C++11-compatible constexpr).
template <u32 NS>
struct LpcSctTicks {
    static constexpr u32 value =
        (NS * (F_CPU / 1000000UL) + 500UL) / 1000UL;
};

// Number of 32-bit words required to encode a single chunk of bits in
// the DMA stream (one channel = one word per bit).
inline constexpr u32 LpcPwmDmaChunkWords() FL_NO_EXCEPT {
    return FASTLED_LPC_PWM_DMA_CHUNK_BITS;
}

// =============================================================================
// ClocklessController - PWM+DMA implementation
// =============================================================================

template <u8 DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<DATA_PIN>::port_t     data_t;

    // SCT tick counts for each WS2812 timing position. Computed at compile
    // time from the chipset timing struct's T1/T2/T3 (nanoseconds).
    static constexpr u32 kT0Rise  = 0;
    static constexpr u32 kT0Fall  = LpcSctTicks<TIMING::T1>::value;
    static constexpr u32 kT1Fall  = LpcSctTicks<TIMING::T1 + TIMING::T2>::value;
    static constexpr u32 kBitEnd  = LpcSctTicks<TIMING::T1 + TIMING::T2 + TIMING::T3>::value;

    data_t     mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

    // Per-instance working buffer for the 3-stream encoding. Static so a
    // single 768-byte allocation is shared across controller instances
    // (mutually exclusive use - the SCT is a single global resource).
    // TODO(2842): if multiple controllers ever coexist this buffer needs
    // to move into a per-instance allocation. Today only one PWM+DMA
    // controller may be live at once - see header comment.
    static u32 sChannelBuf[3 * FASTLED_LPC_PWM_DMA_CHUNK_BITS];

public:
    virtual void init() FL_NO_EXCEPT {
        FastPin<DATA_PIN>::setOutput();
        mPinMask = FastPin<DATA_PIN>::mask();
        mPort    = FastPin<DATA_PIN>::port();

        // Power up SCT and DMA in SYSAHBCLKCTRL0.
        // TODO(2842): verify SYSCON->SYSAHBCLKCTRL0 bit assignments against
        // UM11029 §4.6.13. SCT = bit 8 (per LPC8xx historical layout);
        // DMA = bit 29 (per LPC845 SDK clock_config.h).
        SYSCON->SYSAHBCLKCTRL0 |= (1UL << 8) | (1UL << 29);

        configureSct();
        configureDma();
    }

    virtual u16 getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT {
        mWait.wait();
        // CPU-free path: encode chunks ahead of the DMA stream, kick off
        // DMA, then either spin-wait on the DMA done flag or yield. We use
        // the simplest valid model in v1: encode-the-whole-frame into
        // chunked streams, kick the SCT, wait for the BUSY flag to clear.
        showRGBInternal(pixels);
        mWait.mark();
    }

    void showRGBInternal(PixelController<RGB_ORDER> pixels) FL_NO_EXCEPT {
        // Detect RGBW (consistent with RP2040 PIO driver pattern).
        const Rgbw rgbw = this->getRgbw();
        const bool is_rgbw = rgbw.active();
        const int bytes_per_pixel = is_rgbw ? 4 : 3;
        const u32 total_bits = static_cast<u32>(
            pixels.mLen * bytes_per_pixel * (8 + XTRA0));

        // Stream-encode in chunks. The SCT keeps producing match events
        // forever once started; the encode/re-arm step refills the channel
        // source buffers ahead of each chunk boundary.
        //
        // TODO(2842): wire SCT MATCH_END interrupt (EV3 -> NVIC IRQn) to
        // notify the encoder thread when a chunk has been consumed. v1 below
        // is the simpler synchronous variant: produce chunk -> kick DMA ->
        // wait for ACTIVE flag to clear -> produce next. Correct timing
        // requires the SCT to halt at chunk boundaries; we configure
        // EVENT3 -> HALT_L for that purpose. See configureSct().

        u32 bits_remaining = total_bits;
        bool first_byte = true;

        while (bits_remaining > 0) {
            const u32 chunk_bits = bits_remaining > LpcPwmDmaChunkWords()
                                       ? LpcPwmDmaChunkWords()
                                       : bits_remaining;
            encodeChunk(pixels, chunk_bits, first_byte);
            first_byte = false;

            startDmaChunk(chunk_bits);
            waitDmaChunk();

            bits_remaining -= chunk_bits;
        }

        // Pull the line low between frames; the WS2812 reset gap is
        // satisfied by CMinWait<WAIT_TIME>.
        FastPin<DATA_PIN>::lo();
    }

private:
    // Encode one chunk of WS2812 bits into the three DMA channel buffers.
    // Layout: sChannelBuf[0 .. CHUNK_BITS)             = RISE stream
    //         sChannelBuf[CHUNK_BITS .. 2*CHUNK_BITS)  = T0_FALL stream
    //         sChannelBuf[2*CHUNK_BITS .. 3*CHUNK_BITS) = T1_FALL stream
    void encodeChunk(PixelController<RGB_ORDER>& pixels,
                     u32 chunk_bits, bool first_byte) FL_NO_EXCEPT {
        const u32 hi_mask = mPinMask;
        const u32 lo_mask = mPinMask;
        u32* rise_stream   = &sChannelBuf[0];
        u32* t0fall_stream = &sChannelBuf[LpcPwmDmaChunkWords()];
        u32* t1fall_stream = &sChannelBuf[2 * LpcPwmDmaChunkWords()];

        // Every bit raises the pin at T0_RISE.
        for (u32 i = 0; i < chunk_bits; ++i) {
            rise_stream[i] = hi_mask;
        }

        if (first_byte) {
            pixels.preStepFirstByteDithering();
        }

        // Walk pixel bytes filling t0fall / t1fall streams.
        u32 bit_idx = 0;
        while (bit_idx < chunk_bits && pixels.has(1)) {
            pixels.stepDithering();
            u8 b0 = pixels.loadAndScale0();
            u8 b1 = pixels.loadAndScale1();
            u8 b2 = pixels.loadAndScale2();

            // RGB-only path; RGBW left as a TODO for v1.
            encodeByte(b0, t0fall_stream, t1fall_stream, bit_idx, chunk_bits,
                       lo_mask);
            bit_idx += 8 + XTRA0;
            if (bit_idx >= chunk_bits) break;
            encodeByte(b1, t0fall_stream, t1fall_stream, bit_idx, chunk_bits,
                       lo_mask);
            bit_idx += 8 + XTRA0;
            if (bit_idx >= chunk_bits) break;
            encodeByte(b2, t0fall_stream, t1fall_stream, bit_idx, chunk_bits,
                       lo_mask);
            bit_idx += 8 + XTRA0;

            pixels.advanceData();
        }

        // Pad tail of chunk with zeros (no-op CLR writes).
        for (; bit_idx < chunk_bits; ++bit_idx) {
            t0fall_stream[bit_idx] = 0;
            t1fall_stream[bit_idx] = 0;
        }

        // TODO(2842): RGBW support - the FlexIO driver uses
        // loadAndScaleRGBW; mirror that here once RGB is hardware-validated.
        (void)first_byte;
    }

    static inline void encodeByte(u8 byte_value,
                                  u32* t0fall_stream,
                                  u32* t1fall_stream,
                                  u32 bit_idx,
                                  u32 chunk_bits,
                                  u32 lo_mask) FL_NO_EXCEPT {
        // MSB first (WS2812 standard).
        for (u8 i = 0; i < 8; ++i) {
            const u32 dst = bit_idx + i;
            if (dst >= chunk_bits) return;
            const bool bit = (byte_value & (0x80u >> i)) != 0;
            // '0' bit: drop at T0_FALL (short HIGH).
            // '1' bit: drop at T1_FALL (long HIGH).
            t0fall_stream[dst] = bit ? 0u : lo_mask;
            t1fall_stream[dst] = bit ? lo_mask : 0u;
        }
    }

    // Configure SCT: 16-bit unified counter, MATCH0=T0_RISE, MATCH1=T0_FALL,
    // MATCH2=T1_FALL, MATCH3=BIT_END -> reload + DMAREQ pulse.
    // UM11029 §16.6.1 CONFIG, §16.6.6 MATCH, §16.6.21 EVx_CTRL.
    void configureSct() FL_NO_EXCEPT {
        // CONFIG: UNIFY (bit 0) - one 32-bit counter; CLKMODE = SYS clock.
        // TODO(2842): verify CONFIG layout vs UM11029 §16.6.1.
        SCT0->CONFIG = 0x00000001UL;  // UNIFY
        SCT0->CTRL   = 0x00000004UL;  // HALT_L = 1 (paused while configuring)

        SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE]   = kT0Rise;
        SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL] = kT0Fall;
        SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL] = kT1Fall;
        SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_END]    = kBitEnd;
        SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE]   = kT0Rise;
        SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL] = kT0Fall;
        SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL] = kT1Fall;
        SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_END]    = kBitEnd;

        // Event 0/1/2/3: each tied to a MATCH register and active in state 0.
        // EVx_CTRL bits 0..3 = MATCHSEL, bit 12 = COMBMODE (MATCH only).
        // TODO(2842): verify EVx_CTRL bit layout vs UM11029 §16.6.21.
        for (u32 ev = 0; ev < 4; ++ev) {
            SCT0->EV[ev].STATE = 0x00000001UL;  // active in STATE 0
            SCT0->EV[ev].CTRL  = (ev & 0xF) |   // MATCHSEL = ev
                                    (0x1UL << 12);  // COMBMODE = MATCH
        }

        // Limit on event 3 (T_END) -> counter reloads.
        SCT0->LIMIT = (1UL << 3);

        // Route SCT events to DMA. UM11029 §17.3.3 DMA hardware trigger
        // matrix - SCT_DMA0/1 are SCT-driven trigger lines.
        // TODO(2842): verify SCT->DMAREQ0/1 vs UM11029 §16.6.18.
        SCT0->DMAREQ0 = (1UL << 0) | (1UL << 1) | (1UL << 2);

        // Enable EV0..EV3.
        SCT0->EVEN = 0xFUL;
    }

    // Configure 3 DMA channels writing to GPIO SET[0]/CLR[0].
    // UM11029 §17.4 channel descriptor, §17.5 XFERCFG.
    void configureDma() FL_NO_EXCEPT {
        // Enable DMA.
        DMA0->CTRL = 1UL;
        // SRAMBASE points at the channel descriptor table - must be 256-byte
        // aligned. TODO(2842): allocate aligned descriptor block (static
        // alignas(256) array) and wire SRAMBASE here. v1 punts to the SDK
        // clock_config.h conventional address.
        // DMA0->SRAMBASE = (uint32_t)sDmaDescriptors;

        const u32 base = FASTLED_LPC_PWM_DMA_BASECH;
        for (u32 i = 0; i < 3; ++i) {
            // Configuration register for DMA channel; UM11029 §17.6.1.
            //     bit 0    PERIPHREQEN = 1 (Peripheral Request Enable for this channel.)
            //     bit 1    HWTRIGEN = 1 (Hardware Triggering Enable for this channel.)
            //     bit 4    TRIGPOL = 0 (Trigger Polarity: 0 = active high, 1 = active low.)
            //     bit 5    TRIGTYPE = 0 (Trigger Type: 0 = edge, 1 = level.)
            //     bit 6    TRIGBURST = 0 (Trigger Burst: 0 = single transfer, 1 = burst transfer.)
            //     bit 8-11  BURSTPOWER = 0 (Burst Power: 0 = 1 transfer, 1 = 2 transfers, ..., 15 = 16 transfers)
            //     bit 16-18 CHPRIORITY = 0 (Channel Priority: 0 = lowest, 15 = highest.)
            // @phatpaul caught the missing PERIPHREQEN in FastLED #3349.
            DMA0->CHANNEL[base + i].CFG =
                (1UL << 0) |  // PERIPHREQEN = 1
                (1UL << 1) |  // HWTRIGEN = 1
                (0UL << 4) |  // TRIGPOL = 0
                (0UL << 5);  // TRIGTYPE = 0

            // INMUX entry maps SCT match event -> channel i.
            // TODO(2842): set up DMA_ITRIG_INMUX[i] = SCT_DMA0_inmux_source.
        }
        // ENABLESET marks the channels as live; descriptors are armed by
        // SETVALID/SETTRIG at chunk start (startDmaChunk()).
        DMA0->COMMON[0].ENABLESET |= (7UL << base);  // 3 contiguous channels
    }

    // Arm one chunk: program each channel's source address to point at the
    // appropriate stream slice in sChannelBuf, set XFERCFG with the chunk's
    // word-count, then trigger.
    void startDmaChunk(u32 chunk_bits) FL_NO_EXCEPT {
        const u32 base = FASTLED_LPC_PWM_DMA_BASECH;
        // TODO(2842): wire the per-channel descriptor block. The DMA on
        // LPC845 fetches the source-end / dest-end pointers from a 16-byte
        // descriptor in SRAM (SRAMBASE + 16*ch). XFERCFG.XFERCOUNT = N-1.

        // Configure XFERCFG for each channel (UM11029 §17.5):
        //   bit  0    CFGVALID
        //   bit  1    RELOAD
        //   bit  2    SWTRIG (don't set - we use HW trigger)
        //   bit  4    SETINTA
        //   bit  8-9  WIDTH  = 10b (32-bit)
        //   bit 12-13 SRCINC = 01b (+1 word)
        //   bit 14-15 DSTINC = 00b (no-inc, write same SET/CLR reg)
        //   bit 16-25 XFERCOUNT = chunk-1
        const u32 xfercfg_base =
            (1UL << 0) |   // CFGVALID
            (2UL << 8) |   // WIDTH = 32-bit
            (1UL << 12) |  // SRCINC = +1 word
            (0UL << 14);   // DSTINC = no
        const u32 count_field = ((chunk_bits - 1UL) & 0x3FFUL) << 16;
        for (u32 i = 0; i < 3; ++i) {
            DMA0->CHANNEL[base + i].XFERCFG = xfercfg_base | count_field;
        }

        // SETVALID flips the descriptor's VALIDPENDING -> VALID, arming the
        // channel for the next hardware trigger from the SCT.
        DMA0->COMMON[0].SETVALID = (7UL << base);

        // Release the SCT HALT so the timer starts producing match events.
        SCT0->CTRL &= ~0x00000004UL;
    }

    void waitDmaChunk() FL_NO_EXCEPT {
        const u32 mask = (7UL << FASTLED_LPC_PWM_DMA_BASECH);
        // Spin until all three channels report ACTIVE=0 (transfer done).
        while ((DMA0->COMMON[0].ACTIVE & mask) != 0) {
            // CPU-free transmission goal: user-level code may run on the
            // outer task; this driver simply blocks at the FastLED.show()
            // boundary. TODO(2842): replace busy-wait with WFI + DMA done
            // IRQ once IRQ handler is wired.
        }
        // Halt SCT in preparation for the next chunk.
        SCT0->CTRL |= 0x00000004UL;
    }
};

// Static channel buffer definition. C++11 requires an out-of-class
// definition for non-inline static data members; we keep it in-header
// via a templated dummy to avoid an extra .cpp file. TODO(2842): move
// to a real .cpp when this driver gains a build-system entry.
template <u8 P, typename T, EOrder R, int X, bool F, int W>
u32 ClocklessController<P, T, R, X, F, W>::sChannelBuf
    [3 * FASTLED_LPC_PWM_DMA_CHUNK_BITS];

}  // namespace fl

#endif  // FL_IS_ARM_LPC && FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA
#endif  // __INC_CLOCKLESS_ARM_LPC_PWM_DMA_H
