/// @file rx_flexpwm_channel.cpp.hpp
/// @brief Teensy 4.x FlexPWM input-capture RX implementation
///
/// Hardware pipeline (based on Paul Stoffregen's WS2812Capture):
///   1. FlexPWM submodule runs a free-running 16-bit counter at F_BUS_ACTUAL
///      (150 MHz on Teensy 4.x, giving ~6.67 ns per tick).
///   2. Dual-circuit capture latches rising edges into CVAL2/CVAL4 and
///      falling edges into CVAL3/CVAL5.
///   3. Each falling-edge capture triggers a DMA request after both registers
///      are valid. A Teensy DMAChannel copies the 16-bit rising and falling
///      capture values into a small circular ring (kRingPairs pairs) as one
///      32-bit minor loop.
///   4. The DMA interrupts at each half of the ring. The ISR decodes the
///      completed half into WS2812 bytes (fl::channels::rx::
///      Ws2812StreamDecoder), carrying the partial bit/byte across halves.
///      A half overwritten before decode is reported as CAPTURE_OVERRUN.
///   5. wait() declares the frame over after signal_range_max_ns of no DMA
///      progress, stops the DMA and decodes the partial last half.
///   6. Pulse widths are 16-bit deltas between captures (a counter wrap
///      inside a pulse is handled by unsigned subtraction), converted to ns
///      with a Q16.16 multiply.
///
/// Memory is the decoded bytes (3 per RGB LED) plus a 1 KB ring, independent
/// of frame length.

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

// FastLED #3219: the per-frame `[FlexPWM CFG]`/`DMA`/`RAW`/`EDGE`/`E`/
// `DECODE` FL_WARN dumps that PR #3216 enabled by default in this
// file have been removed. They were instrumentation added during the
// bimodal-edge investigation (#3066 Phase 4) and produced 18 of the
// 75 serial lines per OBJECT_FLED autoresearch frame -- ~24 % of the
// device's UART output volume. On a 100-LED frame that volume blocked
// the device's UART TX FIFO and stalled the test between patterns.
// The dual-circuit capture refactor + midpoint classifier fix landed
// in this PR series, so the bench-debug surface is no longer needed
// to characterize the previous root cause. If diagnostic dumps are
// needed again, add them behind a `-DFL_RX_FLEXPWM_VERBOSE=1` build
// flag, NOT a hardcoded `#define FL_DEBUG 1` in this header.

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"
#include "platforms/arm/teensy/teensy4_common/rx_flexpwm_channel.h"

#include "fl/stl/vector.h"
#include "fl/log/log.h"
#include "fl/stl/result.h"
#include "fl/stl/cstring.h"
#include "fl/stl/bit_cast.h"
#include "fl/channels/rx/ws2812_stream_decoder.h"

// IWYU pragma: begin_keep
#include <Arduino.h>
#include <DMAChannel.h>
#include <imxrt.h>
// IWYU pragma: end_keep

namespace fl {

// ---------------------------------------------------------------------------
// Pin-to-FlexPWM mapping table
// ---------------------------------------------------------------------------
// Each entry maps a Teensy digital pin to the FlexPWM peripheral, submodule,
// channel (A or B), DMA trigger source, and IOMUXC pin mux configuration
// needed to route the pin to the FlexPWM capture input.
//
// The capture values come from the FlexPWM CVAL registers:
//   Channel A capture: CVAL2 (rising edge) + CVAL3 (falling edge)
//   Channel B capture: CVAL4 (rising edge) + CVAL5 (falling edge)
//
// DMA trigger sources are from the i.MXRT1062 reference manual Table 4-3.

struct FlexPwmPinInfo {
    u8 pin;                            // Teensy digital pin number
    IMXRT_FLEXPWM_t *pwm;             // FlexPWM peripheral base (FLEXPWM1..4)
    u8 submodule;                      // Submodule index (0..3)
    bool channel_b;                    // false = channel A (CVAL2/3), true = channel B (CVAL4/5)
    u8 dma_source;                     // eDMA trigger source number
    volatile u32 *mux_register;        // IOMUXC mux register
    u32 mux_value;                     // Mux alt value to select FlexPWM
    volatile u32 *select_register;     // IOMUXC select input register (or nullptr)
    u32 select_value;                  // Select input value
};

namespace {

// Pin mapping derived from i.MXRT1062 reference manual and Teensy 4.x
// schematic. Pins listed for Teensy 4.0 + 4.1 unless noted.
//
// FlexPWM DMAMUX capture-read sources (from imxrt.h DMAMUX_SOURCE_FLEXPWMn_READm):
//   FLEXPWM1: SM0=32, SM1=33, SM2=34, SM3=35
//   FLEXPWM2: SM0=96, SM1=97, SM2=98, SM3=99
//   FLEXPWM3: SM0=40, SM1=41, SM2=42, SM3=43
//   FLEXPWM4: SM0=104, SM1=105, SM2=106, SM3=107

static const FlexPwmPinInfo kPinMap[] = {
    // Pin 2: FlexPWM4_SM2_A (GPIO_EMC_04, ALT1)
    {2, &IMXRT_FLEXPWM4, 2, false, DMAMUX_SOURCE_FLEXPWM4_READ2,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_04, 1,
     &IOMUXC_FLEXPWM4_PWMA2_SELECT_INPUT, 0},

    // Pin 4: FlexPWM2_SM0_A (GPIO_EMC_06, ALT1)
    {4, &IMXRT_FLEXPWM2, 0, false, DMAMUX_SOURCE_FLEXPWM2_READ0,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_06, 1,
     &IOMUXC_FLEXPWM2_PWMA0_SELECT_INPUT, 0},

    // Pin 5: FlexPWM2_SM1_A (GPIO_EMC_08, ALT1)
    {5, &IMXRT_FLEXPWM2, 1, false, DMAMUX_SOURCE_FLEXPWM2_READ1,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_08, 1,
     &IOMUXC_FLEXPWM2_PWMA1_SELECT_INPUT, 0},

    // Pin 6: FlexPWM2_SM2_A (GPIO_B0_10, ALT2)
    {6, &IMXRT_FLEXPWM2, 2, false, DMAMUX_SOURCE_FLEXPWM2_READ2,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_10, 2,
     &IOMUXC_FLEXPWM2_PWMA2_SELECT_INPUT, 1},

    // Pin 8: FlexPWM1_SM3_A (GPIO_B1_00, ALT6)
    // SELECT_INPUT=4 routes from GPIO_B1_00 (pin 8). The previous value 0
    // selected GPIO_SD_B1_00, an unrelated pad, so FlexPWM never saw the
    // pin 8 signal -- #3359 zero_capture root cause for canonical TX22->RX8.
    {8, &IMXRT_FLEXPWM1, 3, false, DMAMUX_SOURCE_FLEXPWM1_READ3,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00, 6,
     &IOMUXC_FLEXPWM1_PWMA3_SELECT_INPUT, 4},

    // Pin 22: FlexPWM4_SM0_A (GPIO_AD_B1_08, ALT1)
    // SELECT_INPUT=1 routes from GPIO_AD_B1_08 per the i.MX RT1062
    // IOMUXC_FLEXPWM4_PWMA0_SELECT_INPUT daisy table. Previous value 0
    // selected a different pad and would have silently failed FlexPWM RX
    // capture on pin 22 (same class of bug as #3402's pin 8 fix).
    {22, &IMXRT_FLEXPWM4, 0, false, DMAMUX_SOURCE_FLEXPWM4_READ0,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_08, 1,
     &IOMUXC_FLEXPWM4_PWMA0_SELECT_INPUT, 1},

    // Pin 23: FlexPWM4_SM1_A (GPIO_AD_B1_09, ALT1)
    // SELECT_INPUT=1 routes from GPIO_AD_B1_09 per the i.MX RT1062
    // IOMUXC_FLEXPWM4_PWMA1_SELECT_INPUT daisy table.
    {23, &IMXRT_FLEXPWM4, 1, false, DMAMUX_SOURCE_FLEXPWM4_READ1,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09, 1,
     &IOMUXC_FLEXPWM4_PWMA1_SELECT_INPUT, 1},

    // Pin 29: FlexPWM3_SM1_B (GPIO_EMC_31, ALT1)
    {29, &IMXRT_FLEXPWM3, 1, true, DMAMUX_SOURCE_FLEXPWM3_READ1,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_31, 1,
     nullptr, 0},

#if defined(ARDUINO_TEENSY41)
    // Teensy 4.1 only pins

    // Pin 36: FlexPWM2_SM3_A (GPIO_B1_02, ALT6)
    // SELECT_INPUT=4 routes from GPIO_B1_02 per the i.MX RT1062
    // IOMUXC_FLEXPWM2_PWMA3_SELECT_INPUT daisy table. Mirrors the same
    // GPIO_B1_xx / FlexPWM*_PWMA3 / ALT6 pattern as pin 8.
    {36, &IMXRT_FLEXPWM2, 3, false, DMAMUX_SOURCE_FLEXPWM2_READ3,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_02, 6,
     &IOMUXC_FLEXPWM2_PWMA3_SELECT_INPUT, 4},

    // Pin 49: FlexPWM1_SM2_A (GPIO_EMC_23, ALT1) [bottom pads]
    {49, &IMXRT_FLEXPWM1, 2, false, DMAMUX_SOURCE_FLEXPWM1_READ2,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_23, 1,
     &IOMUXC_FLEXPWM1_PWMA2_SELECT_INPUT, 0},

    // Pin 53: FlexPWM3_SM0_A (GPIO_EMC_29, ALT1) [bottom pads]
    // No select_input register: FlexPWM3_PWMA0 has only one pad option on IMXRT1062
    {53, &IMXRT_FLEXPWM3, 0, false, DMAMUX_SOURCE_FLEXPWM3_READ0,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_29, 1,
     nullptr, 0},

    // Pin 54: FlexPWM3_SM2_A (GPIO_EMC_33, ALT1) [bottom pads]
    // No select_input register: FlexPWM3_PWMA2 has only one pad option on IMXRT1062
    {54, &IMXRT_FLEXPWM3, 2, false, DMAMUX_SOURCE_FLEXPWM3_READ2,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_33, 1,
     nullptr, 0},
#endif // ARDUINO_TEENSY41
};

static constexpr size_t kPinMapSize = sizeof(kPinMap) / sizeof(kPinMap[0]);

/// Look up pin info. Returns nullptr for unsupported pins.
static const FlexPwmPinInfo *lookupPin(int pin) {
    for (size_t i = 0; i < kPinMapSize; ++i) {
        if (kPinMap[i].pin == static_cast<u8>(pin)) {
            return &kPinMap[i];
        }
    }
    return nullptr;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Streaming capture ring
// ---------------------------------------------------------------------------
//
// The DMA writes (rise, fall) capture pairs into a fixed circular ring and
// raises an interrupt at each half (INTHALF) and at the wrap (INTMAJOR). The
// ISR decodes the half the DMA just left straight into the decoded-byte
// buffer, so a frame of any length costs its decoded bytes (3 B per RGB LED)
// plus this ring, instead of 4 B per bit of stored timestamps.
//
// Sizing (kRingHalfPairs): one WS2812 bit (one pair) arrives every ~1.25 us,
// so a 128-pair half fills in 160 us. The ISR for a half must finish before
// the DMA wraps back into it, one half-time after the interrupt is raised,
// so the budget per half is 160 us including interrupt latency. Measured on
// a Teensy 4.0 at 600 MHz over 1000-LED frames (isrMaxCycles in
// diagnosticsToJson): a half decodes in ~9.8k cycles (16 us, 10 % of the
// budget); the worst ISR seen, preempted by ObjectFLED's refill ISR, took
// 53k cycles (88 us), still 72 us inside the budget. Zero overruns over
// 128-frame soaks with ObjectFLED and FlexIO TX. Ring = 2 x 128 pairs x 4 B
// = 1 KB. A late ISR is detected (the DMA write position is inside the half
// being decoded) and reported as DecodeError::CAPTURE_OVERRUN.
static constexpr u32 kRingHalfPairs = 128;
static constexpr u32 kRingPairs = 2 * kRingHalfPairs;
/// First edges kept for getRawEdgeTimes() diagnostics.
static constexpr u32 kDiagEdges = 256;

// In DTCM (default .bss on Teensy 4): DMA-accessible and never cached, so
// the ISR needs no cache maintenance per half.
static u16 sCaptureRing[kRingPairs * 2] __attribute__((aligned(32)));  // FL_LINT_ALLOW_GLOBAL(DMA ring for the single active FlexPWM RX instance)

// Since-boot totals across captures, for soak reporting.
static volatile u32 sPeakIsrCycles = 0;  // FL_LINT_ALLOW_GLOBAL(FlexPWM RX ISR statistics since boot)
static volatile u32 sTotalOverruns = 0;  // FL_LINT_ALLOW_GLOBAL(FlexPWM RX ISR statistics since boot)
static volatile u32 sTotalFrames = 0;  // FL_LINT_ALLOW_GLOBAL(FlexPWM RX ISR statistics since boot)

static ChipsetTiming4Phase defaultStreamTiming() {
    const ChipsetTiming ws2812b{250, 625, 375, 280, "WS2812B"};
    return make4PhaseTiming(ws2812b, 150);
}

// ---------------------------------------------------------------------------
// FlexPwmRxChannelImpl -- private implementation
// ---------------------------------------------------------------------------

class FlexPwmRxChannelImpl : public FlexPwmRxChannel {
  public:
    explicit FlexPwmRxChannelImpl(int pin) : mPin(pin) {}
    ~FlexPwmRxChannelImpl() override {
        // #3416 RX-LOW-7: tear down on destruction so subsequent
        // peripheral users on the same pin don't inherit our PAD_CTL
        // (HYS/PKE/PUE) or ALT-mode + SION setting. Disable DMA first
        // so a pending IRQ doesn't fire on a freed object.
        if (mConfigured) {
            mDma.disable();
            mDma.detachInterrupt();
            NVIC_SET_PRIORITY(IRQ_DMA_CH0 + (mDma.channel & 15), 128);
        }
        if (sActiveInstance == this) {
            sActiveInstance = nullptr;
        }
        if (mPinInfo && mPinInfo->mux_register) {
            // Restore to ALT5 (GPIO) without SION, default PAD_CTL.
            *(mPinInfo->mux_register) = 5;
            volatile u32 *pad_register = (volatile u32 *)(
                (uintptr_t)mPinInfo->mux_register + 0x1F0u);
            *pad_register = 0;
        }
    }

    bool begin(const RxConfig &config) override;
    bool finished() const override;
    RxWaitResult wait(u32 timeout_ms) override;
    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                        fl::span<u8> out) override;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset = 0) override;
    const char *name() const override { return "FlexPWM"; }
    int getPin() const override { return mPin; }
    bool injectEdges(fl::span<const EdgeTime> edges) override;

  private:
    void configureFlexPwm();
    void configureDma();
    void serviceHalf();
    void drain();
    u32 dmaWritePair() const;

    static void dmaIsr();
    static FlexPwmRxChannelImpl *sActiveInstance;

    friend fl::json FlexPwmRxChannel::diagnosticsToJson(int requested_pin) FL_NO_EXCEPT;
    friend fl::json FlexPwmRxChannel::streamStatsToJson() FL_NO_EXCEPT;

    int mPin = -1;
    const FlexPwmPinInfo *mPinInfo = nullptr;

    DMAChannel mDma;

    // Streaming decode state. The ISR owns mDecoder and mNextHalf while the
    // DMA runs; drain() masks interrupts before the thread touches them.
    channels::rx::Ws2812StreamDecoder mDecoder;
    fl::vector<u8> mDecoded;  // decoded bytes, sized from buffer_size
    fl::vector<EdgeTime> mDiagEdges;
    volatile u32 mNextHalf = 0;    // ring half the ISR decodes next
    volatile u32 mHalvesDone = 0;  // halves decoded by the ISR this frame
    volatile u32 mOverruns = 0;    // halves overwritten before decode
    volatile u32 mIsrCount = 0;
    volatile u32 mIsrMaxCycles = 0;
    volatile u32 mIsrTotalCycles = 0;
    u32 mTailPairs = 0;  // pairs drained from the partial last half

    // State
    volatile bool mReceiveDone = false;
    bool mConfigured = false;
    bool mStartLow = true;

    // injectEdges() test path: decoded by the batch decoder.
    fl::vector<EdgeTime> mInjectedEdges;
    bool mInjected = false;

    // Config
    u32 mSignalRangeMaxNs = 100000;
};

FlexPwmRxChannelImpl *FlexPwmRxChannelImpl::sActiveInstance = nullptr;

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------

bool FlexPwmRxChannelImpl::begin(const RxConfig &config) {
    mPinInfo = lookupPin(mPin);
    if (!mPinInfo) {
        FL_WARN("Pin " << mPin << " does not support FlexPWM capture on Teensy 4.x");
        return false;
    }

    // Capture reprograms the whole submodule (CTRL, INIT, VAL0..5, LDOK),
    // so any PWM output sharing it -- e.g. analogWrite on pin 7, which is
    // FlexPWM1 SM3 B next to pin 8's SM3 A -- would silently change.
    // Refuse instead of clobbering a sibling that has its output enabled.
    {
        const u8 sm = mPinInfo->submodule;
        const u16 sm_out = FLEXPWM_OUTEN_PWMA_EN(1u << sm) |
                           FLEXPWM_OUTEN_PWMB_EN(1u << sm) |
                           FLEXPWM_OUTEN_PWMX_EN(1u << sm);
        if (mPinInfo->pwm->OUTEN & sm_out) {
            FL_WARN("Pin " << mPin << ": FlexPWM submodule " << int(sm)
                    << " has an active PWM output; capture would alter it");
            return false;
        }
    }

    // The capture ring is shared: one capture at a time. A capture still
    // running on another instance is stopped and reported as overrun.
    if (sActiveInstance && sActiveInstance != this &&
        !sActiveInstance->mReceiveDone) {
        sActiveInstance->mDma.disable();
        sActiveInstance->mDecoder.markOverrun();
        sActiveInstance->mDecoder.flush();
        sActiveInstance->mReceiveDone = true;
    }
    // Stop a previous capture before its decoder state is reset under it.
    if (mConfigured) {
        mDma.disable();
        mDma.clearInterrupt();
    }

    // #3416 RX-MED-6: signal_range_max_ns / 1000 is the inactivity window
    // wait() uses to declare frame end.
    mSignalRangeMaxNs = config.signal_range_max_ns;
    mStartLow = config.start_low;
    mReceiveDone = false;
    mInjected = false;
    mInjectedEdges.clear();

    // Only decoded bytes are kept, so memory is O(frame bytes), not
    // O(edges). The old buffer held buffer_size capture pairs (one per bit),
    // so keep that frame capacity: buffer_size bits = buffer_size / 8 bytes.
    // +1 holds a partial final byte.
    const size_t decoded_capacity = config.buffer_size / 8u + 1u;
    mDecoded.clear();
    mDecoded.resize(decoded_capacity);
    mDiagEdges.resize(kDiagEdges);

    mNextHalf = 0;
    mHalvesDone = 0;
    mOverruns = 0;
    mIsrCount = 0;
    mIsrMaxCycles = 0;
    mIsrTotalCycles = 0;
    mTailPairs = 0;
    const ChipsetTiming4Phase stream_timing =
        config.stream_timing ? *config.stream_timing : defaultStreamTiming();
    mDecoder.reset(stream_timing, channels::rx::flexPwmNsPerTickQ16(F_BUS_ACTUAL),
                   fl::span<u8>(mDecoded), fl::span<EdgeTime>(mDiagEdges));

    configureFlexPwm();
    configureDma();
    mConfigured = true;
    return true;
}

// ---------------------------------------------------------------------------
// FlexPWM configuration
// ---------------------------------------------------------------------------

void FlexPwmRxChannelImpl::configureFlexPwm() {
    IMXRT_FLEXPWM_t *pwm = mPinInfo->pwm;
    u8 sm = mPinInfo->submodule;

    // Enable clock gates for all FlexPWM modules.
    // Teensy core may not enable all modules at startup.
    CCM_CCGR4 |= CCM_CCGR4_PWM1(CCM_CCGR_ON) |
                  CCM_CCGR4_PWM2(CCM_CCGR_ON) |
                  CCM_CCGR4_PWM3(CCM_CCGR_ON) |
                  CCM_CCGR4_PWM4(CCM_CCGR_ON);

    // Configure pin mux to route the pin to FlexPWM input.
    // Set SION (bit 4) to force input path through IOMUXC — required
    // for peripheral input capture when the pad is muxed to an alt function.
    // #3416 RX-MED-2: this is `=` not `|=` -- the assignment wipes any
    // other IOMUXC bits the boot ROM or Teensy core set (e.g. ODE).
    // For our pure-input use that is intentional: we want a known
    // alt-mode + SION starting point, and the SW_PAD_CTL write below
    // sets the only pad attributes we care about (HYS, PKE, PUE).
    *(mPinInfo->mux_register) = mPinInfo->mux_value | 0x10; // SION bit
    if (mPinInfo->select_register) {
        *(mPinInfo->select_register) = mPinInfo->select_value;
    }

    // #3416 RX-HIGH-3 / RX-LOW-8: configure SW_PAD_CTL with hysteresis +
    // keeper so a marginal edge on a long jumper trace doesn't ring
    // across the receiver's Vih/Vil thresholds and produce a spurious
    // "double-H" capture (one of the residual ~0.8% noise-floor
    // symptoms in #3410). The PAD_CTL register lives at a fixed
    // +0x1F0 offset from MUX_CTL for every IOMUXC pad on the i.MX RT1062
    // (verified across GPIO_EMC_*, GPIO_AD_B1_*, GPIO_B0_*, GPIO_B1_*
    // via Teensyduino imxrt.h offsets). Final values (after empirically
    // testing keeper vs pull-up vs pull-down -- all three statistically
    // identical because TX is push-pull):
    //   bit 12 PKE = 1: pull/keep enable
    //   bit 13 PUE = 0: keeper mode (holds last driven level)
    //   bit 16 HYS = 1: hysteresis enable -- the actual noise-floor lever
    volatile u32 *pad_register = (volatile u32 *)(
        (uintptr_t)mPinInfo->mux_register + 0x1F0u);
    // KEEPER mode (PUE=0): pad holds the last driven level. WS2812 lines
    // are actively driven by the TX side both HIGH and LOW; tested
    // pull-up, pull-down, and keeper -- all three produced statistically
    // identical noise floors on bench loopback (0.5-1.0% byte error
    // range across both FlexIO and ObjectFLED TX). Keeper is the least
    // surprising default since push-pull TX leaves no time window for
    // any of the three configurations to actually act differently on
    // the line. Hysteresis (HYS=1) is the one bit that's theoretically
    // useful, kept regardless.
    *pad_register = (1u << 12) |    // PKE
                    (0u << 13) |    // PUE = 0 -> keeper mode
                    (1u << 16);     // HYS

    // Disable the submodule while configuring
    pwm->MCTRL &= ~(FLEXPWM_MCTRL_RUN(1 << sm));

    // CTRL2: Clock source = IPBus clock (F_BUS_ACTUAL), no init from ext
    // CLK_SEL = 0 (IPBus clock), INIT_SEL = 0 (local sync)
    pwm->SM[sm].CTRL2 = 0;

    // CTRL: Full cycle reload, prescaler = 0 (divide by 1). LDMOD makes the
    // buffered INIT/VALx registers load as soon as LDOK is set below instead
    // of waiting for the next reload of the old period.
    pwm->SM[sm].CTRL = FLEXPWM_SMCTRL_FULL | FLEXPWM_SMCTRL_LDMOD;

    // Free-running counter: INIT = 0, VAL1 = 0xFFFF (max period).
    //
    // INIT and VAL0..VAL5 are double-buffered: a write only reaches the
    // counter once MCTRL.LDOK is set for the submodule. Without the LDOK
    // below the counter kept the Teensyduino analogWrite default period
    // (VAL1 = 33464 ticks, 4.482 kHz) while flexPwmTickDeltaNs() assumes a 65536
    // tick wrap. Every ~223 us one HIGH or LOW straddling the wrap then
    // measured ~214 us too long: a LOW was dropped as a gap, and a HIGH
    // turned a 0 bit into a 1 -- the residual random 0->1 flips (#3406).
    // Write CLDOK/LDOK without echoing back pending LDOK bits: a plain
    // read-modify-write would set and clear this submodule's LDOK at once.
    pwm->MCTRL = (pwm->MCTRL & ~FLEXPWM_MCTRL_LDOK(0x0F)) |
                 FLEXPWM_MCTRL_CLDOK(1 << sm);
    pwm->SM[sm].INIT = 0;
    pwm->SM[sm].VAL1 = 0xFFFF;
    pwm->SM[sm].VAL0 = 0;
    pwm->SM[sm].VAL2 = 0;
    pwm->SM[sm].VAL3 = 0;
    pwm->SM[sm].VAL4 = 0;
    pwm->SM[sm].VAL5 = 0;
    pwm->MCTRL = (pwm->MCTRL & ~FLEXPWM_MCTRL_LDOK(0x0F)) |
                 FLEXPWM_MCTRL_LDOK(1 << sm);

    if (!mPinInfo->channel_b) {
        // Channel A capture configuration (CAPTCTRLA / CAPTCOMPA)
        // EDGA0 = 2 captures rising edges to CVAL2.
        // EDGA1 = 1 captures falling edges to CVAL3.
        // DMA fires on CA1DE after both registers for the bit are valid.
        pwm->SM[sm].CAPTCTRLA = FLEXPWM_SMCAPTCTRLA_EDGA0(2) |
                                 FLEXPWM_SMCAPTCTRLA_EDGA1(1) |
                                 FLEXPWM_SMCAPTCTRLA_CFAWM(0) |
                                 FLEXPWM_SMCAPTCTRLA_ARMA;
        // CAPTCOMPA bits 0:1 (CFA, CAE) are write-1-to-clear capture-occurred
        // flags per i.MX RT1062 RM eFlexPWM chapter. Writing 0 does NOT clear
        // them, so a stale flag from a previous capture session causes the
        // dual-circuit capture to produce a mismatched CVAL2/CVAL3 pair on
        // the first read (old falling + fresh rising) -- root cause of the
        // residual 0->1 random bit flips per #3406.
        pwm->SM[sm].CAPTCOMPA = 0x3;

        // CAPTDE selects channel A capture DMA for the submodule read source.
        pwm->SM[sm].DMAEN = FLEXPWM_SMDMAEN_CAPTDE(1) | FLEXPWM_SMDMAEN_CA1DE;
    } else {
        // Channel B capture configuration (CAPTCTRLB / CAPTCOMPB)
        // EDGB0 = 2 captures rising edges to CVAL4.
        // EDGB1 = 1 captures falling edges to CVAL5.
        pwm->SM[sm].CAPTCTRLB = FLEXPWM_SMCAPTCTRLB_EDGB0(2) |
                                 FLEXPWM_SMCAPTCTRLB_EDGB1(1) |
                                 FLEXPWM_SMCAPTCTRLB_ARMB;
        // CAPTCOMPB bits 0:1 (CFB, CBE) are write-1-to-clear -- same as
        // CAPTCOMPA above.
        pwm->SM[sm].CAPTCOMPB = 0x3;

        // CAPTDE selects channel B capture DMA for the submodule read source.
        pwm->SM[sm].DMAEN = FLEXPWM_SMDMAEN_CAPTDE(2) | FLEXPWM_SMDMAEN_CB1DE;
    }

    // Start the submodule counter
    pwm->MCTRL |= FLEXPWM_MCTRL_RUN(1 << sm);
}

// ---------------------------------------------------------------------------
// DMA configuration
// ---------------------------------------------------------------------------

void FlexPwmRxChannelImpl::configureDma() {
    sActiveInstance = this;

    // Source: first FlexPWM capture value register in the rising/falling pair.
    // Channel A: CVAL2 then CVAL3. Channel B: CVAL4 then CVAL5.
    // The source offset advances to the falling-edge register for the second
    // 16-bit read; the minor-loop offset rewinds to the rising-edge register
    // for the next hardware request.
    volatile u16 *capture_reg;
    if (!mPinInfo->channel_b) {
        capture_reg = &(mPinInfo->pwm->SM[mPinInfo->submodule].CVAL2);
    } else {
        capture_reg = &(mPinInfo->pwm->SM[mPinInfo->submodule].CVAL4);
    }

    // Circular ring: one 4-byte minor loop (rise, fall) per hardware request,
    // kRingPairs minor loops per major loop. DLASTSGA rewinds DADDR to the
    // ring start at each major-loop end, and with DREQ clear the channel
    // keeps running, so the ring is reused until wait() stops it. Interrupts
    // at the half (INTHALF) and the end (INTMAJOR) of each pass.
    mDma.begin();
    mDma.TCD->SADDR = const_cast<u16 *>(capture_reg);
    mDma.TCD->SOFF = 4;
    mDma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    mDma.TCD->NBYTES_MLNO =
        DMA_TCD_NBYTES_MLOFFYES_NBYTES(4) |
        DMA_TCD_NBYTES_MLOFFYES_MLOFF(-8) |
        DMA_TCD_NBYTES_SMLOE;
    // The minor-loop offset is not applied after the last minor loop of a
    // major loop; SLAST is applied instead. Rewind the two SOFF steps here,
    // or every pass after the first reads CVAL4/CVAL5 (bench: the second
    // ring pass decoded as all zeros and the channel then stopped).
    mDma.TCD->SLAST = -8;
    mDma.TCD->DADDR = sCaptureRing;
    mDma.TCD->DOFF = 2;
    mDma.TCD->CITER = kRingPairs;
    mDma.TCD->DLASTSGA = -static_cast<i32>(sizeof(sCaptureRing));
    mDma.TCD->BITER = kRingPairs;
    mDma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;

    mDma.triggerAtHardwareEvent(mPinInfo->dma_source);
    mDma.attachInterrupt(dmaIsr);
    // Below the default priority (128) so a TX driver's refill ISR
    // (ObjectFLED ESG chunks, FlexIO ring) preempts this decode: the decode
    // has a whole ring half of slack, a TX refill has less.
    NVIC_SET_PRIORITY(IRQ_DMA_CH0 + (mDma.channel & 15), 192);

    // Settle delay: the IOMUXC pad-mux switch in configureFlexPwm() can latch
    // a spurious edge into CVAL2/CVAL3 BEFORE the real TX starts. Re-arm the
    // capture circuit just before enabling DMA so the spike is consumed by
    // the capture FIFO without DMA picking it up as the first capture pair.
    // Without this, ~1 in 7 frames sees a whole-frame 1-bit shift signature
    // ((0xF0,0x0F,0xAA) decoded as (0xE0,0x1F,0x55)) -- #3406 Round-4 Run 7.
    delayMicroseconds(50);
    //
    // The ARMA toggle does NOT empty the capture FIFOs. A rising edge
    // captured before arming (a previous frame or the pre-test toggles)
    // stays in the edge-0 FIFO (bench: CAPTCTRLA.CA0CNT == 1 at arm). The
    // frame's first real rise is then dropped, and the first DMA read pairs
    // the stale rise with bit 0's fall: a HIGH of tens to hundreds of us
    // that decodes bit 0 as 1 (0x55 -> 0xD5). Pop both FIFOs by reading
    // their CVAL registers until the CNT fields read zero.
    auto &smr = mPinInfo->pwm->SM[mPinInfo->submodule];
    const u16 kFifoCountMaskA =
        FLEXPWM_SMCAPTCTRLA_CA0CNT(7) | FLEXPWM_SMCAPTCTRLA_CA1CNT(7);
    const u16 kFifoCountMaskB =
        FLEXPWM_SMCAPTCTRLB_CB0CNT(7) | FLEXPWM_SMCAPTCTRLB_CB1CNT(7);
    if (!mPinInfo->channel_b) {
        smr.CAPTCTRLA &= ~static_cast<u16>(FLEXPWM_SMCAPTCTRLA_ARMA);
        smr.CAPTCTRLA |= FLEXPWM_SMCAPTCTRLA_ARMA;
        for (int n = 0; n < 16 && (smr.CAPTCTRLA & kFifoCountMaskA); ++n) {
            (void)smr.CVAL2;
            (void)smr.CVAL3;
        }
    } else {
        smr.CAPTCTRLB &= ~static_cast<u16>(FLEXPWM_SMCAPTCTRLB_ARMB);
        smr.CAPTCTRLB |= FLEXPWM_SMCAPTCTRLB_ARMB;
        for (int n = 0; n < 16 && (smr.CAPTCTRLB & kFifoCountMaskB); ++n) {
            (void)smr.CVAL4;
            (void)smr.CVAL5;
        }
    }

    mDma.enable();
}

// ---------------------------------------------------------------------------
// DMA ISR: decode one ring half
// ---------------------------------------------------------------------------

/// Index of the pair the DMA writes next, 0..kRingPairs-1. CITER counts the
/// minor loops left in the current pass and reloads from BITER at the wrap.
u32 FlexPwmRxChannelImpl::dmaWritePair() const {
    const u32 citer = mDma.TCD->CITER;
    return (kRingPairs - citer) % kRingPairs;
}

void FlexPwmRxChannelImpl::serviceHalf() {
    const u32 half = mNextHalf;
    // The DMA must be writing the other half. If it is already inside this
    // one, it finished the other half and wrapped: captures were lost.
    if ((dmaWritePair() / kRingHalfPairs) == half) {
        ++mOverruns;
        mDecoder.markOverrun();
    }
    mDecoder.push(fl::span<const u16>(sCaptureRing)
                      .subspan(half * kRingHalfPairs * 2, kRingHalfPairs * 2));
    // Re-check: did the DMA overwrite this half while we decoded it?
    if ((dmaWritePair() / kRingHalfPairs) == half) {
        ++mOverruns;
        mDecoder.markOverrun();
    }
    mNextHalf = half ^ 1u;
    mHalvesDone = mHalvesDone + 1;
}

void FlexPwmRxChannelImpl::dmaIsr() {
    FlexPwmRxChannelImpl *self = sActiveInstance;
    if (!self) {
        return;
    }
    const u32 t0 = ARM_DWT_CYCCNT;
    self->mDma.clearInterrupt();
    if (self->mReceiveDone) {
        // drain() already decoded this half (an IRQ latched in the NVIC
        // while it ran with interrupts masked). Nothing left to do.
        asm volatile("dsb" ::: "memory");
        return;
    }
    self->serviceHalf();
    const u32 cycles = ARM_DWT_CYCCNT - t0;
    self->mIsrCount = self->mIsrCount + 1;
    self->mIsrTotalCycles = self->mIsrTotalCycles + cycles;
    if (cycles > self->mIsrMaxCycles) {
        self->mIsrMaxCycles = cycles;
    }
    if (cycles > sPeakIsrCycles) {
        sPeakIsrCycles = cycles;
    }
    // Make the DMA_CINT write land before return so the NVIC does not
    // re-enter for the same request (Teensyduino FlexSerial pattern).
    asm volatile("dsb" ::: "memory");
}

/// Stop the capture and decode what the ISR has not: a pending half whose
/// interrupt has not run yet, then the partial half up to the DMA position.
void FlexPwmRxChannelImpl::drain() {
    if (mReceiveDone) {
        return;
    }
    mDma.disable();
    noInterrupts();
    if (DMA_INT & (1u << mDma.channel)) {
        mDma.clearInterrupt();
        serviceHalf();
    }
    const u32 pos = dmaWritePair();
    const u32 half = mNextHalf;
    const u32 half_start = half * kRingHalfPairs;
    if (pos >= half_start && pos < half_start + kRingHalfPairs) {
        mTailPairs = pos - half_start;
        mDecoder.push(fl::span<const u16>(sCaptureRing)
                          .subspan(half_start * 2, mTailPairs * 2));
    } else {
        // The DMA is in the other half with this one still undecoded: a
        // completion interrupt was lost.
        ++mOverruns;
        mDecoder.markOverrun();
    }
    NVIC_CLEAR_PENDING(IRQ_DMA_CH0 + (mDma.channel & 15));
    mDecoder.flush();
    sTotalOverruns = sTotalOverruns + mOverruns;
    sTotalFrames = sTotalFrames + 1;
    mReceiveDone = true;
    interrupts();
}

// ---------------------------------------------------------------------------
// finished() / wait()
// ---------------------------------------------------------------------------

bool FlexPwmRxChannelImpl::finished() const {
    return mReceiveDone;
}

RxWaitResult FlexPwmRxChannelImpl::wait(u32 timeout_ms) {
    if (mInjected) {
        return RxWaitResult::SUCCESS;
    }
    if (mReceiveDone) {
        return mDecoder.stats().pairs > 0 ? RxWaitResult::SUCCESS
                                          : RxWaitResult::TIMEOUT;
    }
    const u32 start = millis();
    // Progress = halves decoded by the ISR plus the DMA position. capture()
    // calls FastLED.wait() before this, so the frame may already be over.
    auto progress = [this]() -> u32 {
        return mHalvesDone * kRingHalfPairs + dmaWritePair();
    };
    u32 last_progress = progress();
    u32 last_change_time = micros();

    while (true) {
        if ((millis() - start) >= timeout_ms) {
            break;
        }
        const u32 current = progress();
        const u32 now_us = micros();
        if (current != last_progress) {
            last_progress = current;
            last_change_time = now_us;
        } else if ((now_us - last_change_time) >= (mSignalRangeMaxNs / 1000)) {
            break;  // idle long enough: frame complete
        }
        yield();
    }

    drain();
    if (mDecoder.stats().pairs == 0) {
        return RxWaitResult::TIMEOUT;
    }
    return RxWaitResult::SUCCESS;
}

// ---------------------------------------------------------------------------
// decode()
// ---------------------------------------------------------------------------

fl::result<u32, DecodeError>
FlexPwmRxChannelImpl::decode(const ChipsetTiming4Phase &timing,
                              fl::span<u8> out) {
    if (mInjected) {
        return channels::rx::decodeFlexPwmEdges(
            timing,
            fl::span<const EdgeTime>(mInjectedEdges),
            out);
    }
    drain();
    mDecoder.finish(timing);
    return mDecoder.copyTo(out);
}

// ---------------------------------------------------------------------------
// getRawEdgeTimes()
// ---------------------------------------------------------------------------

size_t FlexPwmRxChannelImpl::getRawEdgeTimes(fl::span<EdgeTime> out,
                                              size_t offset) {
    // Streaming keeps only the first kDiagEdges edges of the frame.
    const EdgeTime *src = mDiagEdges.data();
    size_t count = mDecoder.diagEdgeCount();
    if (mInjected) {
        src = mInjectedEdges.data();
        count = mInjectedEdges.size();
    } else {
        drain();
    }
    if (offset >= count) {
        return 0;
    }
    const size_t available = count - offset;
    const size_t to_copy = (available < out.size()) ? available : out.size();
    for (size_t i = 0; i < to_copy; ++i) {
        out[i] = src[offset + i];
    }
    return to_copy;
}

// ---------------------------------------------------------------------------
// injectEdges()
// ---------------------------------------------------------------------------

// #3416 RX-LOW-6: this is a TEST-ONLY entry point. It bypasses the
// DMA capture path and pre-loads the batch decoder with synthetic edges.
// The fixture must call begin() again before a real capture.
bool FlexPwmRxChannelImpl::injectEdges(fl::span<const EdgeTime> edges) {
    if (mConfigured) {
        mDma.disable();
    }
    mInjectedEdges.clear();
    mInjectedEdges.reserve(edges.size());
    for (size_t i = 0; i < edges.size(); ++i) {
        mInjectedEdges.push_back(edges[i]);
    }
    mInjected = true;
    mReceiveDone = true;
    return true;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

fl::shared_ptr<FlexPwmRxChannel> FlexPwmRxChannel::create(int pin) {
    const FlexPwmPinInfo *info = lookupPin(pin);
    if (!info) {
        FL_WARN("Pin " << pin << " does not support FlexPWM capture on Teensy 4.x");
        return fl::shared_ptr<FlexPwmRxChannel>();
    }
    return fl::make_shared<FlexPwmRxChannelImpl>(pin);
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

static u32 flexPwmDiagPtrToU32(const volatile void *ptr) FL_NO_EXCEPT {
    return static_cast<u32>(fl::ptr_to_int(const_cast<void *>(ptr)));
}

static void flexPwmDiagSetU32(fl::json &obj, const char *key, u32 value) FL_NO_EXCEPT {
    obj.set(key, static_cast<i64>(value));
}

static void flexPwmDiagSetPtr(fl::json &obj, const char *key,
                              const volatile void *ptr) FL_NO_EXCEPT {
    flexPwmDiagSetU32(obj, key, flexPwmDiagPtrToU32(ptr));
}

fl::json FlexPwmRxChannel::streamStatsToJson() FL_NO_EXCEPT {
    fl::json out = fl::json::object();
    FlexPwmRxChannelImpl *active = FlexPwmRxChannelImpl::sActiveInstance;
    if (active) {
        out.set("pairs", static_cast<i64>(active->mDecoder.stats().pairs));
        out.set("isrCount", static_cast<i64>(active->mIsrCount));
        out.set("isrMaxCycles", static_cast<i64>(active->mIsrMaxCycles));
        out.set("overruns", static_cast<i64>(active->mOverruns));
    }
    out.set("ringBytes", static_cast<i64>(sizeof(sCaptureRing)));
    out.set("isrPeakCyclesSinceBoot", static_cast<i64>(sPeakIsrCycles));
    out.set("overrunsSinceBoot", static_cast<i64>(sTotalOverruns));
    out.set("framesSinceBoot", static_cast<i64>(sTotalFrames));
    return out;
}

fl::json FlexPwmRxChannel::diagnosticsToJson(int requested_pin) FL_NO_EXCEPT {
    fl::json out = fl::json::object();
    out.set("format", "flexpwm-rx-diag-v1");
    out.set("requestedPin", static_cast<i64>(requested_pin));

    const FlexPwmPinInfo *info = lookupPin(requested_pin);
    out.set("requestedPinSupported", info != nullptr);
    if (info) {
        out.set("requestedDmaSource", static_cast<i64>(info->dma_source));
        out.set("requestedSubmodule", static_cast<i64>(info->submodule));
        out.set("requestedChannelB", info->channel_b);
        flexPwmDiagSetPtr(out, "requestedMuxRegister", info->mux_register);
        flexPwmDiagSetU32(out, "requestedMuxValue", info->mux_value);
        if (info->select_register) {
            out.set("hasSelectRegister", true);
            flexPwmDiagSetPtr(out, "requestedSelectRegister", info->select_register);
            flexPwmDiagSetU32(out, "requestedSelectValue", info->select_value);
        } else {
            out.set("hasSelectRegister", false);
        }
    }

    FlexPwmRxChannelImpl *active = FlexPwmRxChannelImpl::sActiveInstance;
    out.set("activeInstance", active != nullptr);
    if (!active || !active->mPinInfo) {
        return out;
    }

    out.set("activePin", static_cast<i64>(active->mPin));
    out.set("activePinMatches", active->mPin == requested_pin);
    out.set("configured", active->mConfigured);
    out.set("receiveDone", static_cast<bool>(active->mReceiveDone));
    // Streaming capture: ring geometry, ISR cost and overruns.
    const channels::rx::Ws2812StreamDecoder::Stats &st = active->mDecoder.stats();
    out.set("ringPairs", static_cast<i64>(kRingPairs));
    out.set("ringBytes", static_cast<i64>(sizeof(sCaptureRing)));
    out.set("decodedCapacity", static_cast<i64>(active->mDecoded.size()));
    out.set("pairs", static_cast<i64>(st.pairs));
    out.set("bits", static_cast<i64>(st.bits));
    out.set("bitErrors", static_cast<i64>(st.errors));
    out.set("fullBytes", static_cast<i64>(st.fullBytes));
    out.set("halvesDone", static_cast<i64>(active->mHalvesDone));
    out.set("tailPairs", static_cast<i64>(active->mTailPairs));
    out.set("overruns", static_cast<i64>(active->mOverruns));
    out.set("isrCount", static_cast<i64>(active->mIsrCount));
    out.set("isrMaxCycles", static_cast<i64>(active->mIsrMaxCycles));
    out.set("isrTotalCycles", static_cast<i64>(active->mIsrTotalCycles));
    out.set("cpuHz", static_cast<i64>(F_CPU_ACTUAL));
    out.set("isrPeakCyclesSinceBoot", static_cast<i64>(sPeakIsrCycles));
    out.set("overrunsSinceBoot", static_cast<i64>(sTotalOverruns));
    out.set("framesSinceBoot", static_cast<i64>(sTotalFrames));
    out.set("calibratedMidpointNs", static_cast<i64>(st.calibratedMidpointNs));
    out.set("frameMidpointNs", static_cast<i64>(st.frameMidpointNs));
    out.set("nearThresholdBits", static_cast<i64>(st.exceptions));
    out.set("inexact", st.inexact);
    out.set("signalRangeMaxNs", static_cast<i64>(active->mSignalRangeMaxNs));

    // Pin pad / GPIO state for the RX pin itself. Lets us tell if the pad
    // inherited some pull/keeper/ODE/HYS setting from boot that would block
    // FlexPWM input capture even when GPIO-mode digitalRead works.
#if defined(NUM_DIGITAL_PINS)
    if (active->mPin >= 0 && active->mPin < NUM_DIGITAL_PINS) {
        volatile u32 *padReg = portControlRegister(active->mPin);
        volatile u32 *fastOut = portOutputRegister(active->mPin);
        volatile u32 *fastMode = portModeRegister(active->mPin);
        volatile u32 *fastInput =
            reinterpret_cast<volatile u32 *>( // ok reinterpret cast - Teensy fast GPIO PSR offset
                fl::ptr_to_int(const_cast<u32 *>(fastOut)) + 0x08u);
        volatile u32 *standardOut = reinterpret_cast<volatile u32 *>( // ok reinterpret cast - Teensy GPIO alias address map
            fl::ptr_to_int(const_cast<u32 *>(fastOut)) - 0x01E48000u);
        volatile u32 *standardMode = reinterpret_cast<volatile u32 *>( // ok reinterpret cast - Teensy GPIO alias address map
            fl::ptr_to_int(const_cast<u32 *>(fastMode)) - 0x01E48000u);
        volatile u32 *standardInput =
            reinterpret_cast<volatile u32 *>( // ok reinterpret cast - Teensy fast GPIO PSR offset
                fl::ptr_to_int(const_cast<u32 *>(standardOut)) + 0x08u);
        flexPwmDiagSetPtr(out, "rxPadRegister", padReg);
        flexPwmDiagSetU32(out, "rxPadValue", *padReg);
        const u8 bit = digitalPinToBit(active->mPin);
        out.set("rxBit", static_cast<i64>(bit));
        out.set("rxMask", static_cast<i64>(1u << bit));
        flexPwmDiagSetU32(out, "rxFastModeValue", *fastMode);
        flexPwmDiagSetU32(out, "rxFastInputValue", *fastInput);
        flexPwmDiagSetU32(out, "rxStandardModeValue", *standardMode);
        flexPwmDiagSetU32(out, "rxStandardInputValue", *standardInput);
        const u32 gpio6_base =
            static_cast<u32>(fl::ptr_to_int(&GPIO6_DR));
        const u32 output_addr =
            static_cast<u32>(fl::ptr_to_int(const_cast<u32 *>(fastOut)));
        if (output_addr >= gpio6_base) {
            const u8 offset = static_cast<u8>((output_addr - gpio6_base) >> 14);
            out.set("rxFastGpioBank", static_cast<i64>(6 + offset));
            out.set("rxStandardGpioBank", static_cast<i64>(1 + offset));
            if (offset <= 3) {
                volatile u32 *gprReg = &IOMUXC_GPR_GPR26 + offset;
                flexPwmDiagSetU32(out, "rxGprValue", *gprReg);
                out.set("rxMappedToStandard",
                        ((*gprReg) & (1u << bit)) == 0);
            }
        }
    }
#endif

    const FlexPwmPinInfo *active_info = active->mPinInfo;
    out.set("activeSubmodule", static_cast<i64>(active_info->submodule));
    out.set("activeChannelB", active_info->channel_b);
    out.set("activeDmaSource", static_cast<i64>(active_info->dma_source));
    flexPwmDiagSetU32(out, "activeMuxValueLive", *(active_info->mux_register));
    flexPwmDiagSetU32(out, "activeMuxValueExpected", active_info->mux_value | 0x10);
    if (active_info->select_register) {
        flexPwmDiagSetU32(out, "activeSelectValueLive", *(active_info->select_register));
        flexPwmDiagSetU32(out, "activeSelectValueExpected", active_info->select_value);
    }

    const u8 sm = active_info->submodule;
    IMXRT_FLEXPWM_t *pwm = active_info->pwm;
    flexPwmDiagSetPtr(out, "pwm", pwm);
    flexPwmDiagSetU32(out, "mctrl", pwm->MCTRL);
    flexPwmDiagSetU32(out, "ctrl", pwm->SM[sm].CTRL);
    flexPwmDiagSetU32(out, "ctrl2", pwm->SM[sm].CTRL2);
    flexPwmDiagSetU32(out, "dmaen", pwm->SM[sm].DMAEN);
    flexPwmDiagSetU32(out, "captctrla", pwm->SM[sm].CAPTCTRLA);
    flexPwmDiagSetU32(out, "captctrlb", pwm->SM[sm].CAPTCTRLB);
    flexPwmDiagSetU32(out, "captcompa", pwm->SM[sm].CAPTCOMPA);
    flexPwmDiagSetU32(out, "captcompb", pwm->SM[sm].CAPTCOMPB);
    flexPwmDiagSetU32(out, "cval2", pwm->SM[sm].CVAL2);
    flexPwmDiagSetU32(out, "cval3", pwm->SM[sm].CVAL3);
    flexPwmDiagSetU32(out, "cval4", pwm->SM[sm].CVAL4);
    flexPwmDiagSetU32(out, "cval5", pwm->SM[sm].CVAL5);

    out.set("dmaChannel", static_cast<i64>(active->mDma.channel));
    flexPwmDiagSetU32(out, "dmaErq", DMA_ERQ);
    flexPwmDiagSetU32(out, "dmaHrs", DMA_HRS);
    flexPwmDiagSetU32(out, "dmaInt", DMA_INT);
    flexPwmDiagSetU32(out, "dmaErr", DMA_ERR);
    flexPwmDiagSetU32(out, "dmaEs", DMA_ES);
    flexPwmDiagSetU32(out, "dmamuxChcfg", *(&DMAMUX_CHCFG0 + active->mDma.channel));

    if (active->mDma.TCD) {
        out.set("hasTcd", true);
        flexPwmDiagSetPtr(out, "tcdAddress", active->mDma.TCD);
        flexPwmDiagSetPtr(out, "saddr", active->mDma.TCD->SADDR);
        flexPwmDiagSetU32(out, "soff", static_cast<u32>(active->mDma.TCD->SOFF));
        flexPwmDiagSetU32(out, "attr", active->mDma.TCD->ATTR);
        flexPwmDiagSetU32(out, "nbytes", active->mDma.TCD->NBYTES);
        flexPwmDiagSetU32(out, "slast", static_cast<u32>(active->mDma.TCD->SLAST));
        flexPwmDiagSetPtr(out, "daddr", active->mDma.TCD->DADDR);
        flexPwmDiagSetU32(out, "doff", static_cast<u32>(active->mDma.TCD->DOFF));
        flexPwmDiagSetU32(out, "citer", active->mDma.TCD->CITER);
        flexPwmDiagSetU32(out, "dlastsga", static_cast<u32>(active->mDma.TCD->DLASTSGA));
        flexPwmDiagSetU32(out, "csr", active->mDma.TCD->CSR);
        flexPwmDiagSetU32(out, "biter", active->mDma.TCD->BITER);
    } else {
        out.set("hasTcd", false);
    }

    return out;
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
