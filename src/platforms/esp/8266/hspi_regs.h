#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/hspi_regs.h
/// @brief The single owner of the ESP8266 SPI1 (HSPI) register sequence.
///
/// Both the legacy SPIOutput path (`ESP8266HSPIOutput`) and the Bus::SPI
/// channel driver (`Esp8266HspiChannelDriver`) program the peripheral only
/// through these primitives. Fix register behavior here, once -- do not grow
/// a second copy of the pad mux / GPMUX / SPI1U / FIFO sequence elsewhere.
/// Definitions live in hspi_regs.cpp.hpp; clock math in spi_hspi_clock.h.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/8266/spi_hspi_clock.h"

namespace fl {
namespace esp8266 {

/// SPI1W0..15 = 16 words = 512 bits = 64 bytes per hardware transaction.
constexpr u16 kHspiMaxBatchBytes = 64;

/// Block until the SPI1 controller is idle.
void hspiWaitIdle() FL_NO_EXCEPT;

/// Switch GPIO13/14 to the HSPI alternate function and program SPI1 for
/// byte-oriented MOSI transfers at @p clk (mirrors the Arduino core's
/// `pinMode(pin, SPECIAL)` + `setClockDivider()` + `setDataBits()`).
void hspiConfigure(HspiClock clk) FL_NO_EXCEPT;

/// Return GPIO13/14 to plain GPIO inputs (mirrors the core's SPI end()).
void hspiRelease() FL_NO_EXCEPT;

/// Program the transfer length. MOSI and MISO lengths stay equal.
void hspiSetTransferBits(u32 nbits) FL_NO_EXCEPT;

/// Start the programmed transfer and wait for it to finish. The spin is
/// bounded by at most 512 bits at the configured rate, so no OS yield.
void hspiKick() FL_NO_EXCEPT;

/// Shift out a single bit (MSB-first): a one-bit transfer emits bit 7 of
/// word byte 0.
void hspiWriteBit(bool bit) FL_NO_EXCEPT;

/// Stage up to kHspiMaxBatchBytes into the FIFO and shift them out, blocking
/// until done.
void hspiWriteChunk(const u8* data, u16 nbytes) FL_NO_EXCEPT;

}  // namespace esp8266
}  // namespace fl
