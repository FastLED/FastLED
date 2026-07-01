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
// DMA to the GPIO masked-port register (MPIN). The SCT generates three match
// events at the WS2812 timing positions (T0_RISE, T0_FALL, T1_FALL); all three
// events pulse SCT DMA-request-0, driving a SINGLE DMA channel that writes one
// 32-bit pin-level word to GPIO->MPIN[0] per event. The GPIO MASK register is
// programmed so only the data pin is affected, so each word is simply "pin
// high" (mask) or "pin low" (0). The pre-encoded stream carries three words
// per WS2812 bit, so the only CPU work is the encode-pass that runs ahead of
// the DMA stream.
//
// Why one channel + MPIN rather than three SET/CLR channels: the SCT exposes
// only two DMA request lines (DMAREQ0/DMAREQ1), which cannot give three
// edges three independent triggers. Combining all three edges onto DMAREQ0
// and writing pin levels to the masked MPIN register collapses the whole bit
// waveform onto one channel. See #2845 design notes below.
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │ HARDWARE FINDING (2026-07, on-device LPC845): the DMA controller CANNOT   │
// │ master-write the GPIO port at 0xA0000000. A DMA transfer targeting        │
// │ GPIO->MPIN[0] (0xA0002180) raises DMA0->COMMON[0].ERRINT; the identical   │
// │ transfer to a RAM address completes cleanly. Confirmed by the             │
// │ FASTLED_LPC_PWM_DMA_DEBUG_RAM_DST diagnostic in startDmaChunk(). The DMA   │
// │ reaches SRAM (0x1000_0000) and the APB/AHB peripherals (0x4000/0x5000)    │
// │ but not the 0xA000_0000 GPIO slave - matching the SDK, whose DMA examples │
// │ only ever target 0x4000_0000 peripherals, never GPIO.                     │
// │                                                                            │
// │ CONSEQUENCE: this DMA-to-GPIO design (and the parallel/multi-lane goal    │
// │ that motivated it) does NOT work on LPC845 as written. A working CPU-free │
// │ path must keep all DMA traffic in SRAM/peripheral space - e.g. DMA the    │
// │ SCT MATCH-compare values (0x5000_4000) and let the SCT drive an SCTOUT    │
// │ pin directly, or use the SPI-DMA driver (spi_arm_lpc_dma.h) for a single  │
// │ lane. The multi-output-via-GPIO-bitmask idea needs a chip whose DMA can   │
// │ reach GPIO. See TODO(2842)/#2845.                                          │
// └─────────────────────────────────────────────────────────────────────────┘
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
//   * DMA channel: 1 channel, index = FASTLED_LPC_PWM_DMA_BASECH (default 0),
//     hardware-triggered from SCT DMA-request-0 via DMA_ITRIG_INMUX. It fires
//     on all three edge events, writing 3 pin-level words per bit to MPIN[0].
//   * SCT match events: MATCH0/MATCH1/MATCH2 (16-bit unified counter mode),
//     all OR'd onto SCT DMAREQ0.
//   * SCT itself: claimed for the lifetime of the driver. User code may not
//     repurpose the SCT while a FastLED LPC845-PWM-DMA controller exists.
//   * GPIO port 0: pin direction set to output. The data pin can be any
//     PIO0_x on the package. GPIO->MASK[0] is set to ~pinmask so the DMA's
//     full-word MPIN[0] writes affect only the data pin; MASK gates the MPIN
//     register ONLY, so SET/CLR/PIN access by other code is unaffected. (See
//     pin-constraint note below.)
//
// PIN CONSTRAINTS
// ---------------
// Any GPIO0 pin is acceptable. The SCT is *not* routed to the data pin via
// hardware (no SCTOUT mux); the DMA writes the pin-level word to the masked
// MPIN[0] register at SCT match time. Stage 4 (#2845) will explore using the
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
// The single DMA channel uses one linear source buffer (width = 32-bit words)
// writing to a fixed destination (LPC_GPIO->MPIN[0]). The stream carries three
// words per WS2812 bit, in SCT-event (temporal) order:
//
//   [3*i + 0] (T0_RISE):  pin-high (mask)             -> MPIN[0]
//   [3*i + 1] (T0_FALL):  '1'-bit -> mask, '0' -> 0   -> MPIN[0]
//   [3*i + 2] (T1_FALL):  pin-low (0)                 -> MPIN[0]
//
// With GPIO->MASK[0] = ~pinmask, each MPIN write touches only the data pin,
// so the words are just "high" / "low" levels. For a 144-LED strip the full
// buffer would be 3 * (144*24) = 10368 32-bit words = 41 KB, exceeding the
// LPC845's 16 KB SRAM. We therefore stream-encode in BUFFER_BITS chunks
// (default 64 bits = 768 bytes total) and re-arm DMA between chunks. See
// showRGBInternal() below.
//
// This is option (a) from the original #2845 design menu, chosen because the
// SCT exposes only two DMA request lines (DMAREQ0/DMAREQ1) and thus cannot
// give three edges three independent triggers. Collapsing all three edges
// onto DMAREQ0 + one masked-MPIN channel sidesteps that limit entirely. The
// discarded alternatives were: 3 SET/CLR channels (needs 3 request lines -
// impossible), and 2 channels (RISE on DMAREQ0 + combined FALL on DMAREQ1
// writing 2 words/bit to CLR) - viable but uses a second channel and line.
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
//   2. DMA channel sharing. The v1 layout uses a single channel driven by
//      SCT DMAREQ0 (all three edge events). For multi-strip the SAME channel
//      and MPIN[0] destination are reused — only the pin-level word written
//      per event changes (a wider mask covering all lane pins). The SCT
//      match timebase stays single and drives all lanes synchronously; the
//      per-bit stream still carries 3 words, now with multi-pin masks.
//
//   3. Encode pass. Single-strip's encoder emits three level words per
//      WS2812 bit for one pin. Multi-strip OR's the active-lane masks into
//      each of those three words. The encode-time cost is O(LANE_COUNT *
//      leds); DMA time is unchanged (still 3 words per bit). Note MASK[0]
//      must then clear all lane-pin bits, not just one.
//
//   4. Pin constraints. All lane pins must live on GPIO port 0 (same
//      constraint as v1 — DMA writes to LPC_GPIO->MPIN[0]).
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
#include "fl/stl/align.h"
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

// LPC845 DMA channel descriptor (UM11029 §17.4.3). Raw 16-byte hardware
// layout - we deliberately do NOT depend on the vendor fsl_dma.h; this is
// the same struct the SDK's dma_descriptor_t describes, hand-declared so the
// PWM+DMA driver stays SDK-free. The controller locates channel N's
// descriptor at SRAMBASE + 16*N, so a table of these is indexed by absolute
// channel number. For a one-shot (non-reloading) transfer the hardware still
// reads srcEndAddr/dstEndAddr from the head descriptor to compute addresses;
// xfercfg/linkToNextDesc are only consumed when RELOAD is set.
struct LpcDmaDescriptor {
    volatile u32 xfercfg;         // reloaded transfer config (RELOAD only)
    const void*  srcEndAddr;      // last source address of the transfer
    void*        dstEndAddr;      // last destination address of the transfer
    void*        linkToNextDesc;  // next descriptor in chain, 0 = none
};

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

    // Named values for CMSIS register *fields* that the PAL provides a
    // shift/mask macro for but no symbolic value (the PAL macros take a raw
    // number). Keeps the register writes below free of magic literals.
    static constexpr u32 kSctStateDefault  = 0u;  // SCT powers up in state 0
    static constexpr u32 kSctCombModeMatch = 1u;  // EV_CTRL COMBMODE: match only
    static constexpr u32 kDmaWidth32Bit    = 2u;  // XFERCFG WIDTH: 32-bit word
    static constexpr u32 kDmaIncOneUnit    = 1u;  // XFERCFG SRC/DSTINC: +1 unit
    static constexpr u32 kDmaIncNone       = 0u;  // XFERCFG SRC/DSTINC: no incr
    static constexpr u32 kDmaTrigPolRising = 1u;  // CFG TRIGPOL: active-high /
                                                  // rising edge (the SCT DMA
                                                  // request line pulses HIGH)
    static constexpr u32 kDmaTrigEdge      = 0u;  // CFG TRIGTYPE: edge-sensitive
    // DMA hardware-trigger source index for the SCT's DMA-request-0 line
    // (UM11029 §11 DMA trigger matrix; equals the SDK's kINPUTMUX_SctDma0ToDma
    // low field). Selected via INPUTMUX->DMA_ITRIG_INMUX[ch].
    static constexpr u32 kInmuxSrcSctDma0  = 2u;

    data_t     mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

    // Single-channel MPIN encoding: one DMA channel triggered by the three
    // SCT match events (RISE, T0_FALL, T1_FALL) writes three 32-bit words per
    // WS2812 bit to GPIO->MPIN[0], in temporal order. The buffer is therefore
    // laid out interleaved as 3 words per bit:
    //   sChannelBuf[3*i + 0] = level at T0_RISE  (always pin-high)
    //   sChannelBuf[3*i + 1] = level at T0_FALL  ('1' stays high, '0' -> low)
    //   sChannelBuf[3*i + 2] = level at T1_FALL  (always pin-low)
    // Static so a single 768-byte allocation is shared across controller
    // instances (mutually exclusive use - the SCT is a single global
    // resource). TODO(2842): move to per-instance if multiple controllers
    // ever coexist. Today only one PWM+DMA controller may be live at once.
    static u32 sChannelBuf[3 * FASTLED_LPC_PWM_DMA_CHUNK_BITS];

    // DMA channel descriptor table pointed at by DMA0->SRAMBASE. The hardware
    // fetches channel N's descriptor at SRAMBASE + 16*N, so the table is
    // indexed by absolute channel number and must span the channel we drive
    // (BASECH). SRAMBASE ignores its low 9 bits (UM11029 §17.6.2), so the
    // block must be 512-byte aligned - NOT 256. Static/shared for the same
    // reason as sChannelBuf: one PWM+DMA controller is live at a time.
    FL_ALIGNAS(512) static LpcDmaDescriptor
        sDmaDescriptors[FASTLED_LPC_PWM_DMA_BASECH + 1];

public:
    virtual void init() FL_NO_EXCEPT {
        FastPin<DATA_PIN>::setOutput();
        mPinMask = FastPin<DATA_PIN>::mask();
        mPort    = FastPin<DATA_PIN>::port();

        // Masked-port write protection (UM11029 §12.4.2 / PERI_GPIO MASKP).
        // A MASK bit of 1 means "this pin is NOT affected by MPIN writes", so
        // we mask every pin EXCEPT the data pin. The DMA then streams full
        // 32-bit words to MPIN[0] and only the data pin ever changes - all
        // other GPIO0 users (SET/CLR/PIN) are unaffected since MASK gates
        // only the MPIN register. Nothing else in the codebase uses MPIN[0],
        // so leaving MASK set for the controller's lifetime is safe.
        GPIO->MASK[0] = ~static_cast<u32>(mPinMask);

        // Power up SCT, DMA and GPIO0 in SYSAHBCLKCTRL0 (UM11029 §4.6.13).
        // The SCT and DMA clocks also feed the INPUTMUX on LPC845 (it has no
        // dedicated clock gate - it borrows SCT+DMA; see fsl_inputmux.c
        // INPUTMUX_Init), so configureDma() can write DMA_ITRIG_INMUX without
        // any further enable. GPIO0 must be clocked for the MASK/MPIN writes.
        SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_SCT(1) |
                                  SYSCON_SYSAHBCLKCTRL0_DMA(1) |
                                  SYSCON_SYSAHBCLKCTRL0_GPIO0(1);

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
    // Encode one chunk of WS2812 bits into the interleaved MPIN stream.
    // Three words per bit in temporal (SCT event) order:
    //   sChannelBuf[3*i + 0] = T0_RISE  level = pin-high (mask)
    //   sChannelBuf[3*i + 1] = T0_FALL  level = '1' ? high : low
    //   sChannelBuf[3*i + 2] = T1_FALL  level = pin-low (0)
    // Each word is written verbatim to GPIO->MPIN[0]; MASK (set in init())
    // ensures only the data pin is affected, so "high" = mask, "low" = 0.
    void encodeChunk(PixelController<RGB_ORDER>& pixels,
                     u32 chunk_bits, bool first_byte) FL_NO_EXCEPT {
        const u32 mask = mPinMask;
        u32* stream = &sChannelBuf[0];

        if (first_byte) {
            pixels.preStepFirstByteDithering();
        }

        u32 bit_idx = 0;
        while (bit_idx < chunk_bits && pixels.has(1)) {
            pixels.stepDithering();
            u8 b0 = pixels.loadAndScale0();
            u8 b1 = pixels.loadAndScale1();
            u8 b2 = pixels.loadAndScale2();

            // RGB-only path; RGBW left as a TODO for v1.
            encodeByte(b0, stream, bit_idx, chunk_bits, mask);
            bit_idx += 8 + XTRA0;
            if (bit_idx >= chunk_bits) break;
            encodeByte(b1, stream, bit_idx, chunk_bits, mask);
            bit_idx += 8 + XTRA0;
            if (bit_idx >= chunk_bits) break;
            encodeByte(b2, stream, bit_idx, chunk_bits, mask);
            bit_idx += 8 + XTRA0;

            pixels.advanceData();
        }

        // Pad tail of chunk: hold the pin low across all three events.
        for (; bit_idx < chunk_bits; ++bit_idx) {
            stream[3 * bit_idx + 0] = 0u;
            stream[3 * bit_idx + 1] = 0u;
            stream[3 * bit_idx + 2] = 0u;
        }

        // TODO(2842): RGBW support - the FlexIO driver uses
        // loadAndScaleRGBW; mirror that here once RGB is hardware-validated.
        (void)first_byte;
    }

    static inline void encodeByte(u8 byte_value,
                                  u32* stream,
                                  u32 bit_idx,
                                  u32 chunk_bits,
                                  u32 mask) FL_NO_EXCEPT {
        // MSB first (WS2812 standard). Three interleaved words per bit.
        for (u8 i = 0; i < 8; ++i) {
            const u32 dst = bit_idx + i;
            if (dst >= chunk_bits) return;
            const bool bit = (byte_value & (0x80u >> i)) != 0;
            u32* w = &stream[3 * dst];
            w[0] = mask;              // T0_RISE: pin goes high
            w[1] = bit ? mask : 0u;   // T0_FALL: '1' stays high, '0' drops low
            w[2] = 0u;                // T1_FALL: pin low (end of bit)
        }
    }

    // Configure SCT: 16-bit unified counter, MATCH0=T0_RISE, MATCH1=T0_FALL,
    // MATCH2=T1_FALL, MATCH3=BIT_END -> reload + DMAREQ pulse.
    // UM11029 §16.6.1 CONFIG, §16.6.6 MATCH, §16.6.21 EVx_CTRL.
    void configureSct() FL_NO_EXCEPT {
        // One 32-bit unified up-counter (UNIFY), clocked from the bus clock
        // (CLKMODE=0). Keep it halted and cleared while we program it.
        SCT0->CTRL   = SCT_CTRL_HALT_L(1) | SCT_CTRL_CLRCTR_L(1);
        SCT0->CONFIG = SCT_CONFIG_UNIFY(1);
        // Force the state machine to state 0. Every event below is enabled
        // only in state 0 (STATEMSK bit 0); if the SCT powered up in another
        // state the matches would never fire - no DMA requests, no LIMIT
        // reset, and the counter would free-run instead of looping to kBitEnd.
        SCT0->STATE = SCT_STATE_STATE_L(kSctStateDefault);

        // Compare values (and their reload shadows) for the four edge counts.
        SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE]   = kT0Rise;
        SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL] = kT0Fall;
        SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL] = kT1Fall;
        SCT0->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_END]    = kBitEnd;
        SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE]   = kT0Rise;
        SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL] = kT0Fall;
        SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL] = kT1Fall;
        SCT0->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_END]    = kBitEnd;

        // Each event fires purely on its own match register, active in the
        // SCT's power-up state (state 0). UM11029 §16.6.21 EVx_CTRL.
        for (u32 ev = 0; ev < 4; ++ev) {
            SCT0->EV[ev].STATE = SCT_EV_STATE_STATEMSKn(1u << kSctStateDefault);
            SCT0->EV[ev].CTRL  = SCT_EV_CTRL_MATCHSEL(ev) |
                                 SCT_EV_CTRL_COMBMODE(kSctCombModeMatch);
        }

        // The MATCH_END event limits (auto-resets) the counter, so the bit
        // clock free-runs: 0 -> kBitEnd -> 0 -> ... UM11029 §16.6.11 LIMIT.
        SCT0->LIMIT =
            SCT_LIMIT_LIMMSK_L(1u << FASTLED_LPC_PWM_DMA_SCT_MATCH_END);

        // The three edge events (RISE, T0_FALL, T1_FALL) each drive SCT DMA
        // request 0 - one pulse per event, i.e. 3 MPIN transfers per WS2812
        // bit. MATCH_END is deliberately excluded (it only reloads the
        // counter). The SCT has only two DMA request lines, which is why all
        // three edges share DMAREQ0. UM11029 §16.6.18.
        SCT0->DMAREQ0 =
            SCT_DMAREQ0_DEV_0((1u << FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE) |
                              (1u << FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL) |
                              (1u << FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL));

        // No SCT CPU interrupts: the driver consumes events via DMA only.
        // Leaving EVEN clear avoids a lockup in a default SCT_IRQ handler if
        // that NVIC line is ever enabled elsewhere. UM11029 §16.6.16 EVEN.
        SCT0->EVEN = 0u;
        // Clear any stale event flags so the first run starts clean (W1C).
        SCT0->EVFLAG = SCT_EVFLAG_FLAG(0xFFu);
    }

    // Configure the single DMA channel writing to GPIO->MPIN[0].
    // UM11029 §17.4 channel descriptor, §17.5 XFERCFG.
    void configureDma() FL_NO_EXCEPT {
        DMA0->CTRL = DMA_CTRL_ENABLE(1);
        // SRAMBASE points at the channel descriptor table. Bits 8:0 are
        // reserved (UM11029 §17.6.2), so the block is 512-byte aligned - see
        // the FL_ALIGNAS(512) sDmaDescriptors declaration. The cast is safe:
        // the array is over-aligned relative to the register's requirement.
        DMA0->SRAMBASE = reinterpret_cast<u32>(sDmaDescriptors);  // ok reinterpret cast - LPC DMA SRAMBASE requires raw descriptor-table address

        const u32 ch = FASTLED_LPC_PWM_DMA_BASECH;

        // Route SCT DMA-request-0 to this channel's hardware-trigger input.
        INPUTMUX->DMA_ITRIG_INMUX[ch] =
            INPUTMUX_DMA_ITRIG_INMUX_INP(kInmuxSrcSctDma0);

        // Hardware-triggered, rising-edge, one transfer per trigger (UM11029
        // §17.6.1). TRIGPOL MUST be active-high: the SCT DMA-request line
        // pulses HIGH on each event, so TRIGPOL=0 (active-low) would never
        // recognize the trigger and the channel would stall forever.
        // PERIPHREQEN is left 0 on purpose: the channel's fixed peripheral
        // request line (a USART/SPI DMA request for low channel indices, e.g.
        // the debug-console UART on channel 0) must NOT also fire this
        // channel - only the SCT trigger should.
        DMA0->CHANNEL[ch].CFG =
            DMA_CHANNEL_CFG_HWTRIGEN(1)  |
            DMA_CHANNEL_CFG_TRIGPOL(kDmaTrigPolRising)  |
            DMA_CHANNEL_CFG_TRIGTYPE(kDmaTrigEdge)      |
            DMA_CHANNEL_CFG_TRIGBURST(0);   // single transfer per trigger

        // ENABLESET is write-1-to-set; the descriptor is armed by SETVALID at
        // chunk start (startDmaChunk()).
        DMA0->COMMON[0].ENABLESET = DMA_COMMON_ENABLESET_ENA(1u << ch);
    }

    // Arm one chunk: point the channel's descriptor at the interleaved
    // stream, set XFERCFG with the chunk's word-count, then release the SCT.
    void startDmaChunk(u32 chunk_bits) FL_NO_EXCEPT {
        const u32 ch    = FASTLED_LPC_PWM_DMA_BASECH;
        const u32 words = 3u * chunk_bits;  // 3 MPIN writes per WS2812 bit

        // Populate the channel head descriptor (UM11029 §17.4.3). The DMA
        // reconstructs the running address from the *end* address: with
        // SRCINC=+1 word the last source word is &stream[words-1]; with
        // DSTINC=0 the destination end address is simply the MPIN register.
        // No RELOAD is used, so xfercfg/link stay zero - the live transfer
        // config is written to CHANNEL[ch].XFERCFG below.
        LpcDmaDescriptor& d = sDmaDescriptors[ch];
        d.srcEndAddr     = &sChannelBuf[words - 1u];
#if defined(FASTLED_LPC_PWM_DMA_DEBUG_RAM_DST)
        // DIAGNOSTIC (TODO(2842)): redirect the DMA destination to RAM
        // (sChannelBuf[0]) instead of GPIO->MPIN[0]. DSTINC=0, so every word
        // lands in that one location - contents are irrelevant, we only watch
        // DMA0->COMMON[0].ERRINT. If ERRINT stops asserting with this defined,
        // the LPC845 DMA cannot master-access the GPIO at 0xA0000000 and the
        // driver's DMA-to-GPIO premise must change. If ERRINT still asserts,
        // the fault is elsewhere (descriptor/source).
        d.dstEndAddr     = reinterpret_cast<void*>(reinterpret_cast<u32>(&sChannelBuf[0]));  // ok reinterpret cast - diagnostic RAM destination
#else
        d.dstEndAddr     = reinterpret_cast<void*>(reinterpret_cast<u32>(&GPIO->MPIN[0]));  // ok reinterpret cast - LPC DMA descriptor requires raw destination address
#endif
        d.xfercfg        = 0u;
        d.linkToNextDesc = nullptr;

        // Configure XFERCFG (UM11029 §17.5): valid config, 32-bit words,
        // source auto-increments through the stream, destination fixed on the
        // MPIN register, and XFERCOUNT = words-1 (the field is N-1 encoded).
        // RELOAD/SWTRIG/SETINT stay clear - single HW-triggered pass, polled.
        const u32 xfercfg =
            DMA_CHANNEL_XFERCFG_CFGVALID(1) |
            DMA_CHANNEL_XFERCFG_WIDTH(kDmaWidth32Bit) |
            DMA_CHANNEL_XFERCFG_SRCINC(kDmaIncOneUnit) |
            DMA_CHANNEL_XFERCFG_DSTINC(kDmaIncNone) |
            DMA_CHANNEL_XFERCFG_XFERCOUNT(words - 1u);
        DMA0->CHANNEL[ch].XFERCFG = xfercfg;

        // SETVALID flips the descriptor's VALIDPENDING -> VALID, arming the
        // channel for the next hardware trigger from the SCT.
        DMA0->COMMON[0].SETVALID = DMA_COMMON_SETVALID_SV(1u << ch);

        // Zero the counter AND release HALT in one write (CLRCTR_L is
        // self-clearing; every other CTRL field is 0 = up-count, no
        // prescale). Starting each chunk at COUNT=0 guarantees the counter is
        // always below the MATCH_END limit, so the reset match is reached
        // from below every time. Without this, waitDmaChunk() halts the
        // counter at an arbitrary point; if it was ever frozen at/after the
        // limit value, releasing HALT would let it free-run to 2^32 (the
        // "counts all the way up" hang) instead of looping at kBitEnd.
        SCT0->CTRL = SCT_CTRL_CLRCTR_L(1);
    }

    void waitDmaChunk() FL_NO_EXCEPT {
        const u32 mask =
            DMA_COMMON_ACTIVE_ACT(1u << FASTLED_LPC_PWM_DMA_BASECH);
        // Spin until the channel reports ACTIVE=0 (transfer done).
        while ((DMA0->COMMON[0].ACTIVE & mask) != 0) {
            // CPU-free transmission goal: user-level code may run on the
            // outer task; this driver simply blocks at the FastLED.show()
            // boundary. TODO(2842): replace busy-wait with WFI + DMA done
            // IRQ once IRQ handler is wired.
        }
        // Halt SCT in preparation for the next chunk.
        SCT0->CTRL |= SCT_CTRLL_HALT_L_MASK;
    }
};

// Static channel buffer definition. C++11 requires an out-of-class
// definition for non-inline static data members; we keep it in-header
// via a templated dummy to avoid an extra .cpp file. TODO(2842): move
// to a real .cpp when this driver gains a build-system entry.
template <u8 P, typename T, EOrder R, int X, bool F, int W>
u32 ClocklessController<P, T, R, X, F, W>::sChannelBuf
    [3 * FASTLED_LPC_PWM_DMA_CHUNK_BITS];

// DMA descriptor table definition (see sChannelBuf note above). FL_ALIGNAS is
// repeated here so the emitted symbol carries the 512-byte alignment.
template <u8 P, typename T, EOrder R, int X, bool F, int W>
FL_ALIGNAS(512) LpcDmaDescriptor ClocklessController<P, T, R, X, F, W>::sDmaDescriptors
    [FASTLED_LPC_PWM_DMA_BASECH + 1];

}  // namespace fl

#endif  // FL_IS_ARM_LPC && FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA
#endif  // __INC_CLOCKLESS_ARM_LPC_PWM_DMA_H
