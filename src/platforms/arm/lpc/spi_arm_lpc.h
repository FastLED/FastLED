// IWYU pragma: private

#ifndef __INC_SPI_ARM_LPC_H
#define __INC_SPI_ARM_LPC_H

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fastspi_types.h"
#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

// Scoped to LPC8xx (LPC845 / LPC804). The SPI register layout below is the
// LPC8xx "SPI" peripheral block per NXP UM11029 (LPC84x) / UM11065 (LPC80x)
// chapter "SPI". LPC11xx and LPC15xx have their own SPI controllers with
// different register layouts (UM10398 / UM10462 SSP for LPC11; LPC15 SPI is
// closer to LPC8xx but uses different base addresses) — driver wiring for
// those families is tracked in #2845 Stage 4.
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804)

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {

/// @file spi_arm_lpc.h
/// @brief NXP LPC8xx hardware SPI driver — APA102 / SK9822 / WS2801 strip
///        support via SPI master peripheral. Closes #2845 Stage 4 item 3.
///
/// Register layout per UM11029 section "SPI". Base addresses:
///   LPC845 SPI0 @ 0x40058000
///   LPC845 SPI1 @ 0x4005C000
///   LPC804 SPI0 @ 0x40058000  (single SPI on LPC804)
///
/// Default targets SPI0; users can route to SPI1 by passing the
/// corresponding base address as the `pSPIX` template parameter.
///
/// Hardware verification gated on #2845 Stage 3 (no LPC845 board has run
/// FastLED-output LEDs yet). Bytes-on-the-wire correctness is implied by
/// adherence to UM11029; AC timing on real silicon needs scope verification.
///
/// **Header-only by necessity.** `ARMHardwareSPIOutput` is a class template
/// whose definitions must be visible at every instantiation point. The
/// repo's `*.impl.hpp` router pattern (cf. `watchdog.impl.cpp.hpp`) cannot
/// host template definitions — only `.impl.cpp.hpp` files may include
/// `.impl.hpp`, and those are non-template compile units. This matches the
/// other template SPI drivers in `src/platforms/arm/*/spi_*.h`. The
/// CodeRabbit suggestion to split into `.impl.hpp` (PR #2872 review,
/// tracked at #2875) does not apply for template-heavy headers.

// Use the vendor CMSIS PAL SPI_Type from <LPC845.h> / <LPC804.h> (brought
// in via led_sysdefs_arm_lpc.h). SPI0/SPI1 pointer macros and SPI0_BASE/
// SPI1_BASE come from the vendor header. FL_LPC_SPI0_BASE / FL_LPC_SPI1_BASE
// kept as aliases for backward compat with users that pass the base address
// as the pSPIX template parameter.
#ifndef FL_LPC_SPI0_BASE
#define FL_LPC_SPI0_BASE SPI0_BASE
#endif
#ifndef FL_LPC_SPI1_BASE
#define FL_LPC_SPI1_BASE SPI1_BASE
#endif

// SPI register bit definitions (UM11029)
#define FL_LPC_SPI_CFG_ENABLE         (1u << 0)
#define FL_LPC_SPI_CFG_MASTER         (1u << 2)
#define FL_LPC_SPI_CFG_LSBF           (1u << 3)   // 0 = MSB first
#define FL_LPC_SPI_CFG_CPHA           (1u << 4)
#define FL_LPC_SPI_CFG_CPOL           (1u << 5)
#define FL_LPC_SPI_CFG_LOOP           (1u << 7)
#define FL_LPC_SPI_CFG_SPOL0          (1u << 8)

#define FL_LPC_SPI_STAT_RXRDY         (1u << 0)
#define FL_LPC_SPI_STAT_TXRDY         (1u << 1)
#define FL_LPC_SPI_STAT_RXOV          (1u << 2)
#define FL_LPC_SPI_STAT_TXUR          (1u << 3)
#define FL_LPC_SPI_STAT_SSA           (1u << 4)
#define FL_LPC_SPI_STAT_SSD           (1u << 5)
#define FL_LPC_SPI_STAT_STALLED       (1u << 6)
#define FL_LPC_SPI_STAT_ENDTRANSFER   (1u << 7)
#define FL_LPC_SPI_STAT_MSTIDLE       (1u << 8)

// TXDATCTL layout: bits [15:0] = data, [19:16] = TXSSEL, [20] = EOT,
// [21] = EOF, [22] = RXIGNORE, [27:24] = LEN (length-1, 0..15 → 1..16 bits)
#define FL_LPC_SPI_TXCTL_LEN_8BIT     (7u << 24)
#define FL_LPC_SPI_TXCTL_EOT          (1u << 20)
#define FL_LPC_SPI_TXCTL_RXIGNORE     (1u << 22)

#define ARM_HARDWARE_SPI

/// @brief LPC8xx hardware SPI output for FastLED clocked strips.
/// @tparam _DATA_PIN  Pin index for MOSI (must be SWM-routed to SPIx_MOSI)
/// @tparam _CLOCK_PIN Pin index for SCK  (must be SWM-routed to SPIx_SCK)
/// @tparam _SPI_CLOCK_DIVIDER Approximate clock divider (host clock / this)
/// @tparam pSPIX      Peripheral base address (FL_LPC_SPI0_BASE or
///                    FL_LPC_SPI1_BASE)
///
/// Pin routing through the LPC8xx Switch Matrix (SWM) is done by the user's
/// `SystemInit` / board setup code — this driver assumes MOSI/SCK have been
/// routed to the requested pins before `init()` is called. Same pattern as
/// the Stage 2a bit-bang clockless driver, which relies on FastPin's GPIO
/// view rather than reconfiguring IOCON.
template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER,
          u32 pSPIX = FL_LPC_SPI0_BASE>
class ARMHardwareSPIOutput {
    Selectable *mPSelect;

    static inline SPI_Type* spi_block() __attribute__((always_inline)) {
        // C-style cast is the canonical embedded-register access pattern
        // (cf. clockless_arm_lpc_plu.h::reg32); the FastLED
        // reinterpret_cast linter explicitly allows C-style casts for MMIO
        // base addresses, where reinterpret_cast is otherwise flagged.
        return (SPI_Type*)(pSPIX);  // MMIO base address
    }

public:
    ARMHardwareSPIOutput() : mPSelect(nullptr) {}
    explicit ARMHardwareSPIOutput(Selectable *pSelect) : mPSelect(pSelect) {}

    void setSelect(Selectable *pSelect) FL_NO_EXCEPT { mPSelect = pSelect; }

    /// Initialize SPI peripheral as master, 8-bit, MSB-first, CPOL=0, CPHA=0.
    /// The user's startup code is responsible for:
    ///   (a) enabling the SPI clock in SYSCON->SYSAHBCLKCTRL[11] / [12]
    ///       (SPI0 / SPI1) on LPC845, or SYSCON->SYSAHBCLKCTRL0[11] on LPC804
    ///   (b) routing MOSI/SCK through SWM to the requested pins
    void init() FL_NO_EXCEPT {
        FastPin<_DATA_PIN>::setOutput();
        FastPin<_CLOCK_PIN>::setOutput();

        SPI_Type *spi = spi_block();

        // Disable before reconfig
        spi->CFG = 0;

        // Clock divider. LPC8xx SPI clock = peripheral clock / (DIV + 1).
        // _SPI_CLOCK_DIVIDER is interpreted as the same / (DIV+1) value,
        // matching the Teensy LC driver convention.
        u32 div = (_SPI_CLOCK_DIVIDER > 1) ? (_SPI_CLOCK_DIVIDER - 1) : 0;
        if (div > 0xFFF) div = 0xFFF;  // 12-bit field
        spi->DIV = div;

        // Master mode, MSB first, mode 0 (CPOL=0, CPHA=0)
        spi->CFG = FL_LPC_SPI_CFG_ENABLE | FL_LPC_SPI_CFG_MASTER;
    }

    void inline select() FL_NO_EXCEPT __attribute__((always_inline)) {
        if (mPSelect != nullptr) { mPSelect->select(); }
    }

    void inline release() FL_NO_EXCEPT __attribute__((always_inline)) {
        waitFully();
        if (mPSelect != nullptr) { mPSelect->release(); }
    }

    void endTransaction() FL_NO_EXCEPT { release(); }

    /// Wait until TX FIFO can accept another byte.
    static void wait() __attribute__((always_inline)) {
        while (!(spi_block()->STAT & FL_LPC_SPI_STAT_TXRDY)) {}
    }

    /// Wait until the master is fully idle (transfer drained).
    void waitFully() FL_NO_EXCEPT {
        while (!(spi_block()->STAT & FL_LPC_SPI_STAT_MSTIDLE)) {}
    }

    /// Per-bit write — not the most efficient but required by the SPI base
    /// API used for sm16716-style strips. APA102 / SK9822 don't exercise
    /// this path (start bit is part of the byte stream).
    template <u8 BIT>
    inline static void writeBit(u8 b) FL_NO_EXCEPT {
        wait();
        SPI_Type *spi = spi_block();
        // OR in RXIGNORE so this transmit doesn't clock data into RXDAT;
        // the driver never drains RXDAT, so otherwise repeated writeBit()
        // calls would accumulate RXOV. Matches writeByte() below.
        spi->TXDATCTL = ((b & (1u << BIT)) ? 1u : 0u) |
                        FL_LPC_SPI_TXCTL_RXIGNORE;
    }

    /// Write a single 8-bit byte. Caller responsible for waiting on prior
    /// fill; this primitive only writes the TX data register.
    static void writeByte(u8 b) __attribute__((always_inline)) {
        wait();
        SPI_Type *spi = spi_block();
        // TXDATCTL with LEN=7 (8-bit transfer) and RX ignored.
        spi->TXDATCTL = static_cast<u32>(b) | FL_LPC_SPI_TXCTL_LEN_8BIT |
                        FL_LPC_SPI_TXCTL_RXIGNORE;
    }

    static void writeWord(u16 w) __attribute__((always_inline)) {
        writeByte(static_cast<u8>(w >> 8));
        writeByte(static_cast<u8>(w & 0xFF));
    }

    /// Write `len` copies of `value` over SPI.
    static void writeBytesValueRaw(u8 value, int len) FL_NO_EXCEPT {
        while (len--) { writeByte(value); }
    }

    /// Bracketed variant — select + write + flush + release.
    void writeBytesValue(u8 value, int len) FL_NO_EXCEPT {
        select();
        writeBytesValueRaw(value, len);
        waitFully();
        release();
    }

    /// Bracketed bulk byte write with optional per-byte adjuster D.
    template <class D>
    void writeBytes(FASTLED_REGISTER u8 *data, int len,
                    void *context = nullptr) FL_NO_EXCEPT {
        u8 *end = data + len;
        select();
        while (data != end) {
            writeByte(D::adjust(*data++));
        }
        D::postBlock(len, context);
        waitFully();
        release();
    }

    void writeBytes(FASTLED_REGISTER u8 *data, int len) {
        writeBytes<DATA_NOP>(data, len);
    }

    /// Bracketed pixel-controller path used for APA102 / SK9822 / WS2801.
    /// Handles the optional FLAG_START_BIT prefix (APA102's 0xFF brightness
    /// byte per LED) via writeBit<0>.
    template <u8 FLAGS, class D, EOrder RGB_ORDER>
    void writePixels(PixelController<RGB_ORDER> pixels,
                     void *context = nullptr) FL_NO_EXCEPT {
        int len = pixels.mLen;

        select();
        while (pixels.has(1)) {
            if (FLAGS & FLAG_START_BIT) {
                writeBit<0>(1);
                writeByte(D::adjust(pixels.loadAndScale0()));
                writeByte(D::adjust(pixels.loadAndScale1()));
                writeByte(D::adjust(pixels.loadAndScale2()));
            } else {
                writeByte(D::adjust(pixels.loadAndScale0()));
                writeByte(D::adjust(pixels.loadAndScale1()));
                writeByte(D::adjust(pixels.loadAndScale2()));
            }

            pixels.advanceData();
            pixels.stepDithering();
        }
        D::postBlock(len, context);
        waitFully();
        release();
    }

    /// Finalize transmission. The MSTIDLE wait in waitFully() already
    /// drains the FIFO; no additional flush is needed.
    static void finalizeTransmission() FL_NO_EXCEPT {}
};

}  // namespace fl

FL_DISABLE_WARNING_POP

#endif  // FL_IS_ARM_LPC_845 || FL_IS_ARM_LPC_804

#endif  // __INC_SPI_ARM_LPC_H
