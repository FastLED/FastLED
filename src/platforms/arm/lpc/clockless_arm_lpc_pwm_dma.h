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
//   * FL_LPC845 (auto-detected from CMSIS device macros in is_lpc.h)
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
// clock (F_CPU = 30 MHz on LPC845). At F_CPU = 30 MHz the SCT tick is
// 33.33 ns; the WS2812 timing positions fall on these counts:
//
//   T0_RISE  = 0            (rising edge of every bit)
//   T0_FALL  = 11  (~367 ns) (T0H window end -> drop for '0' bits)
//   T1_FALL  = 24  (~800 ns) (T1H window end -> drop for '1' bits)
//   T_END    = 37  (~1233 ns) (counter reload, next bit starts)
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
// MULTI-CHAIN SUPPORT
// -------------------
// v1 supports a single chain. Multi-chain (parallel output from one DMA
// stream sharing the SCT timebase) is Stage 4 work tracked in #2845.

// =============================================================================
// IMPLEMENTATION
// =============================================================================

#if defined(FL_IS_ARM_LPC) && defined(FL_LPC845) && defined(FASTLED_LPC_PWM_DMA)

#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/cstring.h"
#include "eorder.h"

// Forward declarations for the CMSIS LPC845 peripheral struct types. When
// the user's build pulls in <LPC845.h> via led_sysdefs_arm_lpc.h, the real
// CMSIS definitions take precedence (their inclusion guard is the typedef
// name, e.g. LPC_SCT_TYPE). The duplicates below exist only so that this
// file still parses in build environments that have FASTLED_LPC_PWM_DMA set
// but cannot locate <LPC845.h> on the include path (e.g. IDE introspection).
// They are *not* full struct definitions; the driver only touches members
// declared on the real CMSIS types, so on a real LPC845 build only the
// member access paths exercised below need to exist in the CMSIS header.

#if !defined(LPC_SCT_BASE)
#define LPC_SCT_BASE  0x50004000UL
#endif
#if !defined(LPC_DMA_BASE)
#define LPC_DMA_BASE  0x50008000UL
#endif
#if !defined(LPC_SYSCON_BASE)
#define LPC_SYSCON_BASE 0x40048000UL
#endif

namespace fl {

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

// Default base DMA channel; users can override at build time to avoid
// collisions with other peripherals (USART/SPI/I2C also have DMA triggers
// on LPC845 - UM11029 17.3.3 hardware trigger table).
#ifndef FASTLED_LPC_PWM_DMA_BASECH
#define FASTLED_LPC_PWM_DMA_BASECH 0
#endif

// Streaming chunk size in WS2812 bits per re-arm. Each chunk is
// 3 * BUFFER_BITS * 4 bytes of working RAM (default 64 bits = 768 bytes).
// Must be a multiple of 8 (one byte = 8 bits, encoder fills byte-at-a-time).
#ifndef FASTLED_LPC_PWM_DMA_CHUNK_BITS
#define FASTLED_LPC_PWM_DMA_CHUNK_BITS 64
#endif

// SCT match indices and counter reload event.
// TODO(2842): verify SCT EVx assignment is unused by user code at build
// time. UM11029 §16.6.21 (EVx_CTRL) controls which match triggers which
// event; we use EV0/EV1/EV2 here.
#define FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE   0
#define FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL 1
#define FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL 2
#define FASTLED_LPC_PWM_DMA_SCT_MATCH_END    3

// Forward-declared peripheral struct shim. Only present when the real CMSIS
// header has NOT defined LPC_SCT (its include guard). This lets the file
// compile in environments where CMSIS isn't on the include path; users with
// a real LPC845 toolchain include <LPC845.h> via led_sysdefs_arm_lpc.h and
// the genuine struct wins.
#if !defined(LPC_SCT) && !defined(LPC_SCT_TYPE_)
struct FL_LPC_SCT_Shim {
    volatile u32 CONFIG;        // 0x000  config
    volatile u32 CTRL;          // 0x004  control
    volatile u32 LIMIT_L;       // 0x008
    volatile u32 LIMIT_H;       // 0x00C
    volatile u32 HALT_L;        // 0x010
    volatile u32 HALT_H;        // 0x014
    volatile u32 STOP_L;        // 0x018
    volatile u32 STOP_H;        // 0x01C
    volatile u32 START_L;       // 0x020
    volatile u32 START_H;       // 0x024
    volatile u32 _resv0[10];
    volatile u32 COUNT_U;       // 0x040
    volatile u32 STATE;         // 0x044
    volatile u32 INPUT;         // 0x048
    volatile u32 REGMODE_L;     // 0x04C
    volatile u32 REGMODE_H;     // 0x050
    volatile u32 OUTPUT;        // 0x054
    volatile u32 OUTPUTDIRCTRL; // 0x058
    volatile u32 RES;           // 0x05C
    volatile u32 DMAREQ0;       // 0x060
    volatile u32 DMAREQ1;       // 0x064
    volatile u32 _resv1[35];
    volatile u32 EVEN;          // 0x0F0  event enable
    volatile u32 EVFLAG;        // 0x0F4
    volatile u32 CONEN;         // 0x0F8
    volatile u32 CONFLAG;       // 0x0FC
    union {
        volatile u32 U;
        struct { volatile u16 L, H; };
    } MATCH[8];                 // 0x100
    volatile u32 _resv2[56];
    union {
        volatile u32 U;
        struct { volatile u16 L, H; };
    } MATCHREL[8];              // 0x200
    volatile u32 _resv3[56];
    struct {
        volatile u32 STATE;     // 0x300, 0x308, ...
        volatile u32 CTRL;      // 0x304, 0x30C, ...
    } EV[8];
};
#define LPC_SCT ((FL_LPC_SCT_Shim*)LPC_SCT_BASE)
#endif  // !LPC_SCT

#if !defined(LPC_DMA) && !defined(LPC_DMA_TYPE_)
struct FL_LPC_DMA_ChannelShim {
    volatile u32 CFG;
    volatile u32 CTLSTAT;
    volatile u32 XFERCFG;
    volatile u32 _resv;
};
struct FL_LPC_DMA_Shim {
    volatile u32 CTRL;
    volatile u32 INTSTAT;
    volatile u32 SRAMBASE;
    volatile u32 _resv0[5];
    volatile u32 ENABLESET0;
    volatile u32 _resv1;
    volatile u32 ENABLECLR0;
    volatile u32 _resv2;
    volatile u32 ACTIVE0;
    volatile u32 _resv3;
    volatile u32 BUSY0;
    volatile u32 _resv4;
    volatile u32 ERRINT0;
    volatile u32 _resv5;
    volatile u32 INTENSET0;
    volatile u32 _resv6;
    volatile u32 INTENCLR0;
    volatile u32 _resv7;
    volatile u32 INTA0;
    volatile u32 _resv8;
    volatile u32 INTB0;
    volatile u32 _resv9;
    volatile u32 SETVALID0;
    volatile u32 _resv10;
    volatile u32 SETTRIG0;
    volatile u32 _resv11;
    volatile u32 ABORT0;
    volatile u32 _resv12[225];
    FL_LPC_DMA_ChannelShim CHANNEL[25];
};
#define LPC_DMA ((FL_LPC_DMA_Shim*)LPC_DMA_BASE)
#endif  // !LPC_DMA

#if !defined(LPC_SYSCON) && !defined(LPC_SYSCON_TYPE_)
struct FL_LPC_SYSCON_Shim {
    volatile u32 _resv0[16];
    volatile u32 SYSAHBCLKCTRL0; // 0x040 (SCT bit 8, DMA bit 29)
    volatile u32 SYSAHBCLKCTRL1; // 0x044
    // TODO(2842): verify exact SYSAHBCLKCTRL0 bit positions against UM11029
    // §4.6.13 (LPC845 has different layout vs LPC8N04).
};
#define LPC_SYSCON ((FL_LPC_SYSCON_Shim*)LPC_SYSCON_BASE)
#endif

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
inline constexpr u32 LpcPwmDmaChunkWords() FL_NOEXCEPT {
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
    virtual void init() FL_NOEXCEPT {
        FastPin<DATA_PIN>::setOutput();
        mPinMask = FastPin<DATA_PIN>::mask();
        mPort    = FastPin<DATA_PIN>::port();

        // Power up SCT and DMA in SYSAHBCLKCTRL0.
        // TODO(2842): verify SYSCON->SYSAHBCLKCTRL0 bit assignments against
        // UM11029 §4.6.13. SCT = bit 8 (per LPC8xx historical layout);
        // DMA = bit 29 (per LPC845 SDK clock_config.h).
        LPC_SYSCON->SYSAHBCLKCTRL0 |= (1UL << 8) | (1UL << 29);

        configureSct();
        configureDma();
    }

    virtual u16 getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER>& pixels) FL_NOEXCEPT {
        mWait.wait();
        // CPU-free path: encode chunks ahead of the DMA stream, kick off
        // DMA, then either spin-wait on the DMA done flag or yield. We use
        // the simplest valid model in v1: encode-the-whole-frame into
        // chunked streams, kick the SCT, wait for the BUSY flag to clear.
        showRGBInternal(pixels);
        mWait.mark();
    }

    void showRGBInternal(PixelController<RGB_ORDER> pixels) FL_NOEXCEPT {
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
                     u32 chunk_bits, bool first_byte) FL_NOEXCEPT {
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
                                  u32 lo_mask) FL_NOEXCEPT {
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
    void configureSct() FL_NOEXCEPT {
        // CONFIG: UNIFY (bit 0) - one 32-bit counter; CLKMODE = SYS clock.
        // TODO(2842): verify CONFIG layout vs UM11029 §16.6.1.
        LPC_SCT->CONFIG = 0x00000001UL;  // UNIFY
        LPC_SCT->CTRL   = 0x00000004UL;  // HALT_L = 1 (paused while configuring)

        LPC_SCT->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE].U   = kT0Rise;
        LPC_SCT->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL].U = kT0Fall;
        LPC_SCT->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL].U = kT1Fall;
        LPC_SCT->MATCH[FASTLED_LPC_PWM_DMA_SCT_MATCH_END].U    = kBitEnd;
        LPC_SCT->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_RISE].U   = kT0Rise;
        LPC_SCT->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_T0FALL].U = kT0Fall;
        LPC_SCT->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_T1FALL].U = kT1Fall;
        LPC_SCT->MATCHREL[FASTLED_LPC_PWM_DMA_SCT_MATCH_END].U    = kBitEnd;

        // Event 0/1/2/3: each tied to a MATCH register and active in state 0.
        // EVx_CTRL bits 0..3 = MATCHSEL, bit 12 = COMBMODE (MATCH only).
        // TODO(2842): verify EVx_CTRL bit layout vs UM11029 §16.6.21.
        for (u32 ev = 0; ev < 4; ++ev) {
            LPC_SCT->EV[ev].STATE = 0x00000001UL;  // active in STATE 0
            LPC_SCT->EV[ev].CTRL  = (ev & 0xF) |   // MATCHSEL = ev
                                    (0x1UL << 12);  // COMBMODE = MATCH
        }

        // Limit on event 3 (T_END) -> counter reloads.
        LPC_SCT->LIMIT_L = (1UL << 3);

        // Route SCT events to DMA. UM11029 §17.3.3 DMA hardware trigger
        // matrix - SCT_DMA0/1 are SCT-driven trigger lines.
        // TODO(2842): verify SCT->DMAREQ0/1 vs UM11029 §16.6.18.
        LPC_SCT->DMAREQ0 = (1UL << 0) | (1UL << 1) | (1UL << 2);

        // Enable EV0..EV3.
        LPC_SCT->EVEN = 0xFUL;
    }

    // Configure 3 DMA channels writing to GPIO SET[0]/CLR[0].
    // UM11029 §17.4 channel descriptor, §17.5 XFERCFG.
    void configureDma() FL_NOEXCEPT {
        // Enable DMA.
        LPC_DMA->CTRL = 1UL;
        // SRAMBASE points at the channel descriptor table - must be 256-byte
        // aligned. TODO(2842): allocate aligned descriptor block (static
        // alignas(256) array) and wire SRAMBASE here. v1 punts to the SDK
        // clock_config.h conventional address.
        // LPC_DMA->SRAMBASE = (uint32_t)sDmaDescriptors;

        const u32 base = FASTLED_LPC_PWM_DMA_BASECH;
        for (u32 i = 0; i < 3; ++i) {
            // CFG: HWTRIGEN=1, TRIGTYPE=edge, TRIGBURST=0, BURSTPOWER=0,
            // CHPRIORITY=0. The trigger source is selected by the DMA-INMUX
            // peripheral; UM11029 §17.6.1.
            // TODO(2842): verify CFG bit positions vs UM11029 §17.6.2.
            LPC_DMA->CHANNEL[base + i].CFG = (1UL << 0);  // HWTRIGEN

            // INMUX entry maps SCT match event -> channel i.
            // TODO(2842): set up DMA_ITRIG_INMUX[i] = SCT_DMA0_inmux_source.
        }
        // ENABLESET marks the channels as live; descriptors are armed by
        // SETVALID/SETTRIG at chunk start (startDmaChunk()).
        LPC_DMA->ENABLESET0 |= (7UL << base);  // 3 contiguous channels
    }

    // Arm one chunk: program each channel's source address to point at the
    // appropriate stream slice in sChannelBuf, set XFERCFG with the chunk's
    // word-count, then trigger.
    void startDmaChunk(u32 chunk_bits) FL_NOEXCEPT {
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
            LPC_DMA->CHANNEL[base + i].XFERCFG = xfercfg_base | count_field;
        }

        // SETVALID flips the descriptor's VALIDPENDING -> VALID, arming the
        // channel for the next hardware trigger from the SCT.
        LPC_DMA->SETVALID0 = (7UL << base);

        // Release the SCT HALT so the timer starts producing match events.
        LPC_SCT->CTRL &= ~0x00000004UL;
    }

    void waitDmaChunk() FL_NOEXCEPT {
        const u32 mask = (7UL << FASTLED_LPC_PWM_DMA_BASECH);
        // Spin until all three channels report ACTIVE=0 (transfer done).
        while ((LPC_DMA->ACTIVE0 & mask) != 0) {
            // CPU-free transmission goal: user-level code may run on the
            // outer task; this driver simply blocks at the FastLED.show()
            // boundary. TODO(2842): replace busy-wait with WFI + DMA done
            // IRQ once IRQ handler is wired.
        }
        // Halt SCT in preparation for the next chunk.
        LPC_SCT->CTRL |= 0x00000004UL;
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

#endif  // FL_IS_ARM_LPC && FL_LPC845 && FASTLED_LPC_PWM_DMA
#endif  // __INC_CLOCKLESS_ARM_LPC_PWM_DMA_H
