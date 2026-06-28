// IWYU pragma: private

/// @file flexio_spi_mode.cpp.hpp
/// @brief FlexIO2 SPI-mode hardware programming for i.MX RT1062 (Teensy 4.x).
///
/// Pairs with `flexio_driver.cpp.hpp` (clockless WS2812 mode) -- both files
/// drive the SAME FlexIO2 peripheral (shifter 0 + timer 0), so the two modes
/// are mutually exclusive at runtime. The unified ChannelEngineFlexIO chooses
/// per-channel inside show() based on `ChannelData::isSpi()` (see #3428 and
/// `src/fl/channels/README.md` -> "Rule: Parallel-IO peripherals -- one engine
/// for both clockless and SPI modes").
///
/// SPI configuration:
///   - Mode 0 (CPOL=0, CPHA=0) -- SCLK idle LOW, data sampled on rising edge.
///   - MSB-first, 8-bit byte beats, no inter-byte gap.
///   - Clock clamped to [100 kHz, 25 MHz] -- APA102 spec is 30 MHz max but
///     we keep margin for cable and chipset variance.
///   - DMA-driven via the same eDMA channel pattern as the clockless mode.
///
/// Reference: NXP i.MX RT1060 Reference Manual rev 2, chapter 50 (FlexIO).
/// The shifter/timer programming mirrors Teensyduino's
/// `libraries/FlexIO_t4/src/FlexIOSPI.cpp` (canonical NXP-blessed reference)
/// with the FastLED bounded-timeout + DMA-once-allocate patterns applied.

#ifndef CHANNEL_ENGINE_FLEXIO_SPI_MODE_CPP_HPP_
#define CHANNEL_ENGINE_FLEXIO_SPI_MODE_CPP_HPP_

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_spi_mode.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_internal.h"

#include "fl/log/log.h"
#include "fl/stl/cstring.h"

// IWYU pragma: begin_keep
#include <Arduino.h>
#include <imxrt.h>
#include <DMAChannel.h>
// IWYU pragma: end_keep

// The Teensy framework defines FLEXIO2_* as macros that expand to struct
// member accesses (e.g. IMXRT_FLEXIO2.offset010). We need to #undef them
// so we can define our own register pointers that support array indexing.
// Mirrors the same #undef dance in flexio_driver.cpp.hpp.
#undef FLEXIO2_CTRL
#undef FLEXIO2_SHIFTSTAT
#undef FLEXIO2_SHIFTERR
#undef FLEXIO2_TIMSTAT
#undef FLEXIO2_SHIFTSDEN
#undef FLEXIO2_SHIFTCTL
#undef FLEXIO2_SHIFTCFG
#undef FLEXIO2_SHIFTBUF
#undef FLEXIO2_SHIFTBUFBIS
#undef FLEXIO2_SHIFTBUFBBS
#undef FLEXIO2_TIMCTL
#undef FLEXIO2_TIMCFG
#undef FLEXIO2_TIMCMP

namespace fl {

// ============================================================================
// FlexIO2 Register Access Helpers (duplicated from flexio_driver.cpp.hpp;
// the two TUs are intentionally independent so either mode can compile alone)
// ============================================================================

static constexpr u32 kSpiFLEXIO2_BASE = 0x401B0000;
static constexpr u32 kSpiIOMUXC_BASE  = 0x401F8000;

static volatile u32& SPI_FLEXIO2_CTRL      = *(volatile u32*)(kSpiFLEXIO2_BASE + 0x008);
static volatile u32& SPI_FLEXIO2_SHIFTSTAT = *(volatile u32*)(kSpiFLEXIO2_BASE + 0x010);
static volatile u32& SPI_FLEXIO2_SHIFTERR  = *(volatile u32*)(kSpiFLEXIO2_BASE + 0x014);
static volatile u32& SPI_FLEXIO2_TIMSTAT   = *(volatile u32*)(kSpiFLEXIO2_BASE + 0x018);
static volatile u32& SPI_FLEXIO2_SHIFTSDEN = *(volatile u32*)(kSpiFLEXIO2_BASE + 0x030);

static volatile u32* SPI_FLEXIO2_SHIFTCTL    = (volatile u32*)(kSpiFLEXIO2_BASE + 0x080);
static volatile u32* SPI_FLEXIO2_SHIFTCFG    = (volatile u32*)(kSpiFLEXIO2_BASE + 0x100);
static volatile u32* SPI_FLEXIO2_SHIFTBUF    = (volatile u32*)(kSpiFLEXIO2_BASE + 0x200);
static volatile u32* SPI_FLEXIO2_SHIFTBUFBBS = (volatile u32*)(kSpiFLEXIO2_BASE + 0x380);

static volatile u32* SPI_FLEXIO2_TIMCTL = (volatile u32*)(kSpiFLEXIO2_BASE + 0x400);
static volatile u32* SPI_FLEXIO2_TIMCFG = (volatile u32*)(kSpiFLEXIO2_BASE + 0x480);
static volatile u32* SPI_FLEXIO2_TIMCMP = (volatile u32*)(kSpiFLEXIO2_BASE + 0x500);

// FlexIO2 base clock under the standard Teensy 4.x CCM setup
// (PLL3_PFD3 / pred=2 / podf=2 = 120 MHz). Must match the divisor pair
// programmed by flexio_clock_init() in flexio_driver.cpp.hpp.
static constexpr u32 kFlexIO2BaseClockHz = 120000000u;

// Clock-rate clamp window (see #3428 header doc).
static constexpr u32 kSpiClockMinHz =    100000u;  // 100 kHz floor
static constexpr u32 kSpiClockMaxHz = 25000000u;   // 25 MHz ceiling

// ============================================================================
// Module State
// ============================================================================

static DMAChannel* sSpiDmaChannel = nullptr;
static volatile bool sSpiDmaComplete = true;
static bool sSpiInitialized = false;
static FlexIOSPIPinInfo sSpiCurrentPins{};
// Saved IOMUXC mux values so flexio_spi_deinit() can return the pads to a
// safe ALT5 (GPIO) input without remembering the original alt function the
// user had configured. ALT5 + SION=0 = pin functions as a plain GPIO input.
static u32 sSpiSavedMosiMux = 0;
static u32 sSpiSavedSclkMux = 0;

// ============================================================================
// DMA ISR
// ============================================================================

static void flexio_spi_dma_isr() {
    // Guard: deinit() can delete sSpiDmaChannel while a pending IRQ is still
    // queued at the NVIC. Mirror flexio_driver.cpp.hpp:158 fix.
    if (sSpiDmaChannel != nullptr) {
        sSpiDmaChannel->clearInterrupt();
    }
    // Ensure the clearInterrupt() store reaches eDMA before the ISR returns
    // (Cortex-M7 posted-write barrier). Matches flexio_driver.cpp.hpp DSB.
    asm volatile("dsb" ::: "memory");
    sSpiDmaComplete = true;
}

// ============================================================================
// flexio_spi_lookup_pins
// ============================================================================

bool flexio_spi_lookup_pins(u8 mosi_pin, u8 sclk_pin,
                            FlexIOSPIPinInfo* info) FL_NO_EXCEPT {
    if (!info) return false;
    if (mosi_pin == sclk_pin) return false;  // aliasing not allowed

    FlexIOPinEntry mosi_entry{};
    FlexIOPinEntry sclk_entry{};
    if (!flexio_lookup_pin_entry(mosi_pin, &mosi_entry)) return false;
    if (!flexio_lookup_pin_entry(sclk_pin, &sclk_entry)) return false;

    // Different FlexIO2 pin indices required -- two distinct pads.
    if (mosi_entry.flexio_pin == sclk_entry.flexio_pin) return false;

    info->mosi_pin = mosi_pin;
    info->sclk_pin = sclk_pin;
    info->mosi_flexio_pin = mosi_entry.flexio_pin;
    info->sclk_flexio_pin = sclk_entry.flexio_pin;
    info->mosi_mux_reg = (volatile u32*)(kSpiIOMUXC_BASE + mosi_entry.mux_reg_offset);
    info->mosi_pad_reg = (volatile u32*)(kSpiIOMUXC_BASE + mosi_entry.pad_reg_offset);
    info->sclk_mux_reg = (volatile u32*)(kSpiIOMUXC_BASE + sclk_entry.mux_reg_offset);
    info->sclk_pad_reg = (volatile u32*)(kSpiIOMUXC_BASE + sclk_entry.pad_reg_offset);
    return true;
}

// ============================================================================
// flexio_spi_init
// ============================================================================

// IOMUXC pad tuning for SPI -- higher drive strength + higher slew rate
// than the clockless TX. APA102 strips are typically driven at 1-25 MHz over
// short jumpers; reuse the Teensy FlexIOSPI defaults (DSE=7, SPEED=2).
#ifndef FL_FLEXIO_SPI_DSE
#define FL_FLEXIO_SPI_DSE 7
#endif
#ifndef FL_FLEXIO_SPI_SPEED
#define FL_FLEXIO_SPI_SPEED 2
#endif

bool flexio_spi_init(const FlexIOSPIPinInfo& pin_info,
                     u32 clock_hz) FL_NO_EXCEPT {
    if (clock_hz == 0) {
        FL_LOG_FLEXIO_F("FlexIO_SPI: init refused -- clock_hz == 0");
        return false;
    }
    if (sSpiInitialized) {
        flexio_spi_deinit();
    }

    // Clock-rate clamp.
    if (clock_hz < kSpiClockMinHz) {
        FL_LOG_FLEXIO_F("FlexIO_SPI: clock %u Hz below floor; clamping to %u Hz",
                        (unsigned)clock_hz, (unsigned)kSpiClockMinHz);
        clock_hz = kSpiClockMinHz;
    } else if (clock_hz > kSpiClockMaxHz) {
        FL_LOG_FLEXIO_F("FlexIO_SPI: clock %u Hz above ceiling; clamping to %u Hz",
                        (unsigned)clock_hz, (unsigned)kSpiClockMaxHz);
        clock_hz = kSpiClockMaxHz;
    }

    FL_LOG_FLEXIO_F("FlexIO_SPI: init MOSI=%d(flex %d) SCLK=%d(flex %d) @ %u Hz",
                    (int)pin_info.mosi_pin, (int)pin_info.mosi_flexio_pin,
                    (int)pin_info.sclk_pin, (int)pin_info.sclk_flexio_pin,
                    (unsigned)clock_hz);

    // Bring the CCM gate up if the clockless mode hasn't already done so.
    // flexio_ensure_clock() is idempotent and matches the exact pred/podf
    // dividers the clockless mode uses (so the 120 MHz base assumption
    // holds for either init order).
    flexio_ensure_clock();

    // Software reset, bounded so a stuck SWRST can't hang the boot path.
    SPI_FLEXIO2_CTRL = FLEXIO_CTRL_SWRST;
    {
        const u32 swrst_start = micros();
        while (SPI_FLEXIO2_CTRL & FLEXIO_CTRL_SWRST) {
            if ((u32)(micros() - swrst_start) >= 1000) {
                break;
            }
        }
    }
    SPI_FLEXIO2_CTRL = 0;

    // ------------------------------------------------------------------
    // Pin muxing: ALT4 + SION on both MOSI and SCLK
    // (SION required to route the pin to the FlexIO peripheral; see
    // flexio_pin_init() in flexio_driver.cpp.hpp for the full investigation
    // -- without SION the pad stays gated and no signal appears.)
    // ------------------------------------------------------------------
    sSpiSavedMosiMux = *(pin_info.mosi_mux_reg);
    sSpiSavedSclkMux = *(pin_info.sclk_mux_reg);
    // Used by the DMA-alloc-failure cleanup below to restore pin muxes;
    // a future patch may also restore them in flexio_spi_deinit().

    *(pin_info.mosi_mux_reg) = 4u | 0x10u;  // ALT4 + SION
    *(pin_info.mosi_pad_reg) =
        ((FL_FLEXIO_SPI_DSE   & 0x7u) << 3) |
        ((FL_FLEXIO_SPI_SPEED & 0x3u) << 6);

    *(pin_info.sclk_mux_reg) = 4u | 0x10u;  // ALT4 + SION
    *(pin_info.sclk_pad_reg) =
        ((FL_FLEXIO_SPI_DSE   & 0x7u) << 3) |
        ((FL_FLEXIO_SPI_SPEED & 0x3u) << 6);

    // ------------------------------------------------------------------
    // Baud divider: timer toggles SCLK twice per data bit (one rise + one
    // fall). SCLK frequency = flexio_clk / (2 * (div + 1)).
    // Solve: div = (flexio_clk / (2 * sclk)) - 1.
    // Field is 8 bits so clamp to [0, 255].
    //
    // Ceiling division (not floor) so actual SCLK never EXCEEDS the
    // requested/clamped rate -- with floor, a 25 MHz request becomes
    // div=1 and emits 120/(2*2)=30 MHz, breaking the 25 MHz upper clamp.
    // Per coderabbitai review on PR #3431.
    // ------------------------------------------------------------------
    const u32 div_denom = 2u * clock_hz;
    u32 div_calc = (kFlexIO2BaseClockHz + div_denom - 1u) / div_denom;
    if (div_calc == 0) div_calc = 1;
    u32 baud_div_field = div_calc - 1u;
    if (baud_div_field > 0xFFu) {
        // Clamp -- effective min SCLK ~= 120 MHz / (2 * 256) = 234 kHz.
        // For sub-234 kHz we would need to tweak CCM_CS1CDR dividers,
        // but the engine layer only delivers >= 100 kHz so log + clamp
        // instead of failing.
        // TODO(#3428): expose a CCM tweak path if a real <234 kHz target
        // shows up.
        FL_LOG_FLEXIO_F("FlexIO_SPI: baud div overflow (%u); clamping to 255 -> effective ~234 kHz",
                        (unsigned)baud_div_field);
        baud_div_field = 0xFFu;
    }

    // TIMCMP for 8-bit beats: high byte = (8 * 2) - 1 = 15 transitions,
    // low byte = baud divider. See RM 50.5.1.18 and FlexIOSPI.cpp:248.
    const u32 bit_count_field = (8u * 2u) - 1u;  // = 15

    // ------------------------------------------------------------------
    // Shifter 0: MOSI transmit.
    //   SMOD=2     -> transmit
    //   TIMSEL=0   -> driven by timer 0
    //   TIMPOL=1   -> shift on FALLING edge of shift clock (Mode 0:
    //                 data set on falling edge so it is stable for the
    //                 slave's rising-edge sample). Matches Teensy
    //                 FlexIOSPI.cpp:115 SHIFT_ON_FALLING_EDGE.
    //   PINCFG=3   -> shifter output drives the pin
    //   PINSEL     -> MOSI FlexIO pin index
    //   PINPOL=0   -> active high MOSI
    // TODO(#3428): verify TIMPOL on logic-analyzer loopback -- if data is
    // shifted by half a clock period, flip to SHIFT_ON_RISING_EDGE.
    // ------------------------------------------------------------------
    SPI_FLEXIO2_SHIFTCFG[0] = 0;  // no start/stop bits (raw byte beats)

    SPI_FLEXIO2_SHIFTCTL[0] =
        FLEXIO_SHIFTCTL_MODE_TRANSMIT |
        FLEXIO_SHIFTCTL_SHIFT_ON_FALLING_EDGE |
        FLEXIO_SHIFTCTL_PINMODE_OUTPUT |
        FLEXIO_SHIFTCTL_TIMSEL(0) |
        FLEXIO_SHIFTCTL_PINSEL(pin_info.mosi_flexio_pin);

    // ------------------------------------------------------------------
    // Timer 0: SCLK baud generator.
    //   TIMOD=1                  -> dual 8-bit baud counter
    //   TRIGGER_SHIFTER(0)       -> driven by shifter 0 status flag
    //   TRIGGER_ACTIVE_LOW       -> timer runs while shifter has data
    //                               (SSF=0 = busy), pauses when empty
    //   PINMODE_OUTPUT (PINCFG=3) -> timer drives SCLK pin
    //   PINSEL                    -> SCLK FlexIO pin index
    //   PINPOL=0                  -> SCLK active high (idle low)
    // ------------------------------------------------------------------
    SPI_FLEXIO2_TIMCTL[0] =
        FLEXIO_TIMCTL_MODE_8BIT_BAUD |
        FLEXIO_TIMCTL_TRIGGER_SHIFTER(0) |
        FLEXIO_TIMCTL_TRIGGER_ACTIVE_LOW |
        FLEXIO_TIMCTL_PINMODE_OUTPUT |
        FLEXIO_TIMCTL_PINSEL(pin_info.sclk_flexio_pin);

    // ------------------------------------------------------------------
    // Timer 0 config:
    //   TIMOUT=1 (OUTPUT_LOW_WHEN_ENABLED) -- SCLK idles LOW (Mode 0
    //                                          CPOL=0). NOTE: TIMOUT
    //                                          encoding is INVERTED from
    //                                          what the field name might
    //                                          suggest -- TIMOUT(1) = LOW,
    //                                          TIMOUT(0) = HIGH. Verified
    //                                          against imxrt.h:4146-4147
    //                                          and RM 50.5.1.21.
    //   TIMDEC=0 (DEC_ON_CLOCK_SHIFT_ON_OUTPUT) -- standard baud mode
    //   TIMDIS=2 (DISABLE_ON_8BIT_MATCH)        -- stop when bit count
    //                                              reaches 0
    //   TIMENA=2 (ENABLE_WHEN_TRIGGER_HIGH)     -- start when shifter
    //                                              loaded (note: TRGPOL
    //                                              inverts the sense of
    //                                              "high" -- effective
    //                                              start is on SSF=LOW)
    // ------------------------------------------------------------------
    SPI_FLEXIO2_TIMCFG[0] =
        FLEXIO_TIMCFG_DEC_ON_CLOCK_SHIFT_ON_OUTPUT |
        FLEXIO_TIMCFG_OUTPUT_LOW_WHEN_ENABLED |
        FLEXIO_TIMCFG_DISABLE_ON_8BIT_MATCH |
        FLEXIO_TIMCFG_ENABLE_WHEN_TRIGGER_HIGH;

    SPI_FLEXIO2_TIMCMP[0] = (bit_count_field << 8) | (baud_div_field & 0xFFu);

    // Clear any latched status from a previous mode.
    SPI_FLEXIO2_SHIFTSTAT = 0xFFu;
    SPI_FLEXIO2_SHIFTERR  = 0xFFu;
    SPI_FLEXIO2_TIMSTAT   = 0xFFu;

    // DMA channel allocate-once. SHIFTSDEN stays OFF until show() so a
    // pre-armed channel cannot fire on a stale half-programmed TCD.
    if (sSpiDmaChannel == nullptr) {
        sSpiDmaChannel = new DMAChannel();  // ok bare allocation -- one-shot
        if (sSpiDmaChannel == nullptr) {
            FL_LOG_FLEXIO_F("FlexIO_SPI: failed to allocate DMA channel");
            // Restore pin muxes -- without this, MOSI/SCLK stay stolen by
            // the failed init (we already switched them to ALT4|SION above)
            // and any subsequent re-init at different pins would dangle.
            // Per coderabbitai review on PR #3431.
            SPI_FLEXIO2_CTRL = 0;
            *(pin_info.mosi_mux_reg) = sSpiSavedMosiMux;
            *(pin_info.sclk_mux_reg) = sSpiSavedSclkMux;
            return false;
        }
        sSpiDmaChannel->triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO2_REQUEST0);
        sSpiDmaChannel->attachInterrupt(flexio_spi_dma_isr);
    }

    // Save pin info for deinit pad-restore.
    sSpiCurrentPins = pin_info;

    // Enable FlexIO. FASTACC stays OFF -- it requires FlexIO clock >= 2x
    // bus clock, but on Teensy 4.x the bus is 600 MHz and FlexIO is
    // 120 MHz, so FASTACC=1 would corrupt reads. See flexio_driver.cpp.hpp.
    SPI_FLEXIO2_CTRL = FLEXIO_CTRL_FLEXEN;

    sSpiInitialized = true;
    sSpiDmaComplete = true;
    return true;
}

// ============================================================================
// flexio_spi_show
// ============================================================================

bool flexio_spi_show(const u8* buffer, u32 num_bytes) FL_NO_EXCEPT {
    if (!sSpiInitialized || !sSpiDmaChannel || !buffer || num_bytes == 0) {
        return false;
    }

    // Drain any prior transfer.
    flexio_spi_wait();

    // Quiesce: hard-disable DMA + clear flags before reprogramming the TCD.
    // flexio_spi_wait() can return early via its 50 ms timeout; if it does,
    // DMA is still live and modifying the TCD mid-transfer is UB on
    // i.MX RT eDMA. Mirrors flexio_show() in flexio_driver.cpp.hpp.
    sSpiDmaChannel->disable();
    sSpiDmaChannel->clearComplete();
    sSpiDmaChannel->clearInterrupt();
    sSpiDmaChannel->clearError();
    sSpiDmaComplete = true;

    // Disarm the FlexIO->DMA request line while we re-program the TCD.
    SPI_FLEXIO2_SHIFTSDEN = 0;

    // Reset shifter status so the first SSF transition after FLEXEN cycles
    // the trigger cleanly into the timer.
    SPI_FLEXIO2_SHIFTSTAT = 0xFFu;
    SPI_FLEXIO2_SHIFTERR  = 0xFFu;
    SPI_FLEXIO2_TIMSTAT   = 0xFFu;

    // Cache coherence: if the caller's buffer lives in cached memory
    // (OCRAM2 / ITCM / user heap), flush it so the eDMA reads the
    // freshly-written bytes and not a stale cache line. Matches the
    // FlexIOSPI library pattern (FlexIOSPI.cpp:518) -- only flush when
    // the address is >= 0x20200000 (i.e. cached OCRAM2 / external).
    if ((uintptr_t)buffer >= 0x20200000u) {
        arm_dcache_flush((void*)buffer, num_bytes);
    }
    asm volatile("dsb" ::: "memory");

    // ------------------------------------------------------------------
    // Configure the eDMA TCD: 1-byte beats from user buffer to
    // SHIFTBUFBBS[0] (byte+bit-swap alias). Writing single bytes into
    // SHIFTBUFBBS streams them out MSB-first, which is what APA102 /
    // SK9822 / Mode 0 SPI expects.
    //
    // RM 50.5.1.30 SHIFTBUFBBS: "Bit & Byte Swap" -- the shifter loads
    // from this alias with both byte order and bit order reversed
    // relative to SHIFTBUF, so an LSB-first internal shifter clocked
    // from this address emits the original byte stream MSB-first.
    // (Cross-checked against Teensy FlexIOSPI.cpp:178 where the same
    // alias is the canonical TX target for MSBFIRST mode.)
    // TODO(#3428): scope MOSI for first byte 0xA5 -> expect 1010 0101
    // starting from bit7; if it comes out reversed, switch to
    // SHIFTBUFBIS and adjust SOFF/SSIZE.
    // ------------------------------------------------------------------
    sSpiDmaChannel->TCD->SADDR = buffer;
    sSpiDmaChannel->TCD->SOFF  = 1;
    sSpiDmaChannel->TCD->ATTR  = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);
    sSpiDmaChannel->TCD->NBYTES_MLNO = 1;
    sSpiDmaChannel->TCD->SLAST = -(i32)num_bytes;
    sSpiDmaChannel->TCD->DADDR = &SPI_FLEXIO2_SHIFTBUFBBS[0];
    sSpiDmaChannel->TCD->DOFF  = 0;
    sSpiDmaChannel->TCD->CITER_ELINKNO = num_bytes;
    sSpiDmaChannel->TCD->BITER_ELINKNO = num_bytes;
    sSpiDmaChannel->TCD->DLASTSGA = 0;
    sSpiDmaChannel->TCD->CSR = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_DREQ;

    sSpiDmaComplete = false;

    // Arm the channel BEFORE enabling the FlexIO->DMA request line, so the
    // first DMA request lands on a fully-armed TCD (same FX-CRIT-2
    // ordering pattern as flexio_show()).
    sSpiDmaChannel->enable();
    SPI_FLEXIO2_SHIFTSDEN = (1u << 0);  // shifter-0-empty -> DMA request

    return true;
}

// ============================================================================
// flexio_spi_wait
// ============================================================================

void flexio_spi_wait() FL_NO_EXCEPT {
    if (sSpiDmaComplete) return;

    // Bounded 50 ms wait for the DMA major-loop completion.
    // 25 MHz SCLK * 8 bits/byte means worst-case 320 ns/byte, so even a
    // 4096-byte frame finishes in ~1.3 ms -- 50 ms is far more headroom
    // than any plausible APA102 frame needs.
    const u32 start = millis();
    const u32 timeout_ms = 50;
    while (!sSpiDmaComplete) {
        if ((u32)(millis() - start) >= timeout_ms) {
            // Force-recover so the next show() is not blocked forever.
            // Matches the flexio_wait() FX-MED-4 recovery pattern.
            if (sSpiDmaChannel) {
                sSpiDmaChannel->disable();
                sSpiDmaChannel->clearComplete();
                sSpiDmaChannel->clearError();
            }
            SPI_FLEXIO2_SHIFTSDEN = 0;
            sSpiDmaComplete = true;
            FL_LOG_FLEXIO_F("FlexIO_SPI: wait timed out after %u ms -- recovering",
                            (unsigned)timeout_ms);
            return;
        }
    }

    // DMA major-loop complete, but the shifter may still be clocking out
    // the last byte. Wait for TIMSTAT[0] (timer 0 reached compare) to
    // confirm the wire has actually drained. Bounded 5 ms -- one
    // 8-bit beat at the slowest supported 234 kHz takes ~34 us, so 5 ms
    // = ~146 byte-times of slack.
    const u32 drain_start = millis();
    const u32 drain_timeout_ms = 5;
    while (!(SPI_FLEXIO2_TIMSTAT & 0x1u)) {
        if ((u32)(millis() - drain_start) >= drain_timeout_ms) {
            FL_LOG_FLEXIO_F("FlexIO_SPI: shifter drain timeout after %u ms",
                            (unsigned)drain_timeout_ms);
            break;
        }
    }
    SPI_FLEXIO2_TIMSTAT = 0xFFu;  // W1C the timer status latch
}

// ============================================================================
// flexio_spi_deinit
// ============================================================================

void flexio_spi_deinit() FL_NO_EXCEPT {
    if (!sSpiInitialized) return;

    flexio_spi_wait();

    // Shut down FlexIO peripheral BEFORE touching DMA so no more
    // shifter-empty events can fire while we tear the channel down.
    SPI_FLEXIO2_CTRL = 0;
    SPI_FLEXIO2_SHIFTSDEN = 0;
    SPI_FLEXIO2_SHIFTCTL[0] = 0;
    SPI_FLEXIO2_SHIFTCFG[0] = 0;
    SPI_FLEXIO2_TIMCTL[0]   = 0;
    SPI_FLEXIO2_TIMCFG[0]   = 0;
    SPI_FLEXIO2_TIMCMP[0]   = 0;

    if (sSpiDmaChannel) {
        // Order: disable ERQ -> detach interrupt -> clear pending NVIC IRQ
        // -> mark pointer null -> delete. Without the detach + IRQ clear,
        // a pending interrupt vector dereferences a deleted channel
        // (same #3410 IMPRECISERR class fault we hit in clockless deinit).
        sSpiDmaChannel->disable();
        sSpiDmaChannel->detachInterrupt();
        sSpiDmaChannel->clearInterrupt();
        DMAChannel* to_delete = sSpiDmaChannel;
        sSpiDmaChannel = nullptr;
        delete to_delete;  // ok bare allocation
    }

    // Restore pad mux to ALT5 (GPIO) input. Drop SION so the pad is fully
    // released from FlexIO; the caller can re-pinMode() if they want to
    // reuse the pin for something else after deinit.
    if (sSpiCurrentPins.mosi_mux_reg) {
        *(sSpiCurrentPins.mosi_mux_reg) = 5u;
    }
    if (sSpiCurrentPins.sclk_mux_reg) {
        *(sSpiCurrentPins.sclk_mux_reg) = 5u;
    }

    sSpiInitialized = false;
    sSpiDmaComplete = true;
    sSpiCurrentPins = FlexIOSPIPinInfo{};
}

}  // namespace fl

#endif  // FL_IS_TEENSY_4X

#endif  // CHANNEL_ENGINE_FLEXIO_SPI_MODE_CPP_HPP_
