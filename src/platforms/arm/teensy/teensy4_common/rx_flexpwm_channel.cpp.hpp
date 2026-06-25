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
///      capture values into a RAM buffer as one 32-bit minor loop.
///   4. After the DMA transfer completes (buffer full or auto-disable), an ISR
///      sets a completion flag.
///   5. Software computes pulse widths as 16-bit deltas between consecutive
///      captures. 16-bit wraparound is safe because the longest expected pulse
///      (~280 us reset) is well within the ~437 us wrap period at 150 MHz.
///   6. Tick deltas are converted to nanoseconds:
///        ns = (u64)delta_ticks * 1000000000ULL / F_BUS_ACTUAL

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

// FastLED #3219: the per-frame `[FlexPWM CFG]`/`DMA`/`RAW`/`EDGE`/`E`/
// `DECODE` FL_WARN_F dumps that PR #3216 enabled by default in this
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
    {8, &IMXRT_FLEXPWM1, 3, false, DMAMUX_SOURCE_FLEXPWM1_READ3,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00, 6,
     &IOMUXC_FLEXPWM1_PWMA3_SELECT_INPUT, 0},

    // Pin 22: FlexPWM4_SM0_A (GPIO_AD_B1_08, ALT1)
    {22, &IMXRT_FLEXPWM4, 0, false, DMAMUX_SOURCE_FLEXPWM4_READ0,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_08, 1,
     &IOMUXC_FLEXPWM4_PWMA0_SELECT_INPUT, 0},

    // Pin 23: FlexPWM4_SM1_A (GPIO_AD_B1_09, ALT1)
    {23, &IMXRT_FLEXPWM4, 1, false, DMAMUX_SOURCE_FLEXPWM4_READ1,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09, 1,
     &IOMUXC_FLEXPWM4_PWMA1_SELECT_INPUT, 0},

    // Pin 29: FlexPWM3_SM1_B (GPIO_EMC_31, ALT1)
    {29, &IMXRT_FLEXPWM3, 1, true, DMAMUX_SOURCE_FLEXPWM3_READ1,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_31, 1,
     nullptr, 0},

#if defined(ARDUINO_TEENSY41)
    // Teensy 4.1 only pins

    // Pin 36: FlexPWM2_SM3_A (GPIO_B1_02, ALT6)
    {36, &IMXRT_FLEXPWM2, 3, false, DMAMUX_SOURCE_FLEXPWM2_READ3,
     &IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_02, 6,
     &IOMUXC_FLEXPWM2_PWMA3_SELECT_INPUT, 1},

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

// ---------------------------------------------------------------------------
// Decode helpers (same algorithm as ESP32 RMT RX decoder)
// ---------------------------------------------------------------------------

/// Convert 16-bit tick delta to nanoseconds using the bus clock frequency.
static inline u32 tickDeltaNs(u16 t0, u16 t1) {
    u16 delta = static_cast<u16>(t1 - t0); // handles wraparound
    return static_cast<u32>(
        (static_cast<u64>(delta) * 1000000000ULL) / F_BUS_ACTUAL);
}

/// Decode a single bit from high/low nanosecond durations.
///
/// **Always returns 0 or 1** (never -1). Bench evidence (#3219, 5-LED test):
/// when a single bit's HIGH duration landed marginally outside T0H_max but
/// also outside T1H_min, the old "return -1" path made the decoder `continue`
/// past that bit -- which dropped one bit from the stream and SHIFTED every
/// downstream bit forward by one position in its byte. A single edge-of-
/// tolerance pulse then propagated through the rest of the frame as
/// cascading byte/LED errors (e.g. Pattern B 5-LED: one bit error caused
/// all 5 LEDs to fail with `R: 0x55 -> 0xAB` left-shift). Classifying by
/// the midpoint between t0h_max and t1h_min keeps byte alignment intact;
/// at worst a single LSB flips in the affected bit's byte instead of
/// poisoning everything that follows.
static inline int decodeBit(u32 high_ns, u32 low_ns,
                            const ChipsetTiming4Phase &timing) {
    (void)low_ns;  // HIGH-only classification is more robust to TX/RX skew
    const u32 midpoint =
        (timing.t0h_max_ns + timing.t1h_min_ns) / 2u;
    return (high_ns >= midpoint) ? 1 : 0;
}

/// Decode a bit when the following LOW phase is a reset/gap or was not
/// captured. WS2812 bit value is encoded by HIGH width; LOW validation is only
/// possible for intra-frame bit periods. Always returns 0 or 1 -- see the
/// rationale on `decodeBit()`.
static inline int decodeBitFromHigh(u32 high_ns,
                                    const ChipsetTiming4Phase &timing) {
    const u32 midpoint =
        (timing.t0h_max_ns + timing.t1h_min_ns) / 2u;
    return (high_ns >= midpoint) ? 1 : 0;
}

/// Check if a low-duration pulse qualifies as a reset.
static inline bool isResetPulse(u32 low_ns,
                                const ChipsetTiming4Phase &timing) {
    return low_ns >= (static_cast<u32>(timing.reset_min_us) * 1000u);
}

/// Check if a pulse is a gap to tolerate (longer than normal but shorter
/// than reset). A gap must be LONGER than any valid bit LOW pulse to avoid
/// incorrectly skipping normal protocol data.
static inline bool isGapPulse(u32 low_ns,
                              const ChipsetTiming4Phase &timing) {
    if (timing.gap_tolerance_ns == 0) {
        return false;
    }
    u32 reset_ns = static_cast<u32>(timing.reset_min_us) * 1000u;
    // Gap must be longer than the longest valid bit LOW pulse
    u32 max_valid_low = (timing.t0l_max_ns > timing.t1l_max_ns)
                            ? timing.t0l_max_ns
                            : timing.t1l_max_ns;
    return low_ns > max_valid_low && low_ns < reset_ns &&
           low_ns <= timing.gap_tolerance_ns;
}

/// Decode an EdgeTime buffer into bytes (MSB-first).
///
/// Polarity-aware decoder: uses the HIGH/LOW labels from the gap-aware
/// edge builder. Edges come in HIGH/LOW pairs representing one bit each.
/// If polarity is wrong (noise), skip and resync on the next HIGH edge.
static fl::result<u32, DecodeError>
decodeEdges(const ChipsetTiming4Phase &timing,
            fl::span<const EdgeTime> edges, fl::span<u8> bytes_out) {
    if (edges.size() == 0 || bytes_out.size() == 0) {
        return fl::result<u32, DecodeError>::success(0);
    }

    u32 byte_index = 0;
    u8 current_byte = 0;
    u8 bit_count = 0;
    u32 error_count = 0;
    u32 total_bits = 0;
    u32 resync_count = 0;

    size_t i = 0;
    while (i + 1 < edges.size()) {
        // Expect edges[i] = HIGH, edges[i+1] = LOW
        if (!edges[i].high) {
            // Polarity error: skip this edge and resync on next HIGH
            ++resync_count;
            ++i;
            continue;
        }

        u32 high_ns = edges[i].ns;
        int bit = -1;
        if (i + 1 < edges.size() && !edges[i + 1].high) {
            u32 low_ns = edges[i + 1].ns;
            bit = decodeBit(high_ns, low_ns, timing);
            i += 2;
        } else {
            bit = decodeBitFromHigh(high_ns, timing);
            i += 1;
        }
        ++total_bits;

        if (bit < 0) {
            ++error_count;
            continue;
        }

        current_byte = (current_byte << 1) | static_cast<u8>(bit);
        ++bit_count;

        if (bit_count == 8) {
            if (byte_index >= bytes_out.size()) {
                return fl::result<u32, DecodeError>::failure(
                    DecodeError::BUFFER_OVERFLOW);
            }
            bytes_out[byte_index++] = current_byte;
            current_byte = 0;
            bit_count = 0;
        }
    }

    // Handle partial final byte (left-align remaining bits)
    if (bit_count > 0 && byte_index < bytes_out.size()) {
        bytes_out[byte_index++] =
            static_cast<u8>(current_byte << (8 - bit_count));
    }

    // Check error rate (>10% is considered too high)
    if (total_bits > 0 &&
        (error_count * 10) > total_bits) {
        return fl::result<u32, DecodeError>::failure(
            DecodeError::HIGH_ERROR_RATE);
    }

    return fl::result<u32, DecodeError>::success(byte_index);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// FlexPwmRxChannelImpl -- private implementation
// ---------------------------------------------------------------------------

class FlexPwmRxChannelImpl : public FlexPwmRxChannel {
  public:
    explicit FlexPwmRxChannelImpl(int pin) : mPin(pin) {}
    ~FlexPwmRxChannelImpl() override = default;

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
    void buildEdgeTimesFromCaptures();

    static void dmaIsr();
    static FlexPwmRxChannelImpl *sActiveInstance;

    friend fl::json FlexPwmRxChannel::diagnosticsToJson(int requested_pin) FL_NO_EXCEPT;

    int mPin = -1;
    const FlexPwmPinInfo *mPinInfo = nullptr;

    // DMA
    DMAChannel mDma;
    fl::vector<u16> mCaptureBuffer; // Raw 16-bit capture values from DMA
    size_t mBufferSize = 512;       // Requested buffer size in edge-pairs

    // State
    volatile bool mReceiveDone = false;
    bool mConfigured = false;
    bool mStartLow = true;

    // Decoded edge cache (built from mCaptureBuffer or injected)
    fl::vector<EdgeTime> mEdges;
    bool mEdgesValid = false;

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
        FL_WARN_F("Pin %s does not support FlexPWM capture on Teensy 4.x", mPin);
        return false;
    }

    mBufferSize = config.buffer_size;
    mSignalRangeMaxNs = config.signal_range_max_ns;
    mStartLow = config.start_low;
    mReceiveDone = false;
    mEdgesValid = false;
    mEdges.clear();

    // Allocate capture buffer: 2 captures per bit (rising + falling).
    // Cap to 8192 captures to avoid exhausting Teensy RAM (~16KB buffer).
    // For 100 LEDs × 24 bits = 2400 bits, we need ~4800 captures.
    size_t cap_count = mBufferSize * 2;
    if (cap_count > 8192) {
        cap_count = 8192;
    }
    mCaptureBuffer.clear();
    mCaptureBuffer.reserve(cap_count);
    for (size_t i = 0; i < cap_count; ++i) {
        mCaptureBuffer.push_back(0);
    }

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
    *(mPinInfo->mux_register) = mPinInfo->mux_value | 0x10; // SION bit
    if (mPinInfo->select_register) {
        *(mPinInfo->select_register) = mPinInfo->select_value;
    }

    // Disable the submodule while configuring
    pwm->MCTRL &= ~(FLEXPWM_MCTRL_RUN(1 << sm));

    // CTRL2: Clock source = IPBus clock (F_BUS_ACTUAL), no init from ext
    // CLK_SEL = 0 (IPBus clock), INIT_SEL = 0 (local sync)
    pwm->SM[sm].CTRL2 = 0;

    // CTRL: Full cycle reload, prescaler = 0 (divide by 1)
    pwm->SM[sm].CTRL = FLEXPWM_SMCTRL_FULL;

    // Free-running counter: INIT = 0, VAL1 = 0xFFFF (max period)
    pwm->SM[sm].INIT = 0;
    pwm->SM[sm].VAL1 = 0xFFFF;
    pwm->SM[sm].VAL0 = 0;
    pwm->SM[sm].VAL2 = 0;
    pwm->SM[sm].VAL3 = 0;
    pwm->SM[sm].VAL4 = 0;
    pwm->SM[sm].VAL5 = 0;

    if (!mPinInfo->channel_b) {
        // Channel A capture configuration (CAPTCTRLA / CAPTCOMPA)
        // EDGA0 = 2 captures rising edges to CVAL2.
        // EDGA1 = 1 captures falling edges to CVAL3.
        // DMA fires on CA1DE after both registers for the bit are valid.
        pwm->SM[sm].CAPTCTRLA = FLEXPWM_SMCAPTCTRLA_EDGA0(2) |
                                 FLEXPWM_SMCAPTCTRLA_EDGA1(1) |
                                 FLEXPWM_SMCAPTCTRLA_CFAWM(0) |
                                 FLEXPWM_SMCAPTCTRLA_ARMA;
        pwm->SM[sm].CAPTCOMPA = 0;

        // CAPTDE selects channel A capture DMA for the submodule read source.
        pwm->SM[sm].DMAEN = FLEXPWM_SMDMAEN_CAPTDE(1) | FLEXPWM_SMDMAEN_CA1DE;
    } else {
        // Channel B capture configuration (CAPTCTRLB / CAPTCOMPB)
        // EDGB0 = 2 captures rising edges to CVAL4.
        // EDGB1 = 1 captures falling edges to CVAL5.
        pwm->SM[sm].CAPTCTRLB = FLEXPWM_SMCAPTCTRLB_EDGB0(2) |
                                 FLEXPWM_SMCAPTCTRLB_EDGB1(1) |
                                 FLEXPWM_SMCAPTCTRLB_ARMB;
        pwm->SM[sm].CAPTCOMPB = 0;

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

    mDma.begin();
    mDma.TCD->SADDR = const_cast<u16 *>(capture_reg);
    mDma.TCD->SOFF = 4;
    mDma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(1) | DMA_TCD_ATTR_DSIZE(1);
    mDma.TCD->NBYTES_MLNO =
        DMA_TCD_NBYTES_MLOFFYES_NBYTES(4) |
        DMA_TCD_NBYTES_MLOFFYES_MLOFF(-8) |
        DMA_TCD_NBYTES_SMLOE;
    mDma.TCD->SLAST = 0;
    mDma.TCD->DADDR = mCaptureBuffer.data();
    mDma.TCD->DOFF = 2;
    mDma.TCD->CITER = mCaptureBuffer.size() / 2;
    mDma.TCD->DLASTSGA = 0;
    mDma.TCD->BITER = mCaptureBuffer.size() / 2;
    mDma.TCD->CSR = DMA_TCD_CSR_DREQ;
    mDma.triggerAtHardwareEvent(mPinInfo->dma_source);
    mDma.interruptAtCompletion();
    mDma.attachInterrupt(dmaIsr);
    mDma.enable();
}

// ---------------------------------------------------------------------------
// DMA ISR
// ---------------------------------------------------------------------------

void FlexPwmRxChannelImpl::dmaIsr() {
    if (sActiveInstance) {
        sActiveInstance->mDma.clearInterrupt();
        sActiveInstance->mReceiveDone = true;
    }
}

// ---------------------------------------------------------------------------
// finished() / wait()
// ---------------------------------------------------------------------------

bool FlexPwmRxChannelImpl::finished() const {
    if (mReceiveDone) {
        return true;
    }

    // Inactivity-based frame detection: check if DMA has stalled
    // (no new edges for longer than mSignalRangeMaxNs).
    // We approximate this by checking the DMA destination address progress.
    // If the DMA pointer hasn't moved in two consecutive checks separated
    // by at least signal_range_max_ns, we declare the frame complete.
    //
    // For simplicity, we check the DMA DADDR (destination address) which
    // gives us the current write position.
    return false;
}

RxWaitResult FlexPwmRxChannelImpl::wait(u32 timeout_ms) {
    u32 start = millis();
    const u16 initial_citer = mDma.TCD->CITER;
    u16 last_citer = initial_citer;
    u32 last_change_time = micros();
    bool exited_on_timeout = false;

    while (!mReceiveDone) {
        u32 now_ms = millis();
        if ((now_ms - start) >= timeout_ms) {
            // Hit caller's timeout. Don't claim SUCCESS unless DMA actually
            // moved -- if CITER is still at its initial value we got zero
            // edges and must report TIMEOUT honestly so capture() can bail
            // and runMultiTest() can emit a real failure instead of decoding
            // a stale/empty buffer.
            exited_on_timeout = true;
            break;
        }

        // Check DMA progress for inactivity-based frame detection.
        // Use CITER (remaining iterations) instead of DADDR, because
        // destinationBuffer() creates circular DMA that wraps DADDR.
        u16 current_citer = mDma.TCD->CITER;
        u32 now_us = micros();

        if (current_citer != last_citer) {
            last_citer = current_citer;
            last_change_time = now_us;
        } else {
            // No progress -- check if idle long enough to declare frame done
            u32 idle_us = now_us - last_change_time;
            if (idle_us >= (mSignalRangeMaxNs / 1000)) {
                // Frame complete due to inactivity
                break;
            }
        }

        yield();
    }

    // Honesty: distinguish "actually got data" from "timeout with nothing".
    //   - mReceiveDone     -> full buffer, definitely SUCCESS
    //   - CITER moved      -> partial buffer, SUCCESS (caller decodes what
    //                          arrived; inactivity-detection path lands here)
    //   - CITER unchanged  -> nothing arrived; do not pretend it did
    const u16 current_citer = mDma.TCD->CITER;
    const bool dma_progressed = mReceiveDone || (current_citer != initial_citer);
    if (!dma_progressed) {
        return RxWaitResult::TIMEOUT;
    }
    (void)exited_on_timeout;  // retained for future telemetry; see #3219
    return RxWaitResult::SUCCESS;
}

// ---------------------------------------------------------------------------
// buildEdgeTimesFromCaptures() -- convert raw captures to EdgeTime
// ---------------------------------------------------------------------------

void FlexPwmRxChannelImpl::buildEdgeTimesFromCaptures() {
    if (mEdgesValid) {
        return;
    }
    mEdges.clear();

    // Determine how many captures the DMA actually wrote.
    //
    // Two methods, depending on whether the DMA completed its major loop:
    //   1. If DMA completion ISR fired (mReceiveDone=true), the entire buffer
    //      was filled. CITER has already reloaded from BITER, so BITER-CITER=0
    //      is misleading — we actually have a full buffer.
    //   2. If DMA is still running or timed out, BITER-CITER gives the number
    //      of completed bit-pair transfers. Each transfer writes two captures.
    //
    // We also check the DONE bit in TCD->CSR as a secondary indicator.
    u16 biter = mDma.TCD->BITER;
    u16 citer = mDma.TCD->CITER;
    bool dma_done = mReceiveDone || (mDma.TCD->CSR & DMA_TCD_CSR_DONE);
    size_t captures_written = 0;

    if (dma_done && (biter == citer)) {
        // DMA completed a full major loop — entire buffer is valid
        captures_written = mCaptureBuffer.size();
    } else if (biter >= citer) {
        captures_written = static_cast<size_t>(biter - citer) * 2u;
    }
    if (captures_written > mCaptureBuffer.size()) {
        captures_written = mCaptureBuffer.size();
    }

    // CRITICAL: Invalidate D-cache for the capture buffer region.
    // DMA writes bypass the CPU cache, so we must invalidate to see
    // the data DMA actually wrote. Without this, the CPU reads stale
    // cache lines and gets all-zero capture values.
    if (captures_written > 0) {
        arm_dcache_delete(mCaptureBuffer.data(),
                          captures_written * sizeof(u16));
    }

    if (captures_written < 2) {
        mEdgesValid = true;
        return;
    }

    // Skip leading phantom pairs. Empirically (#3219, 2026-06-22 bench
    // capture on Teensy 4.0), the IOMUXC pad-mux switch + DMA arm sequence
    // sometimes latches a stray rising+falling pair before the OBJECT_FLED
    // TX driver emits its first real bit. That phantom pair has a
    // plausible-looking HIGH duration (~226 ns) but is followed by a
    // multi-microsecond IDLE before the real frame starts. The original
    // loop pushed the phantom HIGH unconditionally, then skipped the
    // subsequent gap as a "long LOW" via the `low_ns > 5000` branch --
    // leaving a stray leading HIGH in `mEdges` that shifted every bit
    // decoded downstream by one. We must drop ANY leading pair whose
    // following LOW is a reset/idle gap, not just the LOW itself.
    size_t start_i = 0;
    while (start_i + 3 < captures_written) {
        u16 fall = mCaptureBuffer[start_i + 1];
        u16 next_rise = mCaptureBuffer[start_i + 2];
        u32 low_ns = tickDeltaNs(fall, next_rise);
        if (low_ns > 5000) {
            start_i += 2;  // phantom pair + its trailing gap; resume search
        } else {
            break;  // first real intra-frame pair starts here
        }
    }

    // Build EdgeTime pairs from paired rising/falling captures. DMA writes
    // [rise0, fall0, rise1, fall1, ...]. High time is the delta inside a pair;
    // low time is the delta from one pair's falling edge to the next pair's
    // rising edge.

    mEdges.reserve(captures_written - start_i);

    for (size_t i = start_i; i + 3 < captures_written; i += 2) {
        u16 rise = mCaptureBuffer[i];
        u16 fall = mCaptureBuffer[i + 1];
        u16 next_rise = mCaptureBuffer[i + 2];

        u32 high_ns = tickDeltaNs(rise, fall);
        u32 low_ns = tickDeltaNs(fall, next_rise);

        mEdges.push_back(EdgeTime(true, high_ns));

        if (low_ns > 5000) {
            continue;
        }

        mEdges.push_back(EdgeTime(false, low_ns));
    }

    size_t last_pair = captures_written - 2;
    u32 final_high_ns =
        tickDeltaNs(mCaptureBuffer[last_pair], mCaptureBuffer[last_pair + 1]);
    mEdges.push_back(EdgeTime(true, final_high_ns));

    mEdgesValid = true;
}

// ---------------------------------------------------------------------------
// decode()
// ---------------------------------------------------------------------------

fl::result<u32, DecodeError>
FlexPwmRxChannelImpl::decode(const ChipsetTiming4Phase &timing,
                              fl::span<u8> out) {
    buildEdgeTimesFromCaptures();

    if (mEdges.size() == 0) {
        return fl::result<u32, DecodeError>::success(0);
    }

    fl::span<const EdgeTime> edge_span(mEdges.data(), mEdges.size());
    return decodeEdges(timing, edge_span, out);
}

// ---------------------------------------------------------------------------
// getRawEdgeTimes()
// ---------------------------------------------------------------------------

size_t FlexPwmRxChannelImpl::getRawEdgeTimes(fl::span<EdgeTime> out,
                                              size_t offset) {
    buildEdgeTimesFromCaptures();

    if (offset >= mEdges.size()) {
        return 0;
    }

    size_t available = mEdges.size() - offset;
    size_t to_copy = (available < out.size()) ? available : out.size();

    for (size_t i = 0; i < to_copy; ++i) {
        out[i] = mEdges[offset + i];
    }
    return to_copy;
}

// ---------------------------------------------------------------------------
// injectEdges()
// ---------------------------------------------------------------------------

bool FlexPwmRxChannelImpl::injectEdges(fl::span<const EdgeTime> edges) {
    mEdges.clear();
    mEdges.reserve(edges.size());
    for (size_t i = 0; i < edges.size(); ++i) {
        mEdges.push_back(edges[i]);
    }
    mEdgesValid = true;
    mReceiveDone = true;
    return true;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

fl::shared_ptr<FlexPwmRxChannel> FlexPwmRxChannel::create(int pin) {
    const FlexPwmPinInfo *info = lookupPin(pin);
    if (!info) {
        FL_WARN_F("Pin %s does not support FlexPWM capture on Teensy 4.x", pin);
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
    out.set("edgesValid", active->mEdgesValid);
    out.set("edgeCount", static_cast<i64>(active->mEdges.size()));
    out.set("captureBufferSize", static_cast<i64>(active->mCaptureBuffer.size()));
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
