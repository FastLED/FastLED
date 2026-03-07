/// @file rx_flexpwm_channel.cpp.hpp
/// @brief Teensy 4.x FlexPWM input-capture RX implementation
///
/// Hardware pipeline (based on Paul Stoffregen's WS2812Capture):
///   1. FlexPWM submodule runs a free-running 16-bit counter at F_BUS_ACTUAL
///      (150 MHz on Teensy 4.x, giving ~6.67 ns per tick).
///   2. Single-circuit any-edge capture (EDGA0=3) latches the counter value
///      into CVAL2 (channel A) or CVAL4 (channel B) on every edge.
///   3. Each capture event triggers a DMA request (CA0DE or CB0DE). A Teensy
///      DMAChannel copies the 16-bit capture value into a RAM buffer.
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

#define FASTLED_INTERNAL
#include "fl/fastled.h"
#include "platforms/arm/teensy/teensy4_common/rx_flexpwm_channel.h"

#include "fl/stl/vector.h"
#include "fl/warn.h"
#include "fl/result.h"
#include "fl/stl/cstring.h"

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
//   Channel A capture: CVAL2 (any edge, single-circuit mode EDGA0=3)
//   Channel B capture: CVAL4 (any edge, single-circuit mode EDGB0=3)
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
/// Returns 0, 1, or -1 (unrecognised).
static inline int decodeBit(u32 high_ns, u32 low_ns,
                            const ChipsetTiming4Phase &timing) {
    // Check bit-0 thresholds
    if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns &&
        low_ns >= timing.t0l_min_ns && low_ns <= timing.t0l_max_ns) {
        return 0;
    }
    // Check bit-1 thresholds
    if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns &&
        low_ns >= timing.t1l_min_ns && low_ns <= timing.t1l_max_ns) {
        return 1;
    }
    return -1;
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
static fl::Result<u32, DecodeError>
decodeEdges(const ChipsetTiming4Phase &timing,
            fl::span<const EdgeTime> edges, fl::span<u8> bytes_out) {
    if (edges.size() == 0 || bytes_out.size() == 0) {
        return fl::Result<u32, DecodeError>::success(0);
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
        u32 low_ns = edges[i + 1].ns;
        i += 2;

        int bit = decodeBit(high_ns, low_ns, timing);
        ++total_bits;

        if (bit < 0) {
            ++error_count;
            continue;
        }

        current_byte = (current_byte << 1) | static_cast<u8>(bit);
        ++bit_count;

        if (bit_count == 8) {
            if (byte_index >= bytes_out.size()) {
                return fl::Result<u32, DecodeError>::failure(
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

#ifdef FL_DEBUG
    FL_WARN("[FlexPWM DECODE] bytes=" << byte_index
            << " total_bits=" << total_bits
            << " errors=" << error_count
            << " resyncs=" << resync_count
            << " edges=" << edges.size());
    if (edges.size() >= 4) {
        FL_WARN("[FlexPWM DECODE] e[0]=" << (edges[0].high?"H":"L") << edges[0].ns
                << " e[1]=" << (edges[1].high?"H":"L") << edges[1].ns
                << " e[2]=" << (edges[2].high?"H":"L") << edges[2].ns
                << " e[3]=" << (edges[3].high?"H":"L") << edges[3].ns);
    }
#endif

    // Check error rate (>10% is considered too high)
    if (total_bits > 0 &&
        (error_count * 10) > total_bits) {
        return fl::Result<u32, DecodeError>::failure(
            DecodeError::HIGH_ERROR_RATE);
    }

    return fl::Result<u32, DecodeError>::success(byte_index);
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
    fl::Result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
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
        FL_WARN("Pin " << mPin
                        << " does not support FlexPWM capture on Teensy 4.x");
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
        // EDGA0 = 3 (any edge on circuit 0), captures to CVAL2
        // Using single circuit ensures DMA always reads the correct register.
        // CFAWM = 0 (FIFO watermark = 1 entry → DMA fires on each capture)
        // ARM = 1 (arm capture), ONESHOTA = 0 (free-running)
        pwm->SM[sm].CAPTCTRLA = FLEXPWM_SMCAPTCTRLA_EDGA0(3) |
                                 FLEXPWM_SMCAPTCTRLA_CFAWM(0) |
                                 FLEXPWM_SMCAPTCTRLA_ARMA;
        pwm->SM[sm].CAPTCOMPA = 0;

        // Enable both per-event DMA (CA0DE) and FIFO watermark DMA (CAPTDE=1)
        // to ensure the DMAMUX_SOURCE_FLEXPWMn_READm trigger fires.
        pwm->SM[sm].DMAEN = FLEXPWM_SMDMAEN_CA0DE | FLEXPWM_SMDMAEN_CAPTDE(1);
    } else {
        // Channel B capture configuration (CAPTCTRLB / CAPTCOMPB)
        // EDGB0 = 3 (any edge on circuit 0), captures to CVAL4
        pwm->SM[sm].CAPTCTRLB = FLEXPWM_SMCAPTCTRLB_EDGB0(3) |
                                 FLEXPWM_SMCAPTCTRLB_ARMB;
        pwm->SM[sm].CAPTCOMPB = 0;

        // Enable both per-event DMA (CB0DE) and FIFO watermark DMA (CAPTDE=2)
        pwm->SM[sm].DMAEN = FLEXPWM_SMDMAEN_CB0DE | FLEXPWM_SMDMAEN_CAPTDE(2);
    }

    // Start the submodule counter
    pwm->MCTRL |= FLEXPWM_MCTRL_RUN(1 << sm);

#ifdef FL_DEBUG
    FL_WARN("[FlexPWM CFG] pin=" << static_cast<int>(mPinInfo->pin)
            << " pwm" << (pwm == &IMXRT_FLEXPWM1 ? 1 : pwm == &IMXRT_FLEXPWM2 ? 2 : pwm == &IMXRT_FLEXPWM3 ? 3 : 4)
            << " sm=" << static_cast<int>(sm)
            << " chB=" << mPinInfo->channel_b);
    FL_WARN("[FlexPWM CFG] MCTRL=0x" << fl::hex << pwm->MCTRL
            << " CTRL2=0x" << pwm->SM[sm].CTRL2
            << " CTRL=0x" << pwm->SM[sm].CTRL << fl::dec);
    if (!mPinInfo->channel_b) {
        FL_WARN("[FlexPWM CFG] CAPTCTRLA=0x" << fl::hex << pwm->SM[sm].CAPTCTRLA
                << " DMAEN=0x" << pwm->SM[sm].DMAEN
                << " STS=0x" << pwm->SM[sm].STS << fl::dec);
    } else {
        FL_WARN("[FlexPWM CFG] CAPTCTRLB=0x" << fl::hex << pwm->SM[sm].CAPTCTRLB
                << " DMAEN=0x" << pwm->SM[sm].DMAEN
                << " STS=0x" << pwm->SM[sm].STS << fl::dec);
    }
    FL_WARN("[FlexPWM CFG] MUX=0x" << fl::hex << *(mPinInfo->mux_register) << fl::dec);
#endif
}

// ---------------------------------------------------------------------------
// DMA configuration
// ---------------------------------------------------------------------------

void FlexPwmRxChannelImpl::configureDma() {
    sActiveInstance = this;

    // Source: FlexPWM capture value register (single-circuit any-edge mode)
    // Channel A: CVAL2 (all edges captured to circuit 0)
    // Channel B: CVAL4 (all edges captured to circuit 0)
    volatile u16 *capture_reg;
    if (!mPinInfo->channel_b) {
        capture_reg = &(mPinInfo->pwm->SM[mPinInfo->submodule].CVAL2);
    } else {
        capture_reg = &(mPinInfo->pwm->SM[mPinInfo->submodule].CVAL4);
    }

    mDma.begin();
    mDma.source(*capture_reg);
    mDma.destinationBuffer(mCaptureBuffer.data(),
                           mCaptureBuffer.size() * sizeof(u16));
    mDma.transferSize(2);                   // 16-bit transfers
    mDma.transferCount(mCaptureBuffer.size());
    mDma.triggerAtHardwareEvent(mPinInfo->dma_source);

    // Auto-disable DMA after all iterations (DREQ)
    mDma.disableOnCompletion();
    mDma.interruptAtCompletion();
    mDma.attachInterrupt(dmaIsr);
    mDma.enable();

#ifdef FL_DEBUG
    // Debug: dump DMA channel and DMAMUX state
    volatile u32 *dmamux_reg = &DMAMUX_CHCFG0 + mDma.channel;
    uintptr_t dma_daddr = mDma.TCD->DADDR; // ok reading register
    uintptr_t buf_addr = reinterpret_cast<uintptr_t>(mCaptureBuffer.data()); // ok reinterpret cast
    FL_WARN("[FlexPWM DMA] ch=" << mDma.channel
            << " src=" << static_cast<int>(mPinInfo->dma_source)
            << " DMAMUX=0x" << fl::hex << *dmamux_reg << fl::dec
            << " CITER=" << static_cast<int>(mDma.TCD->CITER)
            << " ERQ=" << (DMA_ERQ & (1 << mDma.channel) ? 1 : 0)
            << " DADDR_match=" << (dma_daddr == buf_addr ? "YES" : "NO"));
#endif
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
    u16 last_citer = mDma.TCD->CITER;
    u32 last_change_time = micros();

    while (!mReceiveDone) {
        u32 now_ms = millis();
        if ((now_ms - start) >= timeout_ms) {
            // Timeout -- but we may have partial data; treat it as success
            // if DMA has captured any edges.
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

    // Calculate how many captures DMA actually wrote
    // (This is handled in buildEdgeTimesFromCaptures)

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
    //      of completed transfers.
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
        captures_written = biter - citer;
    }
    if (captures_written > mCaptureBuffer.size()) {
        captures_written = mCaptureBuffer.size();
    }

#ifdef FL_DEBUG
    FL_WARN("[FlexPWM RX] DMA captures_written=" << captures_written
            << " BITER=" << biter << " CITER=" << citer
            << " done=" << (dma_done ? 1 : 0)
            << " mReceiveDone=" << (mReceiveDone ? 1 : 0)
            << " ch=" << mDma.channel
            << " pin=" << static_cast<int>(mPinInfo->pin));
#endif

    // CRITICAL: Invalidate D-cache for the capture buffer region.
    // DMA writes bypass the CPU cache, so we must invalidate to see
    // the data DMA actually wrote. Without this, the CPU reads stale
    // cache lines and gets all-zero capture values.
    if (captures_written > 0) {
        arm_dcache_delete(mCaptureBuffer.data(),
                          captures_written * sizeof(u16));
    }

#ifdef FL_DEBUG
    if (captures_written >= 8) {
        FL_WARN("[FlexPWM RAW] first: " << mCaptureBuffer[0] << " " << mCaptureBuffer[1]
                << " " << mCaptureBuffer[2] << " " << mCaptureBuffer[3]);
        size_t mid = captures_written / 2;
        FL_WARN("[FlexPWM RAW] mid[" << mid << "]: " << mCaptureBuffer[mid] << " " << mCaptureBuffer[mid+1]
                << " " << mCaptureBuffer[mid+2] << " " << mCaptureBuffer[mid+3]);
    }
#endif

    if (captures_written < 2) {
        mEdgesValid = true;
        return;
    }

    // Build EdgeTime pairs from consecutive capture deltas.
    // Gap-aware polarity tracker: ObjectFLED transmits in DMA blocks
    // with ~300µs LOW gaps between them. These gaps shift even/odd
    // polarity alignment. We detect gaps (>5µs) and reset polarity
    // since the signal idles LOW during gaps, so the first edge
    // after a gap is always rising (HIGH).

    mEdges.reserve(captures_written);

    bool next_is_high = true; // First edge after idle/start is rising = HIGH
    for (size_t i = 0; i + 1 < captures_written; ++i) {
        u16 t0 = mCaptureBuffer[i];
        u16 t1 = mCaptureBuffer[i + 1];
        u32 ns = tickDeltaNs(t0, t1);

        // Skip gaps (ObjectFLED DMA block boundaries, ~300µs)
        // and reset polarity since signal returns to LOW during gap
        if (ns > 5000) { // 5µs threshold
            next_is_high = true; // After gap, next edge is rising
            continue;
        }

        mEdges.push_back(EdgeTime(next_is_high, ns));
        next_is_high = !next_is_high;
    }

#ifdef FL_DEBUG
    FL_WARN("[FlexPWM EDGE] total=" << mEdges.size());
    if (mEdges.size() >= 8) {
        FL_WARN("[FlexPWM E] 0:" << (mEdges[0].high?"H":"L") << mEdges[0].ns
                << " 1:" << (mEdges[1].high?"H":"L") << mEdges[1].ns
                << " 2:" << (mEdges[2].high?"H":"L") << mEdges[2].ns
                << " 3:" << (mEdges[3].high?"H":"L") << mEdges[3].ns);
        size_t mid = mEdges.size() / 2;
        FL_WARN("[FlexPWM E@" << mid << "] "
                << (mEdges[mid].high?"H":"L") << mEdges[mid].ns
                << " " << (mEdges[mid+1].high?"H":"L") << mEdges[mid+1].ns
                << " " << (mEdges[mid+2].high?"H":"L") << mEdges[mid+2].ns
                << " " << (mEdges[mid+3].high?"H":"L") << mEdges[mid+3].ns);
    }
#endif

    mEdgesValid = true;
}

// ---------------------------------------------------------------------------
// decode()
// ---------------------------------------------------------------------------

fl::Result<u32, DecodeError>
FlexPwmRxChannelImpl::decode(const ChipsetTiming4Phase &timing,
                              fl::span<u8> out) {
    buildEdgeTimesFromCaptures();

    if (mEdges.size() == 0) {
        return fl::Result<u32, DecodeError>::success(0);
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
        FL_WARN("Pin " << pin
                        << " does not support FlexPWM capture on Teensy 4.x");
        return fl::shared_ptr<FlexPwmRxChannel>();
    }
    return fl::make_shared<FlexPwmRxChannelImpl>(pin);
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
