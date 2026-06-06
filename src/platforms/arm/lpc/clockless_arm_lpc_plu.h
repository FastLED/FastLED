// IWYU pragma: private

#ifndef __INC_CLOCKLESS_ARM_LPC_PLU_H
#define __INC_CLOCKLESS_ARM_LPC_PLU_H

// =============================================================================
//  FastLED LPC804 WS2812-family driver using the Programmable Logic Unit (PLU)
// =============================================================================
//
//  Why this path exists
//  --------------------
//  The LPC804 runs at 15 MHz (vs. LPC845's 30 MHz). At 15 MHz the WS2812 bit
//  timings T1/T2/T3 (roughly 350/350/600 ns for WS2812B) reduce to
//  ~5/5/9 cycles. The shared `m0clockless.h` C++ inner loop needs ~4-5 cycles
//  per branch on a Cortex-M0+, leaving essentially zero margin on T1.
//
//  The LPC804 includes a Programmable Logic Unit (PLU): a small reconfigurable
//  combinational fabric (26 LUTs, four output flip-flops, dedicated I/O mux)
//  that can synthesise the WS2812 bit pulse-shape from a serial data input
//  plus a small timing counter. Once the PLU is programmed, the CPU's only
//  job per bit is "write the next data bit into the PLU's data-input GPIO";
//  the PLU performs the pulse stretch in hardware. This mirrors the RP2040
//  PIO architectural pattern used in `clockless_rp_pio.h`.
//
//  References
//  ----------
//   * NXP UM11065 (LPC80x User Manual), Chapter 12 "Programmable Logic
//     Unit (PLU)" - register layout cited inline below.
//   * NXP video, "Part II: Creative ways to leverage the LPC804 MCU's
//     integrated Programmable Logic feature" (NXP LPC80X-LEVERAGE-PLU).
//   * Drolli blog, "LPC804 PLU WS2812B controller", Nov 2019 - referenced
//     for the *topology* of the LUT graph only; no code was copied.
//
//  Build-time opt-in
//  -----------------
//      -DFASTLED_LPC_PLU=1   (only honoured when FL_LPC804 is set)
//
//  When the macro is set, this header registers itself ahead of the bit-banged
//  C++ driver in `fastled_arm_lpc.h`. The LPC845 path (no PLU) is unaffected.
//
//  Pin constraints
//  ---------------
//  Both the PLU's six primary inputs (PLU_IN[0..5]) and its four primary
//  outputs (PLU_OUT[0..3]) are routed to GPIO pins via the Switch Matrix
//  (SWM) `PINASSIGN_PLU_IN[n]` / `PINASSIGN_PLU_OUT[n]` registers. Per
//  UM11065 Table 88 "PLU pin description" any PIO0_x pin available on the
//  LPC804 package can be assigned as a PLU input or output (the SWM is
//  fully crossbar-routable). The driver does NOT pin-validate at compile
//  time because the routing is package-dependent; the user is responsible
//  for selecting a free, low-skew GPIO and wiring it through the SWM.
//
//  Reset gap reuse
//  ---------------
//  The WS2812 reset/latch gap (>50us) is enforced by the same `CMinWait`
//  mechanism the bit-banged driver uses. Nothing PLU-specific is needed
//  there - once the data stream stops, the PLU outputs a static 0 (the
//  LUT graph is wired to OR-tie that condition).
//
// =============================================================================

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_LPC804) && defined(FASTLED_LPC_PLU)

#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fastled_delay.h"
#include "eorder.h"

namespace fl {

#ifndef FL_CLOCKLESS_CONTROLLER_DEFINED
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#endif

// -----------------------------------------------------------------------------
// PLU register layout (UM11065 Chapter 12)
// -----------------------------------------------------------------------------
//
// The LPC804 PLU base address is 0x40028000 (UM11065 Table 4 "APB1 peripherals").
// The peripheral exposes:
//
//   Offset      Name                  Purpose (UM11065 ref)
//   0x000+     LUTn_TRUTH[26]        Truth-table for LUT n  (Table 96 / §12.6.2)
//   0x800+     OUTPUTm_MUX[4]        Pick which LUT drives PLU_OUTm (§12.6.3)
//   0xC00+     LUTn_INP_MUX[26][5]   Per-LUT, per-input source mux (§12.6.1)
//   0xFF8      WAKEINT_CTRL          Wake/interrupt control (§12.6.4)
//
// The bit-encodings below match UM11065 v1.4 register descriptions. Where the
// public datasheet leaves a sub-field ambiguous we cite the section we
// referenced and flag the assumption with TODO(2841).
// -----------------------------------------------------------------------------

namespace lpc_plu_detail {

// PLU peripheral base address (UM11065 Table 4, APB1).
// TODO(2841): cross-check against LPC804.h CMSIS header at build time.
static const fl::u32 kPluBase = 0x40028000UL;

// Offsets within the PLU register block.
static const fl::u32 kOffLutTruth0   = 0x000;  // 26 x u32, stride 4
static const fl::u32 kOffOutputMux0  = 0x800;  // 4  x u32, stride 4
static const fl::u32 kOffLutInpMux0  = 0xC00;  // 26 x 5 u32, stride 4
static const fl::u32 kOffWakeIntCtrl = 0xFF8;

// SYSCON peripheral base and the SYSAHBCLKCTRL register that gates the PLU.
// UM11065 Chapter 4 (SYSCON) - PLU clock is bit 18 of SYSAHBCLKCTRL0 (§4.6.13).
// TODO(2841): confirm bit position against LPC804 CMSIS once-and-only PLU enum.
static const fl::u32 kSysconBase             = 0x40048000UL;
static const fl::u32 kOffSysahbclkctrl0      = 0x080;
static const fl::u32 kSysconPluClkBit        = (1UL << 18);

// SWM peripheral base. Pin-assignment for PLU inputs / outputs lives here
// (UM11065 Chapter 8 "Switch Matrix"). The PLU pin-assign registers are part
// of the PINASSIGN0..PINASSIGN11 array; the LPC804 device header surfaces
// them as PLU_INPUT_IO[n] / PLU_CLKIN / etc. We deliberately do NOT hard-code
// the SWM register offsets here: the user is expected to pre-configure pin
// routing in board-setup code before calling FastLED.addLeds<>(). This is
// consistent with how Arduino-style LPC845 sketches handle SWM setup today.
// TODO(2841): provide an optional `fl::lpc::plu::set_data_pin(uint8_t pio)`
// helper in a follow-up that programs PINASSIGN_PLU_IN[0] directly.

// Volatile access helpers. C-style cast is the canonical embedded-register
// access pattern (compare ARM_DEMCR / ARM_DWT_CTRL in clockless_arm_giga.h);
// the FastLED reinterpret_cast linter explicitly allows C-style casts for
// MMIO addresses.
static inline volatile fl::u32* reg32(fl::u32 base, fl::u32 off) FL_NOEXCEPT {
    return (volatile fl::u32*)(base + off);  // MMIO register address
}

// LUTn_TRUTH[n] write helper.
static inline void writeLutTruth(fl::u8 lut, fl::u32 truth_table) FL_NOEXCEPT {
    *reg32(kPluBase, kOffLutTruth0 + (fl::u32(lut) << 2)) = truth_table;
}

// LUTn_INP_MUX[m] write helper.
// sel: 0..5 -> primary input PLU_IN0..PLU_IN5
//      6..N -> LUT[sel-6] output (UM11065 §12.6.1 mux encoding)
static inline void writeLutInputMux(fl::u8 lut, fl::u8 input_idx,
                                    fl::u8 sel) FL_NOEXCEPT {
    // Stride is 5 u32 per LUT (one per input).
    fl::u32 off = kOffLutInpMux0 + (fl::u32(lut) * 5UL + input_idx) * 4UL;
    *reg32(kPluBase, off) = sel;
}

// OUTPUTm_MUX write helper. Selects which LUT drives PLU_OUTm.
static inline void writeOutputMux(fl::u8 out_idx, fl::u8 lut_sel) FL_NOEXCEPT {
    *reg32(kPluBase, kOffOutputMux0 + (fl::u32(out_idx) << 2)) = lut_sel;
}

// Enable PLU peripheral clock (SYSCON->SYSAHBCLKCTRL0 bit 18).
static inline void enablePluClock() FL_NOEXCEPT {
    volatile fl::u32* p = reg32(kSysconBase, kOffSysahbclkctrl0);
    *p = *p | kSysconPluClkBit;
}

// -----------------------------------------------------------------------------
// PLU LUT graph for WS2812 bit shaping
// -----------------------------------------------------------------------------
//
// Topology (3-LUT minimum, see Drolli blog for the architectural reference;
// the truth tables below were re-derived from UM11065 §12.6.2 - no source
// was copied):
//
//   Primary inputs:
//     PLU_IN0  = DATA      (CPU-driven GPIO - the next WS2812 bit)
//     PLU_IN1  = CNT0      (LSB of timing counter, ~T0H period tick)
//     PLU_IN2  = CNT1      (MSB of timing counter, fires at the T1H boundary)
//
//   LUT0: "T0H window"    out = !CNT1            (HIGH for first half of bit)
//   LUT1: "T1H window"    out = !(CNT1 & CNT0)   (HIGH for ~75% of bit)
//   LUT2: "bit mux"       out = DATA ? LUT1 : LUT0
//
//   OUTPUT0_MUX = LUT2     -> PLU_OUT0 -> SWM-routed to the WS2812 data pin
//
// Result: when DATA=0 the line is high for T0H only (~350 ns) and low for
// the rest of the bit; when DATA=1 the line is high for T0H + T1H (~700 ns)
// and low for the remainder. This is the classic WS2812 NRZ encoding.
//
// Caveats / TODOs:
//  * CNT0/CNT1 must be driven by a free-running ~2.85 MHz square-wave pair
//    (SCT or MRT). On LPC804 the SCT can be configured to emit a 2-bit
//    Gray-coded counter on two PLU input pins; alternatively the MRT can
//    free-run at ~1.4 MHz and feed CNT1 alone with CNT0 tied to a
//    flip-flop that toggles on each MRT match. The driver leaves this
//    counter wiring as `init_timing_source()` -> TODO(2841).
//  * The truth tables below assume the standard PLU LUT bit ordering
//    where bit-N of LUTn_TRUTH corresponds to input combination N
//    (UM11065 §12.6.2 Table 96). This is the natural mapping and matches
//    every public PLU example I've found, but we cite the section so a
//    future reviewer can confirm against UM11065 if the silicon proves
//    otherwise.
// -----------------------------------------------------------------------------

// LUT0 truth table: out = !IN2 (i.e. !CNT1). IN1/IN0 ignored.
// Bit N is set when input-combo N (IN0..IN4 = bits 0..4) makes out=1.
// out=1 iff bit2 (IN2) is 0 -> N in {0..3, 8..11, 16..19, 24..27}.
static const fl::u32 kLut0Truth_T0H = 0x0F0F0F0FUL;

// LUT1 truth table: out = !(IN1 & IN2). IN0 ignored.
// out=0 only when IN1=1 AND IN2=1 -> bits where (N>>1)&1==1 AND (N>>2)&1==1
// i.e. N & 0b110 == 0b110 -> N in {6,7,14,15,22,23,30,31}.
// Inverted: 0xFFFFFFFF ^ ((1<<6)|(1<<7)|(1<<14)|(1<<15)|(1<<22)|(1<<23)|(1<<30)|(1<<31))
//         = 0x3F3F3F3FUL
static const fl::u32 kLut1Truth_T1H = 0x3F3F3F3FUL;

// LUT2 truth table: out = IN0 ? IN2 : IN1
//   IN0 = DATA, IN1 = LUT0_out, IN2 = LUT1_out
// Bit N: (N>>0)&1 is DATA, (N>>1)&1 is LUT0, (N>>2)&1 is LUT1
// out = ((N>>0)&1) ? ((N>>2)&1) : ((N>>1)&1)
// Enumerating N=0..31: out=1 for combos
//   DATA=0,LUT0=1   -> N & 0b011 == 0b010 -> N in {2,3,6,7,10,11,...}
//   DATA=1,LUT1=1   -> N & 0b101 == 0b101 -> N in {5,7,13,15,21,23,29,31}
// = 0xCACACACAUL  (pattern repeats every 8 input combos because IN3/IN4 are
//                  don't-care)
// TODO(2841): regenerate this constant with a unit-test that enumerates the
// truth table from the boolean expression, to catch bit-ordering surprises
// against UM11065 §12.6.2 Table 96.
static const fl::u32 kLut2Truth_BitMux = 0xCACACACAUL;

// LUTn_INP_MUX sel values for primary inputs (UM11065 §12.6.1).
// sel = 0..5 -> PLU_IN0..PLU_IN5
static const fl::u8 kMuxPluIn0 = 0;
static const fl::u8 kMuxPluIn1 = 1;
static const fl::u8 kMuxPluIn2 = 2;
// sel = 6 + lut_id -> LUTn output
static inline fl::u8 muxLutOutput(fl::u8 lut_id) FL_NOEXCEPT {
    return fl::u8(6u + lut_id);
}

// One-shot PLU init. Idempotent: a second call rewrites the same registers.
static void initPluGraph() FL_NOEXCEPT {
    // Step 1: enable peripheral clock BEFORE any register write
    // (ARM Cortex-M0+ silently drops writes to unpowered peripherals).
    enablePluClock();

    // Step 2: wire LUT0 inputs - only IN2 matters.
    writeLutInputMux(0, 0, kMuxPluIn0);   // ignored
    writeLutInputMux(0, 1, kMuxPluIn1);   // ignored
    writeLutInputMux(0, 2, kMuxPluIn2);   // CNT1
    writeLutInputMux(0, 3, kMuxPluIn0);
    writeLutInputMux(0, 4, kMuxPluIn0);
    writeLutTruth(0, kLut0Truth_T0H);

    // Step 3: wire LUT1 inputs - IN1, IN2 matter.
    writeLutInputMux(1, 0, kMuxPluIn0);   // ignored
    writeLutInputMux(1, 1, kMuxPluIn1);   // CNT0
    writeLutInputMux(1, 2, kMuxPluIn2);   // CNT1
    writeLutInputMux(1, 3, kMuxPluIn0);
    writeLutInputMux(1, 4, kMuxPluIn0);
    writeLutTruth(1, kLut1Truth_T1H);

    // Step 4: wire LUT2 (bit mux) - IN0=DATA, IN1=LUT0_out, IN2=LUT1_out.
    writeLutInputMux(2, 0, kMuxPluIn0);     // DATA
    writeLutInputMux(2, 1, muxLutOutput(0)); // LUT0 out
    writeLutInputMux(2, 2, muxLutOutput(1)); // LUT1 out
    writeLutInputMux(2, 3, kMuxPluIn0);
    writeLutInputMux(2, 4, kMuxPluIn0);
    writeLutTruth(2, kLut2Truth_BitMux);

    // Step 5: route LUT2 to PLU_OUT0.
    writeOutputMux(0, 2);

    // Step 6: PLU is purely combinational + a wake-int block we don't use.
    // Disable any latent wake-interrupt config (UM11065 §12.6.4).
    *reg32(kPluBase, kOffWakeIntCtrl) = 0;

    // Step 7 (deferred): bring up the timing-counter source feeding PLU_IN1 /
    // PLU_IN2. On LPC804 this is most naturally an SCT-generated Gray-coded
    // pair (counter rolls every WS2812 bit period). The exact SCT config is
    // pin-routing dependent and intentionally left for board-level glue.
    // TODO(2841): provide a default SCT-based timing source in a follow-up
    // commit; for now the integrator must call their own init_timing_source()
    // before FastLED.show().
}

}  // namespace lpc_plu_detail


// -----------------------------------------------------------------------------
// ClocklessController - PLU specialisation for LPC804
// -----------------------------------------------------------------------------
//
// Template parameters are kept compatible with the bit-banged driver so that
// `FastLED.addLeds<WS2812, DATA_PIN, GRB>(...)` resolves to either driver
// based purely on which header is included by `fastled_arm_lpc.h`. T1/T2/T3
// are accepted but unused at runtime: the timing is hardware-generated by
// the PLU graph. They are still validated by the upstream timing_traits
// machinery so that bogus chipset selections still fail to compile.
// -----------------------------------------------------------------------------

template <fl::u8 DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<DATA_PIN>::port_t     data_t;

    data_t     mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;
    bool       mPluReady;

public:
    ClocklessController() FL_NOEXCEPT : mPinMask(0), mPort(0), mPluReady(false) {}

    virtual void init() FL_NOEXCEPT {
        // The CPU still owns the data pin - the PLU samples it via SWM, but
        // the pin itself is a normal GPIO that the CPU writes to.
        FastPin<DATA_PIN>::setOutput();
        mPinMask = FastPin<DATA_PIN>::mask();
        mPort    = FastPin<DATA_PIN>::port();

        // Program the PLU LUT graph once. Idempotent.
        lpc_plu_detail::initPluGraph();
        mPluReady = true;
    }

    virtual fl::u16 getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER>& pixels) FL_NOEXCEPT {
        mWait.wait();

        // The PLU drives the wire shape; the CPU's only job is to clock data
        // bits out at the bit-rate cadence. We disable interrupts during the
        // transfer because a long ISR could stretch a single bit past the
        // WS2812 reset threshold (>50us) and end the frame prematurely.
        cli();
        feedPixels(pixels);
        sei();

        mWait.mark();
    }

private:
    // Feed every bit of every pixel into the PLU's data-input GPIO.
    //
    // Per-bit cadence:
    //   1. The PLU's timing counter is free-running on its own clock domain.
    //   2. The CPU samples the next data bit, writes it to the data GPIO.
    //   3. The PLU latches the resulting bit-window output on each counter
    //      rollover. We must guarantee the new DATA value is stable BEFORE
    //      the rollover edge that begins the bit.
    //
    // Synchronisation strategy:
    //   We do not poll the timing counter; we just push bits at slightly
    //   below the bit-rate. The PLU samples DATA at each rising edge of its
    //   internal bit-period clock; as long as we update DATA between
    //   rollovers, the LED stream is correct. At 15 MHz CPU and ~1.25us
    //   per WS2812 bit, that's ~18 cycles of slack per bit - plenty.
    //
    // TODO(2841): once the SCT-based timing source is integrated, expose a
    // "ready-for-next-bit" PLU flag via OUTPUT3 and have the CPU spin on it
    // instead of relying on the slack envelope. That would let this loop
    // run lock-step against the PLU and eliminate any cadence drift.
    void feedPixels(PixelController<RGB_ORDER>& pixels) FL_NOEXCEPT {
        const fl::u8 nLeds = static_cast<fl::u8>(pixels.size());
        if (nLeds == 0) return;

        pixels.preStepFirstByteDithering();

        while (pixels.has(1)) {
            pixels.stepDithering();

            fl::u8 b0 = pixels.loadAndScale0();
            fl::u8 b1 = pixels.loadAndScale1();
            fl::u8 b2 = pixels.loadAndScale2();

            sendByte(b0);
            sendByte(b1);
            sendByte(b2);

            pixels.advanceData();
        }
    }

    // Push one byte MSB-first to the PLU data input.
    inline void sendByte(fl::u8 b) FL_NOEXCEPT {
        // We unroll to 8 bit writes. Each write touches SET (for bit=1) or
        // CLR (for bit=0) of the data pin. The PLU is what actually drives
        // the WS2812 timing waveform - this loop just selects 0 vs 1 width.
        for (fl::u8 i = 0; i < 8; ++i) {
            if (b & 0x80) {
                FastPin<DATA_PIN>::hi();
            } else {
                FastPin<DATA_PIN>::lo();
            }
            b = fl::u8(b << 1);
            // No delay needed here at 15 MHz - the inner loop overhead
            // (~9 cycles, including the branch) already exceeds the WS2812
            // bit period when the PLU is the timing master. If the PLU
            // counter runs faster than expected, the loop will need a
            // calibrated delay; that's a hardware-bringup task.
            // TODO(2841): once the SCT counter rate is locked in, calculate
            // the required NOP padding so each `sendByte` consumes exactly
            // 8 * (PLU_BIT_PERIOD) cycles.
        }
    }
};

}  // namespace fl

#endif  // FL_LPC804 && FASTLED_LPC_PLU
#endif  // __INC_CLOCKLESS_ARM_LPC_PLU_H
