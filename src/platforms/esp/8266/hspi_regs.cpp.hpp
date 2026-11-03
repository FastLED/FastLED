// IWYU pragma: private

/// @file platforms/esp/8266/hspi_regs.cpp.hpp
/// @brief Definitions for hspi_regs.h: the single owner of the ESP8266 SPI1
/// (HSPI) register sequence. Register fixes land here, once.

#include "platforms/esp/8266/hspi_regs.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266)

// IWYU pragma: begin_keep
#include <esp8266_peri.h>  // SDK SPI1/GPIO register map
// IWYU pragma: end_keep

namespace fl {
namespace esp8266 {

/// Block until the SPI1 controller is idle.
void hspiWaitIdle() FL_NO_EXCEPT {
    while (SPI1CMD & SPIBUSY) {}
}

/// Switch GPIO13/14 to the HSPI alternate function and program SPI1 for
/// byte-oriented MOSI transfers at @p clk (mirrors the Arduino core's
/// `pinMode(pin, SPECIAL)` + `setClockDivider()` + `setDataBits()`).
void hspiConfigure(HspiClock clk) FL_NO_EXCEPT {
    // Pad setup: clear the GPIO output enable so the peripheral drives the
    // pads, normal drive strength, then select the HSPI function.
    GPC(13) = (GPC(13) & (0xFUL << GPCI));
    GPC(14) = (GPC(14) & (0xFUL << GPCI));
    GPEC = (1UL << 13) | (1UL << 14);
    GPF13 = GPFFS(GPFFS_BUS(13));
    GPF14 = GPFFS(GPFFS_BUS(14));

    hspiWaitIdle();
    SPI1C = 0;  // MSB-first, no delay chains
    SPI1C1 = 0;
    // SPICLK_EQU_SYSCLK is a GPMUX mux bit (bit 9) as much as SPI1CLK bit 31,
    // so select the divider fields or the system-clock bypass the way the
    // Arduino core's setClockDivider() does. Writing the bypass value with the
    // mux bit clear leaves every divider field at zero -- no clock on the pads.
    if (clk.reg_value == kHspiEquSysClk) {
        GPMUX |= (1UL << 9);
    } else {
        GPMUX &= ~(1UL << 9);
    }
    SPI1CLK = clk.reg_value;
    SPI1U = SPIUMOSI | SPIUDUPLEX | SPIUSSE;
    // 8-bit phase; MISO is unused but kept in step with MOSI.
    SPI1U1 = (static_cast<u32>(7) << SPILMOSI) | (static_cast<u32>(7) << SPILMISO);
}

/// Return GPIO13/14 to plain GPIO inputs (mirrors the core's SPI end()).
void hspiRelease() FL_NO_EXCEPT {
    hspiWaitIdle();
    GPF13 = GPFFS(GPFFS_GPIO(13));
    GPF14 = GPFFS(GPFFS_GPIO(14));
    GPEC = (1UL << 13) | (1UL << 14);
}

/// Program the transfer length. MOSI and MISO lengths stay equal.
void hspiSetTransferBits(u32 nbits) FL_NO_EXCEPT {
    const u32 bits = nbits - 1;
    const u32 mask = (static_cast<u32>(SPIMMOSI) << SPILMOSI) |
                     (static_cast<u32>(SPIMMISO) << SPILMISO);
    SPI1U1 = (SPI1U1 & ~mask) | (bits << SPILMOSI) | (bits << SPILMISO);
}

/// Start the programmed transfer and wait for it to finish. The spin is
/// bounded by at most 512 bits at the configured rate, so no OS yield.
void hspiKick() FL_NO_EXCEPT {
    __sync_synchronize();
    SPI1CMD |= SPIBUSY;
    hspiWaitIdle();
}

/// Shift out a single bit (MSB-first): a one-bit transfer emits bit 7 of
/// word byte 0.
void hspiWriteBit(bool bit) FL_NO_EXCEPT {
    hspiWaitIdle();
    hspiSetTransferBits(1);
    SPI1W0 = bit ? 0x80UL : 0UL;
    hspiKick();
}

/// Stage up to kHspiMaxBatchBytes into the FIFO and shift them out, blocking
/// until done.
void hspiWriteChunk(const u8* data, u16 nbytes) FL_NO_EXCEPT {
    hspiWaitIdle();
    hspiSetTransferBits(static_cast<u32>(nbytes) * 8);

    // SPI1W0..15 are contiguous; word byte 0 is emitted first, so pack the
    // bytes little-endian into each FIFO word.
    volatile u32* fifo = &SPI1W0;
    const u16 nwords = static_cast<u16>((nbytes + 3) / 4);
    for (u16 i = 0; i < nwords; ++i) {
        const u16 b = static_cast<u16>(i * 4);
        u32 word = data[b];
        if (b + 1 < nbytes) word |= static_cast<u32>(data[b + 1]) << 8;
        if (b + 2 < nbytes) word |= static_cast<u32>(data[b + 2]) << 16;
        if (b + 3 < nbytes) word |= static_cast<u32>(data[b + 3]) << 24;
        fifo[i] = word;
    }
    hspiKick();
}

}  // namespace esp8266
}  // namespace fl

#endif  // FL_IS_ESP8266
