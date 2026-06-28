// IWYU pragma: private

/// @file objectfled_spi_mode.cpp.hpp
/// @brief ObjectFLED SPI-mode hardware programming for i.MX RT1062 (Teensy 4.x).
///
/// Pairs with `OjectFLED.cpp.hpp` (clockless WS2812 mode) -- both files
/// drive GPIO6-resident pins, so they MUST be mutually exclusive at runtime.
/// We serialize via ObjectFLEDDmaManager::acquire()/release().
///
/// Hardware layout (distinct from the clockless side):
///   - Timing source : FlexPWM2 submodule 0 (clockless uses QTimer4).
///   - DMA channel   : one allocated via DMAChannel(), trigger source
///                     DMAMUX_SOURCE_FLEXPWM2_WRITE0. Clockless uses XBAR1
///                     sources (30/31/94) -- no overlap.
///   - DMA dest      : GPIO6_DR (whole 32-bit register write). Clockless
///                     uses GPIO1_DR_SET/CLEAR via the IOMUXC_GPR_GPR26
///                     remap -- different alias surface, no conflict at
///                     init time. NOTE: we deliberately do NOT touch GPR26;
///                     this means: (a) we always operate on the native
///                     GPIO6, and (b) we are racy with any other code that
///                     writes GPIO6_DR mid-transfer (e.g. other FastLED
///                     drivers writing other GPIO6 pins). Documented and
///                     accepted -- normal LED frame-loop usage owns the
///                     bank end-to-end.
///
/// SPI configuration:
///   - Mode 0 (CPOL=0, CPHA=0), MSB-first, 8-bit beats, no inter-byte gap.
///   - Clock clamped to [100 kHz, 25 MHz]. APA102 spec is 30 MHz max;
///     we keep margin for cable + chipset variance, matching the FlexIO
///     SPI driver in flexio_spi_mode.cpp.hpp.
///
/// Bit-banged DMA encoding: each input byte expands to 16 32-bit DMA
/// words. For byte b, bit n (n = 7 down to 0), we emit two words:
///       phase-A (SCLK=0): other_gpio6 | (b.bn << mosi_bit)
///       phase-B (SCLK=1): other_gpio6 | (b.bn << mosi_bit) | sclk_mask
/// `other_gpio6` is snapshotted at init from GPIO6_DR and preserved so
/// we don't disturb unrelated GPIO6 outputs.
///
/// References:
///   - NXP i.MX RT1060 Reference Manual, rev 3, chapter 30 (FlexPWM) and
///     chapter 6 (eDMA).
///   - imxrt.h (Teensyduino) for register macros / DMAMUX source IDs.
///   - flexio_spi_mode.cpp.hpp -- bounded-timeout + ISR-safe-delete patterns.
///
/// This file is NEW for #3428 and the FlexPWM2_SM0 register programming
/// below has only had compile-time validation -- bring-up on real hardware
/// is required. TODO markers tag the fields/values that need the most
/// scrutiny on first scope-on-MOSI verification.

#ifndef CHANNEL_ENGINE_OBJECTFLED_SPI_MODE_CPP_HPP_
#define CHANNEL_ENGINE_OBJECTFLED_SPI_MODE_CPP_HPP_

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_spi_mode.h"

#include "fl/log/log.h"
#include "fl/stl/cstring.h"

// IWYU pragma: begin_keep
#include "third_party/object_fled/src/ObjectFLEDDmaManager.h"
#include <Arduino.h>
#include <imxrt.h>
#include <DMAChannel.h>
// IWYU pragma: end_keep

// xbar_connect() is declared inside Teensyduino's pwm.c without a public
// header. Forward-declare here so we can call it from this TU.
extern "C" void xbar_connect(unsigned int input, unsigned int output);

namespace fl {

// ============================================================================
// Compile-time configuration
// ============================================================================

// Maximum input bytes per show(); APA102/SK9822 frames for typical LED
// strips (start-frame + 4B/LED + end-frame) easily fit in 4 KB output.
// 64 input bytes * 16 DMA words/byte * 4 bytes/word = 4096 bytes encoded.
// Place in DMAMEM (OCRAM2) to match the clockless mode's bitdata buffer
// location; DMAMEM is uncached on T4 so we avoid every-show cache flushes.
// TODO(#3428): once the channel engine reports max-frame size, size this
// from that instead of a hard 64.
#ifndef FL_OBJECTFLED_SPI_MAX_BYTES
#define FL_OBJECTFLED_SPI_MAX_BYTES 64
#endif

static constexpr u32 kSpiMaxInputBytes  = (u32)FL_OBJECTFLED_SPI_MAX_BYTES;
static constexpr u32 kSpiWordsPerByte   = 16u;
static constexpr u32 kSpiMaxOutputWords = kSpiMaxInputBytes * kSpiWordsPerByte;

// Clock-rate clamp window (matches flexio_spi_mode.cpp.hpp).
static constexpr u32 kSpiClockMinHz =   100000u;
static constexpr u32 kSpiClockMaxHz = 25000000u;

// IOMUXC pad tuning -- mirror the clockless mode's OUTPUT_PAD_SPEED=0,
// DSE=3. TODO(#3428): if SPI loopback at 25 MHz shows undershoot, bump
// FL_OBJECTFLED_SPI_DSE to 6 and SPEED to 2 (flexio_spi defaults).
#ifndef FL_OBJECTFLED_SPI_DSE
#define FL_OBJECTFLED_SPI_DSE 3
#endif
#ifndef FL_OBJECTFLED_SPI_SPEED
#define FL_OBJECTFLED_SPI_SPEED 0
#endif

// ============================================================================
// Module state (file-static, ISR-shared)
// ============================================================================

static DMAChannel* sSpiDmaChannel = nullptr;
static volatile bool sSpiDmaComplete = true;
static bool sSpiInitialized = false;
static ObjectFLEDSPIPinInfo sSpiCurrentPins{};

// Pre-encoded DMA bit-pattern buffer. DMAMEM = OCRAM2 (uncached on T4),
// matches dma.bitdata in ObjectFLEDDmaManager. 32-byte aligned so the
// SADDR satisfies eDMA 32-bit beat alignment.
static DMAMEM u32 sSpiBitPattern[kSpiMaxOutputWords] __attribute__((used, aligned(32)));

// Saved IOMUXC pad-mux values so deinit() can restore pad ALT5 (GPIO).
static u32 sSpiSavedMosiMux = 0;
static u32 sSpiSavedSclkMux = 0;

// Diagnostics snapshot captured in wait() BEFORE recovery clears state.
// Read by `objectfled_spi_read_diagnostics()` after wait() returns.
static ObjectFLEDSPIDiagnostics sSpiLastDiag{};

// ============================================================================
// GPIO6 pin-bank validation
// ============================================================================

// Returns true if `pin` is a valid digital pin AND its native port is
// GPIO6. Mirrors the offset-from-GPIO6_DR check used in
// ObjectFLED::begin() (OjectFLED.cpp.hpp:244).
static bool pin_is_gpio6(u8 pin) FL_NO_EXCEPT {
    if (pin >= NUM_DIGITAL_PINS) return false;
    const u32 reg = (u32)portOutputRegister(pin);
    const u32 base = (u32)&GPIO6_DR;
    return reg == base;
}

// ============================================================================
// DMA ISR
// ============================================================================

static void objectfled_spi_dma_isr() {
    if (sSpiDmaChannel != nullptr) {
        sSpiDmaChannel->clearInterrupt();
    }
    asm volatile("dsb" ::: "memory");
    sSpiDmaComplete = true;
}

// ============================================================================
// objectfled_spi_lookup_pins
// ============================================================================

bool objectfled_spi_lookup_pins(u8 mosi_pin, u8 sclk_pin,
                                ObjectFLEDSPIPinInfo* info) FL_NO_EXCEPT {
    if (!info) return false;
    if (mosi_pin == sclk_pin) return false;
    if (mosi_pin >= NUM_DIGITAL_PINS) return false;
    if (sclk_pin >= NUM_DIGITAL_PINS) return false;
    if (!pin_is_gpio6(mosi_pin)) {
        FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: MOSI pin %s is not on GPIO6", (int)mosi_pin);
        return false;
    }
    if (!pin_is_gpio6(sclk_pin)) {
        FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: SCLK pin %s is not on GPIO6", (int)sclk_pin);
        return false;
    }

    const u8 mosi_bit = (u8)digitalPinToBit(mosi_pin);
    const u8 sclk_bit = (u8)digitalPinToBit(sclk_pin);
    if (mosi_bit > 31 || sclk_bit > 31) return false;

    const u32 mosi_mask = 1u << mosi_bit;
    const u32 sclk_mask = 1u << sclk_bit;
    if (mosi_mask & sclk_mask) return false;

    info->mosi_pin     = mosi_pin;
    info->sclk_pin     = sclk_pin;
    info->mosi_bit     = mosi_bit;
    info->sclk_bit     = sclk_bit;
    info->mosi_mask    = mosi_mask;
    info->sclk_mask    = sclk_mask;
    info->mosi_mux_reg = portConfigRegister(mosi_pin);
    info->mosi_pad_reg = portControlRegister(mosi_pin);
    info->sclk_mux_reg = portConfigRegister(sclk_pin);
    info->sclk_pad_reg = portControlRegister(sclk_pin);
    return true;
}

// ============================================================================
// FlexPWM2_SM0 baud setup helper
// ============================================================================

// Program FlexPWM2 submodule 0 to fire DMA write requests at `req_hz`
// (which is 2 * SCLK = one request per DMA word).
// TODO(#3428): verify against RM 30 -- exact register sequence below
// uses LDOK + CLDOK pattern from Teensyduino's pwm.c analogWrite path.
// Program QTimer3 channel 0 to generate periodic compare events at
// `req_hz`, routed through XBAR1 to DMA_CH_MUX_REQ95 (DMAMUX_SOURCE_XBAR1_3).
// Mirrors the clockless ObjectFLED QTimer4 setup at OjectFLED.cpp.hpp:305-335
// but on QTimer3 (QTimer4 is taken) and routes to the only unclaimed XBAR
// DMA slot (req95; clockless uses 30/31/94).
static void objectfled_spi_program_qtimer(u32 req_hz) FL_NO_EXCEPT {
    // Disable channel 0 so we can re-program safely.
    TMR3_ENBL &= ~1u;

    // Status/control: output enable, FORCE clears OFLAG, MSTR drives output.
    TMR3_SCTRL0 = TMR_SCTRL_OEN | TMR_SCTRL_FORCE | TMR_SCTRL_MSTR;
    // Compare/status: CL1=1 (capture-load from CMPLD1), TCF1EN enables the
    // compare-1 flag that drives the XBAR input edge.
    TMR3_CSCTRL0 = TMR_CSCTRL_CL1(1) | TMR_CSCTRL_TCF1EN;
    TMR3_CNTR0 = 0;
    TMR3_LOAD0 = 0;

    // Compare period: F_BUS_ACTUAL counts per second / req_hz.
    u32 ipg_hz = (u32)F_BUS_ACTUAL;
    if (ipg_hz == 0) ipg_hz = 150000000u;
    u32 period = ipg_hz / req_hz;
    if (period < 4u)      period = 4u;
    if (period > 0xFFFFu) period = 0xFFFFu;
    TMR3_COMP10 = (u16)period;
    TMR3_CMPLD10 = (u16)period;

    // CTRL: CM(1)=count rising IP clock, PCS(8)=prescaler IP clock /1,
    // LENGTH=count until COMP1 then reload from LOAD, OUTMODE(3)=toggle
    // output on COMP1 (gives a 50% duty toggle the XBAR sees as edges).
    TMR3_CTRL0 = TMR_CTRL_CM(1) | TMR_CTRL_PCS(8) |
                 TMR_CTRL_LENGTH | TMR_CTRL_OUTMODE(3);

    // Route QTIMER3_TIMER0 -> DMA_CH_MUX_REQ95 via XBAR1.
    CCM_CCGR2 |= CCM_CCGR2_XBAR1(CCM_CCGR_ON);
    xbar_connect(XBARA1_IN_QTIMER3_TIMER0, XBARA1_OUT_DMA_CH_MUX_REQ95);

    // Configure XBARA1_CTRL1 slot 1 (DMA output 3 = req95) for edge-detect
    // DMA enable. We OR-in our bits without disturbing slot 0 (which the
    // clockless mode may have configured for req94). See clockless setup
    // at OjectFLED.cpp.hpp:166-173 for the inverse pattern.
    {
        u16 ctrl1 = XBARA1_CTRL1;
        // Clear our slot's STS bit before re-asserting (W1C semantics).
        ctrl1 &= ~XBARA_CTRL_STS1;
        ctrl1 |= XBARA_CTRL_STS1 | XBARA_CTRL_EDGE1(3) | XBARA_CTRL_DEN1;
        XBARA1_CTRL1 = ctrl1;
    }
}

// ============================================================================
// objectfled_spi_init
// ============================================================================

bool objectfled_spi_init(const ObjectFLEDSPIPinInfo& pin_info,
                         u32 clock_hz) FL_NO_EXCEPT {
    // #3428 HARDWARE BRING-UP INCOMPLETE: the FlexPWM2_SM0 register
    // sequence below has had a CCM gate fix (bits 18-19, not 22-23) but
    // a remaining issue still hard-faults the Teensy on the first show()
    // call. Likely culprits to investigate on first scope-on-MOSI debug:
    //   - FlexPWM2 SM0CTRL2 INIT_SEL / CLK_SEL values (RM 30 cross-check).
    //   - eDMA channel allocation conflict with the OOM-test path.
    //   - GPIO6_DR direct-write while clockless mode's GPR26 remap is
    //     in a partially-configured state from a prior test boot.
    //
    // Bench-debug status update (#3428): two real bugs have been
    // root-caused and fixed this session via on-device Serial.flush()
    // checkpoint bisect on Teensy 4.1:
    //
    //   (1) `ObjectFLEDDmaManager::acquire()` spun on `dma3.complete()`
    //       which never returns true if clockless mode has never run
    //       (third-party first-use bug -> firmware hard fault). Fixed
    //       by bypassing the manager from SPI mode (see show()).
    //
    //   (2) `FLEXPWM2_SM0DMAEN = (1u << 4)` was setting `CA0DE`
    //       (Capture-A0 DMA Enable) not `VALDE` (which is bit 9).
    //       Fixed by using the named macro `FLEXPWM_SMDMAEN_VALDE`.
    //
    // With both fixes in place, init+show+wait+deinit completes cleanly
    // (no crash). BUT the DMA major-loop completion still doesn't
    // signal within the 50 ms timeout. Architectural finding: VALDE is
    // designed for "VAL register reload" DMA -- it's the right trigger
    // when DMA writes to FlexPWM's VAL FIFO (the Audio library pattern,
    // see Teensy `Audio/output_pwm.cpp:310`). It is NOT the right
    // trigger for our use case of "fire DMA on each PWM tick so we can
    // write to an arbitrary GPIO".
    //
    // Bench-debug history (#3428): three iterations to reach a working
    // DMA-transmit path on Teensy 4.1.
    //
    // Iteration 1 -- FlexPWM2 + VALDE:
    //   Wrong DMA-trigger primitive. VALDE fires when a PWM VAL register
    //   is reloaded (the use case in Teensy Audio's `output_pwm.cpp`),
    //   not on a generic "PWM cycle tick". Our DMA writes to GPIO6_DR
    //   never triggered.
    //
    // Iteration 2 -- QTimer3 + XBAR1 + DMA_CH_MUX_REQ95:
    //   Architecturally correct (mirrors clockless QTimer4 setup at
    //   OjectFLED.cpp.hpp:305-335), but DMA still failed with
    //   DMA_ES = 0x80000401 (DBE = destination bus error). Root cause:
    //   GPIO6-9 are "fast GPIO" on the Cortex-M7 private TCM bus,
    //   inaccessible from eDMA. Direct writes to &GPIO6_DR bus-fault.
    //
    // Iteration 3 -- QTimer3 + XBAR1 + GPIO1 remap (CURRENT, WORKING):
    //   Clear the matching IOMUXC_GPR_GPR26 bits to remap the pads from
    //   GPIO6 (fast/M7-only) to GPIO1 (system-bus = eDMA-reachable), then
    //   write DMA to &GPIO1_DR. Bit positions stay the same. Mirrors
    //   clockless mode (OjectFLED.cpp.hpp:264). Hardware-verified on
    //   Teensy 4.1 (COM20): wait_ms drops from 55 (bounded timeout) to
    //   5 (just the drain wait) for all three test configurations
    //   (1/6/12 MHz, 4/16/64 bytes). DMA_ERR=0, DMA_ES=0, DMA_INT
    //   cleared by ISR. The full lookup -> init -> show -> wait ->
    //   deinit chain transmits successfully end-to-end.
    //
    // What's still NOT verified:
    //   - Byte-level MOSI bit pattern (need scope to confirm 0xA5
    //     streams as 1010 0101 MSB-first).
    //   - APA102/SK9822 strip lighting up.
    //
    // Safety gate FL_OBJECTFLED_SPI_HARDWARE_ENABLE stays OFF by
    // default until the scope verification completes -- the design is
    // proven correct at the API level, but byte-level wire verification
    // is still pending.
#ifndef FL_OBJECTFLED_SPI_HARDWARE_ENABLE
    (void)pin_info;
    (void)clock_hz;
    FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: gate disabled (set "
                        "FL_OBJECTFLED_SPI_HARDWARE_ENABLE to enable). "
                        "DMA transmit IS hardware-verified on Teensy 4.1 "
                        "but byte-level scope verification of MOSI bit "
                        "pattern is still pending -- see #3428.");
    return false;
#else
    sSpiLastDiag = ObjectFLEDSPIDiagnostics{};  // reset captured snapshot
    if (clock_hz == 0) {
        FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: init refused -- clock_hz == 0");
        return false;
    }
    if (sSpiInitialized) {
        objectfled_spi_deinit();
    }

    if (clock_hz < kSpiClockMinHz) {
        FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: clock %s Hz below floor; clamp to %s",
                            (int)clock_hz, (int)kSpiClockMinHz);
        clock_hz = kSpiClockMinHz;
    } else if (clock_hz > kSpiClockMaxHz) {
        FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: clock %s Hz above ceiling; clamp to %s",
                            (int)clock_hz, (int)kSpiClockMaxHz);
        clock_hz = kSpiClockMaxHz;
    }

    FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: init MOSI=%s(bit %s) SCLK=%s(bit %s) @ %s Hz",
                        (int)pin_info.mosi_pin, (int)pin_info.mosi_bit,
                        (int)pin_info.sclk_pin, (int)pin_info.sclk_bit,
                        (int)clock_hz);
    // CCM gate for FlexPWM2. The macro is at bits 18-19 of CCGR4 (NOT
    // bits 22-23 as an earlier draft had it -- writing to the wrong bits
    // left FlexPWM2 ungated, and the first SM0CTRL2 write bus-faulted
    // the Teensy on bring-up. Verified against imxrt.h:1573.
    CCM_CCGR4 |= CCM_CCGR4_PWM2(CCM_CCGR_ON);

    // Snapshot pre-init mux ALT values so deinit() can restore them. Per
    // coderabbitai review on #3432.
    sSpiSavedMosiMux = *(pin_info.mosi_mux_reg);
    sSpiSavedSclkMux = *(pin_info.sclk_mux_reg);

    *(pin_info.mosi_mux_reg) = 5u;
    *(pin_info.mosi_pad_reg) =
        ((FL_OBJECTFLED_SPI_SPEED & 0x3u) << 6) |
        ((FL_OBJECTFLED_SPI_DSE   & 0x7u) << 3);

    *(pin_info.sclk_mux_reg) = 5u;
    *(pin_info.sclk_pad_reg) =
        ((FL_OBJECTFLED_SPI_SPEED & 0x3u) << 6) |
        ((FL_OBJECTFLED_SPI_DSE   & 0x7u) << 3);

    // CRITICAL (#3428 bench debug): eDMA cannot write to GPIO6_DR
    // directly -- GPIO6-9 are "fast GPIO" on the Cortex-M7 private TCM
    // bus, inaccessible from eDMA. Confirmed via DMA_ES = 0x80000401
    // (DBE = Destination Bus Error, channel 4) on our first transmit
    // attempt. The fix mirrors clockless mode (OjectFLED.cpp.hpp:264):
    // clear the matching IOMUXC_GPR_GPR26 bits to remap the pads from
    // GPIO6 (fast/M7-only) to GPIO1 (slow/system-bus = eDMA-reachable).
    // Bit positions in GPIO1_DR match the original GPIO6_DR positions.
    IOMUXC_GPR_GPR26 &= ~(pin_info.mosi_mask | pin_info.sclk_mask);

    // Now set GDIR / drive low on the GPIO1 alias (the bits are the
    // same; the alias is the eDMA-reachable mirror).
    GPIO1_GDIR |= (pin_info.mosi_mask | pin_info.sclk_mask);
    GPIO1_DR   &= ~(pin_info.mosi_mask | pin_info.sclk_mask);

    objectfled_spi_program_qtimer(2u * clock_hz);

    if (sSpiDmaChannel == nullptr) {
        sSpiDmaChannel = new DMAChannel();  // ok bare allocation -- one-shot, balanced in deinit
        if (sSpiDmaChannel == nullptr) {
            FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: failed to allocate DMA channel");
            return false;
        }
        if (sSpiDmaChannel->TCD == nullptr) {
            FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: DMA channel has null TCD");
            delete sSpiDmaChannel;  // ok bare allocation
            sSpiDmaChannel = nullptr;
            return false;
        }
        // XBAR1_3 -> DMA_CH_MUX_REQ95, driven by QTIMER3_TIMER0 compare event
        // (configured in objectfled_spi_program_qtimer). DMAMUX_SOURCE_XBAR1_3 = 95.
        sSpiDmaChannel->triggerAtHardwareEvent(DMAMUX_SOURCE_XBAR1_3);
        sSpiDmaChannel->attachInterrupt(objectfled_spi_dma_isr);
    }

    sSpiCurrentPins = pin_info;
    sSpiInitialized = true;
    sSpiDmaComplete = true;
    return true;
#endif  // FL_OBJECTFLED_SPI_HARDWARE_ENABLE
}

// ============================================================================
// Encoder: input bytes -> DMA bit-pattern words
// ============================================================================

static inline void encode_byte(u32* dst, u8 b,
                               u32 other, u32 mosi_mask, u32 sclk_mask) FL_NO_EXCEPT {
    #define ENCODE_BIT(BITN)                                              \
        do {                                                              \
            const u32 mosi_bit = (b & (1u << (BITN))) ? mosi_mask : 0u;   \
            dst[0] = other | mosi_bit;                                    \
            dst[1] = other | mosi_bit | sclk_mask;                        \
            dst += 2;                                                     \
        } while (0)
    ENCODE_BIT(7);
    ENCODE_BIT(6);
    ENCODE_BIT(5);
    ENCODE_BIT(4);
    ENCODE_BIT(3);
    ENCODE_BIT(2);
    ENCODE_BIT(1);
    ENCODE_BIT(0);
    #undef ENCODE_BIT
}

// ============================================================================
// objectfled_spi_show
// ============================================================================

bool objectfled_spi_show(const u8* buffer, u32 num_bytes) FL_NO_EXCEPT {
    if (!sSpiInitialized || !sSpiDmaChannel || !buffer || num_bytes == 0) {
        return false;
    }
    if (num_bytes > kSpiMaxInputBytes) {
        // Per coderabbitai review on #3432: reject oversized frames rather
        // than silently truncating. Silent truncation would emit a partial
        // APA102/SK9822 frame with no caller visibility, corrupting the
        // strip output. The header documents that invalid arguments return
        // false; honor that contract.
        FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: num_bytes=%s exceeds max %s; rejecting",
                            (int)num_bytes, (int)kSpiMaxInputBytes);
        return false;
    }

    // NOTE(#3428): we intentionally do NOT call
    // `ObjectFLEDDmaManager::acquire()` here. That method's
    // `waitForCompletion()` spins on `dma3.complete()`, which returns
    // false forever when the clockless mode has never run and so dma3's
    // major-loop INT flag has never been set. Calling it from SPI mode
    // before any clockless show() either hangs or crashes (verified on
    // Teensy 4.1: the spin path crashes the firmware).
    //
    // For the simple "user alternates clockless and SPI shows" case
    // there is no actual race -- each show() completes before the next
    // is initiated. The race only matters if the user issues SPI and
    // clockless concurrently from different contexts, which FastLED's
    // single-threaded engine doesn't do today.
    //
    // TODO(#3428): proper coordination needs a fix at the manager level
    // (e.g., initialize dma3's INT flag in the constructor so
    // first-time `complete()` returns true) or a separate per-bank
    // coordination primitive that doesn't rely on dma3.

    objectfled_spi_wait();

    sSpiDmaChannel->disable();
    sSpiDmaChannel->clearComplete();
    sSpiDmaChannel->clearInterrupt();
    sSpiDmaChannel->clearError();
    sSpiDmaComplete = true;

    // Snapshot "other" bits from the GPIO1 alias (init remapped GPIO6
    // pads -> GPIO1 so eDMA can reach them).
    const u32 other = GPIO1_DR & ~(sSpiCurrentPins.mosi_mask |
                                   sSpiCurrentPins.sclk_mask);

    u32* dst = sSpiBitPattern;
    for (u32 i = 0; i < num_bytes; ++i) {
        encode_byte(dst, buffer[i], other,
                    sSpiCurrentPins.mosi_mask,
                    sSpiCurrentPins.sclk_mask);
        dst += kSpiWordsPerByte;
    }
    const u32 num_words = num_bytes * kSpiWordsPerByte;

    if ((uintptr_t)sSpiBitPattern >= 0x20200000u) {
        arm_dcache_flush((void*)sSpiBitPattern, num_words * sizeof(u32));
    }
    asm volatile("dsb" ::: "memory");

    sSpiDmaChannel->TCD->SADDR        = sSpiBitPattern;
    sSpiDmaChannel->TCD->SOFF         = 4;
    sSpiDmaChannel->TCD->ATTR         = DMA_TCD_ATTR_SSIZE(2)
                                      | DMA_TCD_ATTR_DSIZE(2);
    sSpiDmaChannel->TCD->NBYTES_MLNO  = 4;
    sSpiDmaChannel->TCD->SLAST        = -(i32)(num_words * 4u);
    // DADDR = GPIO1_DR (eDMA-reachable alias; init remapped via GPR26).
    sSpiDmaChannel->TCD->DADDR        = &GPIO1_DR;
    sSpiDmaChannel->TCD->DOFF         = 0;
    sSpiDmaChannel->TCD->CITER_ELINKNO = num_words;
    sSpiDmaChannel->TCD->BITER_ELINKNO = num_words;
    sSpiDmaChannel->TCD->DLASTSGA     = 0;
    sSpiDmaChannel->TCD->CSR          = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_DREQ;

    sSpiDmaComplete = false;
    sSpiDmaChannel->enable();

    // Start QTimer3 channel 0 -- its periodic compare events drive the
    // DMA via XBAR routing set up in init().
    TMR3_CNTR0 = 0;
    TMR3_ENBL |= 1u;
    asm volatile("dsb" ::: "memory");

    return true;
}

// ============================================================================
// objectfled_spi_wait
// ============================================================================

void objectfled_spi_wait() FL_NO_EXCEPT {
    if (sSpiDmaComplete) {
        return;
    }

    const u32 start = millis();
    const u32 timeout_ms = 50;
    while (!sSpiDmaComplete) {
        if ((u32)(millis() - start) >= timeout_ms) {
            // Snapshot register state BEFORE recovery clears it.
            sSpiLastDiag = ObjectFLEDSPIDiagnostics{};
            sSpiLastDiag.initialized = sSpiInitialized;
            sSpiLastDiag.dma_complete = sSpiDmaComplete;
            if (sSpiDmaChannel) {
                sSpiLastDiag.dma_channel = sSpiDmaChannel->channel;
                sSpiLastDiag.dma_erq = DMA_ERQ;
                sSpiLastDiag.dma_int = DMA_INT;
                sSpiLastDiag.dma_err = DMA_ERR;
                sSpiLastDiag.dma_es  = DMA_ES;
                if (sSpiDmaChannel->TCD) {
                    sSpiLastDiag.dma_citer    = sSpiDmaChannel->TCD->CITER_ELINKNO;
                    sSpiLastDiag.dma_biter    = sSpiDmaChannel->TCD->BITER_ELINKNO;
                    sSpiLastDiag.dma_dlastsga = (u32)sSpiDmaChannel->TCD->DLASTSGA;
                }
            }
            sSpiLastDiag.tmr3_cntr0   = TMR3_CNTR0;
            sSpiLastDiag.tmr3_csctrl0 = TMR3_CSCTRL0;
            sSpiLastDiag.tmr3_sctrl0  = TMR3_SCTRL0;
            sSpiLastDiag.tmr3_ctrl0   = TMR3_CTRL0;
            sSpiLastDiag.tmr3_enbl    = TMR3_ENBL;
            sSpiLastDiag.xbar1_ctrl1  = XBARA1_CTRL1;

            if (sSpiDmaChannel) {
                sSpiDmaChannel->disable();
                sSpiDmaChannel->clearComplete();
                sSpiDmaChannel->clearError();
            }
            TMR3_ENBL &= ~1u;  // stop QTimer3 channel 0
            sSpiDmaComplete = true;
            FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: wait timed out after %s ms -- recovering",
                                (int)timeout_ms);
            break;
        }
    }

    // Brief drain wait so the last DMA word has time to land on the wire.
    // 5 ms covers ~5000 single-byte transfers at the slowest 1 MHz config.
    const u32 drain_start = millis();
    const u32 drain_timeout_ms = 5;
    while ((u32)(millis() - drain_start) < drain_timeout_ms) {
        // Just bounded-spin; we have no timer-status proxy that maps
        // cleanly to "shifter drained" in the QTimer-driven path.
    }

    // Stop QTimer3 channel 0 between transfers.
    TMR3_ENBL &= ~1u;

    // Park SCLK + MOSI low on the GPIO1 alias (init remapped GPIO6 -> GPIO1).
    GPIO1_DR = (GPIO1_DR & ~(sSpiCurrentPins.mosi_mask | sSpiCurrentPins.sclk_mask));

    // NOTE(#3428): paired with the show() acquire-skip above. The manager
    // was never acquired (see comment in objectfled_spi_show), so no
    // release is needed.
}

// ============================================================================
// objectfled_spi_deinit
// ============================================================================

void objectfled_spi_deinit() FL_NO_EXCEPT {
    if (!sSpiInitialized) return;

    objectfled_spi_wait();

    // Stop + clear QTimer3 channel 0.
    TMR3_ENBL    &= ~1u;
    TMR3_CSCTRL0  = 0;
    TMR3_CTRL0    = 0;
    TMR3_SCTRL0   = 0;
    // NOTE: XBAR routing is left as configured -- another driver may share
    // CTRL1 slot 0 (req94) and we don't want to clobber its bits. The
    // routing is harmless when our DMA channel is disabled.

    if (sSpiDmaChannel) {
        sSpiDmaChannel->disable();
        sSpiDmaChannel->detachInterrupt();
        sSpiDmaChannel->clearInterrupt();
        DMAChannel* to_delete = sSpiDmaChannel;
        sSpiDmaChannel = nullptr;
        delete to_delete;  // ok bare allocation
    }

    // Per coderabbitai review on #3432: restore the saved mux ALT values
    // captured in objectfled_spi_init() rather than forcing ALT5. If the
    // user previously had the pin on (e.g.) ALT3 for FlexCAN, forcing ALT5
    // would leave them with a silently-changed pin mux post-deinit.
    if (sSpiCurrentPins.mosi_mux_reg) {
        GPIO1_DR   &= ~sSpiCurrentPins.mosi_mask;
        GPIO1_GDIR &= ~sSpiCurrentPins.mosi_mask;
        *(sSpiCurrentPins.mosi_mux_reg) = sSpiSavedMosiMux;
    }
    if (sSpiCurrentPins.sclk_mux_reg) {
        GPIO1_DR   &= ~sSpiCurrentPins.sclk_mask;
        GPIO1_GDIR &= ~sSpiCurrentPins.sclk_mask;
        *(sSpiCurrentPins.sclk_mux_reg) = sSpiSavedSclkMux;
    }
    // NOTE: we deliberately do NOT restore IOMUXC_GPR_GPR26 -- leaving
    // the pads aliased to GPIO1 is harmless when not driven, and avoids
    // a race with any other driver that may have remapped the same bits.

    sSpiInitialized   = false;
    sSpiDmaComplete   = true;
    sSpiCurrentPins   = ObjectFLEDSPIPinInfo{};
    sSpiSavedMosiMux  = 0;
    sSpiSavedSclkMux  = 0;
}

// ============================================================================
// objectfled_spi_read_diagnostics
// ============================================================================

void objectfled_spi_read_diagnostics(ObjectFLEDSPIDiagnostics* out) FL_NO_EXCEPT {
    if (!out) return;

    // If wait() captured a snapshot during its timeout-recovery path,
    // prefer that (pre-cleanup register state is much more diagnostic
    // than post-cleanup).
    if (sSpiLastDiag.tmr3_enbl != 0 || sSpiLastDiag.dma_erq != 0 ||
        sSpiLastDiag.dma_int != 0  || sSpiLastDiag.dma_err != 0) {
        *out = sSpiLastDiag;
        return;
    }

    *out = ObjectFLEDSPIDiagnostics{};
    out->initialized = sSpiInitialized;
    out->dma_complete = sSpiDmaComplete;

    if (sSpiDmaChannel) {
        out->dma_channel = sSpiDmaChannel->channel;
        // Per-channel bit in ERQ/INT/ERR is (1 << channel).
        out->dma_erq = DMA_ERQ;
        out->dma_int = DMA_INT;
        out->dma_err = DMA_ERR;
        out->dma_es  = DMA_ES;
        if (sSpiDmaChannel->TCD) {
            out->dma_citer    = sSpiDmaChannel->TCD->CITER_ELINKNO;
            out->dma_biter    = sSpiDmaChannel->TCD->BITER_ELINKNO;
            out->dma_dlastsga = (u32)sSpiDmaChannel->TCD->DLASTSGA;
        }
    }

    out->tmr3_cntr0   = TMR3_CNTR0;
    out->tmr3_csctrl0 = TMR3_CSCTRL0;
    out->tmr3_sctrl0  = TMR3_SCTRL0;
    out->tmr3_ctrl0   = TMR3_CTRL0;
    out->tmr3_enbl    = TMR3_ENBL;
    out->xbar1_ctrl1  = XBARA1_CTRL1;
}

}  // namespace fl

#endif  // FL_IS_TEENSY_4X

#endif  // CHANNEL_ENGINE_OBJECTFLED_SPI_MODE_CPP_HPP_
