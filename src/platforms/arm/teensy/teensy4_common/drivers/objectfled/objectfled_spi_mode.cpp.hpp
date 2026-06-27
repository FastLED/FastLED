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
static void objectfled_spi_program_flexpwm(u32 req_hz) FL_NO_EXCEPT {
    FLEXPWM2_OUTEN  &= ~(FLEXPWM_OUTEN_PWMA_EN(1) |
                         FLEXPWM_OUTEN_PWMB_EN(1) |
                         FLEXPWM_OUTEN_PWMX_EN(1));
    FLEXPWM2_MCTRL  |= FLEXPWM_MCTRL_CLDOK(1);
    FLEXPWM2_SM0CTRL2 = FLEXPWM_SMCTRL2_INDEP
                      | FLEXPWM_SMCTRL2_INIT_SEL(0)
                      | FLEXPWM_SMCTRL2_CLK_SEL(0);
    FLEXPWM2_SM0CTRL  = FLEXPWM_SMCTRL_FULL
                      | FLEXPWM_SMCTRL_PRSC(0);

    u32 ipg_hz = (u32)F_BUS_ACTUAL;
    if (ipg_hz == 0) ipg_hz = 150000000u;
    u32 period = ipg_hz / req_hz;
    if (period < 4u)     period = 4u;
    if (period > 0xFFFFu) period = 0xFFFFu;

    FLEXPWM2_SM0INIT = 0;
    FLEXPWM2_SM0VAL0 = 0;
    FLEXPWM2_SM0VAL1 = (u16)(period - 1u);
    FLEXPWM2_SM0VAL2 = 0;
    FLEXPWM2_SM0VAL3 = (u16)(period / 2u);
    FLEXPWM2_SM0VAL4 = 0;
    FLEXPWM2_SM0VAL5 = (u16)(period / 2u);

    // VAL1 compare -> DMA request. SMDMAEN bit 4 = VALDE per RM 30.4.3.7.
    // TODO(#3428): replace literal with FLEXPWM_SMDMAEN_VALDE once we
    // confirm the core version macro spelling.
    FLEXPWM2_SM0DMAEN = (1u << 4);

    FLEXPWM2_SM0STS = 0xFFFFu;
    FLEXPWM2_MCTRL  &= ~FLEXPWM_MCTRL_CLDOK(1);
    FLEXPWM2_MCTRL  |=  FLEXPWM_MCTRL_LDOK(1);
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
    // Until the hardware is debugged on-bench, gate the init behind a
    // compile-time flag so the architectural scaffolding (Capabilities,
    // BusSupports, canHandle SPI branch, engine routing) can still ship
    // and merge. Define FL_OBJECTFLED_SPI_HARDWARE_ENABLE to re-enable.
#ifndef FL_OBJECTFLED_SPI_HARDWARE_ENABLE
    (void)pin_info;
    (void)clock_hz;
    FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: hardware bring-up incomplete; "
                        "init disabled until #3428 scope debug completes");
    return false;
#else
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

    sSpiSavedMosiMux = *(pin_info.mosi_mux_reg);
    sSpiSavedSclkMux = *(pin_info.sclk_mux_reg);
    (void)sSpiSavedMosiMux;
    (void)sSpiSavedSclkMux;

    *(pin_info.mosi_mux_reg) = 5u;
    *(pin_info.mosi_pad_reg) =
        ((FL_OBJECTFLED_SPI_SPEED & 0x3u) << 6) |
        ((FL_OBJECTFLED_SPI_DSE   & 0x7u) << 3);

    *(pin_info.sclk_mux_reg) = 5u;
    *(pin_info.sclk_pad_reg) =
        ((FL_OBJECTFLED_SPI_SPEED & 0x3u) << 6) |
        ((FL_OBJECTFLED_SPI_DSE   & 0x7u) << 3);

    GPIO6_GDIR |= (pin_info.mosi_mask | pin_info.sclk_mask);
    GPIO6_DR   &= ~(pin_info.mosi_mask | pin_info.sclk_mask);

    objectfled_spi_program_flexpwm(2u * clock_hz);

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
        sSpiDmaChannel->triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXPWM2_WRITE0);
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
        FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: num_bytes=%s exceeds max %s; truncating",
                            (int)num_bytes, (int)kSpiMaxInputBytes);
        num_bytes = kSpiMaxInputBytes;
    }

    auto& mgr = ObjectFLEDDmaManager::getInstance();
    mgr.acquire(static_cast<void*>(&sSpiBitPattern));

    objectfled_spi_wait();

    sSpiDmaChannel->disable();
    sSpiDmaChannel->clearComplete();
    sSpiDmaChannel->clearInterrupt();
    sSpiDmaChannel->clearError();
    sSpiDmaComplete = true;

    const u32 other = GPIO6_DR & ~(sSpiCurrentPins.mosi_mask |
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
    sSpiDmaChannel->TCD->DADDR        = &GPIO6_DR;
    sSpiDmaChannel->TCD->DOFF         = 0;
    sSpiDmaChannel->TCD->CITER_ELINKNO = num_words;
    sSpiDmaChannel->TCD->BITER_ELINKNO = num_words;
    sSpiDmaChannel->TCD->DLASTSGA     = 0;
    sSpiDmaChannel->TCD->CSR          = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_DREQ;

    sSpiDmaComplete = false;
    sSpiDmaChannel->enable();

    FLEXPWM2_SM0STS = 0xFFFFu;
    FLEXPWM2_MCTRL |= FLEXPWM_MCTRL_RUN(1);
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
            if (sSpiDmaChannel) {
                sSpiDmaChannel->disable();
                sSpiDmaChannel->clearComplete();
                sSpiDmaChannel->clearError();
            }
            FLEXPWM2_MCTRL &= ~FLEXPWM_MCTRL_RUN(1);
            sSpiDmaComplete = true;
            FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: wait timed out after %s ms -- recovering",
                                (int)timeout_ms);
            break;
        }
    }

    // TODO(#3428): verify the compare-flag bit on scope.
    const u32 drain_start = millis();
    const u32 drain_timeout_ms = 5;
    while ((FLEXPWM2_SM0STS & FLEXPWM_SMSTS_CMPF(0x02)) == 0) {
        if ((u32)(millis() - drain_start) >= drain_timeout_ms) {
            FL_LOG_OBJECTFLED_F("ObjectFLED_SPI: shifter drain timeout after %s ms",
                                (int)drain_timeout_ms);
            break;
        }
    }
    FLEXPWM2_SM0STS = 0xFFFFu;
    FLEXPWM2_MCTRL &= ~FLEXPWM_MCTRL_RUN(1);

    GPIO6_DR = (GPIO6_DR & ~(sSpiCurrentPins.mosi_mask | sSpiCurrentPins.sclk_mask));

    ObjectFLEDDmaManager::getInstance().release(
        static_cast<void*>(&sSpiBitPattern));
}

// ============================================================================
// objectfled_spi_deinit
// ============================================================================

void objectfled_spi_deinit() FL_NO_EXCEPT {
    if (!sSpiInitialized) return;

    objectfled_spi_wait();

    FLEXPWM2_MCTRL    &= ~FLEXPWM_MCTRL_RUN(1);
    FLEXPWM2_SM0DMAEN  = 0;
    FLEXPWM2_SM0CTRL   = 0;
    FLEXPWM2_SM0CTRL2  = 0;
    FLEXPWM2_SM0STS    = 0xFFFFu;

    if (sSpiDmaChannel) {
        sSpiDmaChannel->disable();
        sSpiDmaChannel->detachInterrupt();
        sSpiDmaChannel->clearInterrupt();
        DMAChannel* to_delete = sSpiDmaChannel;
        sSpiDmaChannel = nullptr;
        delete to_delete;  // ok bare allocation
    }

    if (sSpiCurrentPins.mosi_mux_reg) {
        GPIO6_DR   &= ~sSpiCurrentPins.mosi_mask;
        GPIO6_GDIR &= ~sSpiCurrentPins.mosi_mask;
        *(sSpiCurrentPins.mosi_mux_reg) = 5u;
    }
    if (sSpiCurrentPins.sclk_mux_reg) {
        GPIO6_DR   &= ~sSpiCurrentPins.sclk_mask;
        GPIO6_GDIR &= ~sSpiCurrentPins.sclk_mask;
        *(sSpiCurrentPins.sclk_mux_reg) = 5u;
    }

    sSpiInitialized   = false;
    sSpiDmaComplete   = true;
    sSpiCurrentPins   = ObjectFLEDSPIPinInfo{};
    sSpiSavedMosiMux  = 0;
    sSpiSavedSclkMux  = 0;
}

}  // namespace fl

#endif  // FL_IS_TEENSY_4X

#endif  // CHANNEL_ENGINE_OBJECTFLED_SPI_MODE_CPP_HPP_
