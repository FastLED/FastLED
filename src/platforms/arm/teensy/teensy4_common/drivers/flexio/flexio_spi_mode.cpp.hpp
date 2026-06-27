// IWYU pragma: private

/// @file flexio_spi_mode.cpp.hpp
/// @brief FlexIO2 SPI-mode hardware helpers -- SCAFFOLDING STUB
///
/// Architectural scaffolding for the unified FlexIO clockless+SPI engine
/// per the rule established in PR #3430 (see
/// `src/fl/channels/README.md` -> "Rule: Parallel-IO peripherals -- one engine
/// for both clockless and SPI modes" and #3428).
///
/// The engine treats FlexIO2 as ONE peripheral that can run in either mode,
/// switching modes per-channel inside `show()`. This file is the SPI-side
/// hardware shim (`flexio_driver.cpp.hpp` is the clockless-side shim).
///
/// **Hardware programming is intentionally not implemented yet.** All
/// functions return / behave as if the SPI hardware is unavailable so
/// `ChannelEngineFlexIO::canHandle()` rejects SPI channels and they fall
/// through to the next bus (e.g. LPSPI) via priority dispatch. A follow-up
/// PR (#3428) replaces these stubs with the real FlexIO2 SPI master mode
/// register programming (shifter 0 = MOSI transmit, timer 0 = SCLK baud
/// generator, Mode 0 / MSB-first / 8-bit beats, DMA-driven).
///
/// Why land the stubs and the engine routing now (without the hardware):
///   - Establishes the unified-engine architectural pattern in code so the
///     `Bus::FLEX_IO` -> SpiChipsetConfig type relationship is well-formed.
///   - Lets reviewers see one ChannelEngine handling both modes, not a fork.
///   - Compile-time-only changes -- runtime behavior is unchanged for any
///     existing user (clockless path is untouched, SPI path was never
///     served by FlexIO before this PR either).

#ifndef CHANNEL_ENGINE_FLEXIO_SPI_MODE_CPP_HPP_
#define CHANNEL_ENGINE_FLEXIO_SPI_MODE_CPP_HPP_

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_spi_mode.h"

namespace fl {

bool flexio_spi_lookup_pins(u8 /*mosi_pin*/, u8 /*sclk_pin*/,
                            FlexIOSPIPinInfo* /*info*/) FL_NO_EXCEPT {
    // TODO(#3428): table of FlexIO2-routable (MOSI, SCLK) pin pairs, sourced
    // from the same imxrt1062 IOMUXC entries as kFlexIOPins in
    // flexio_driver.cpp.hpp. Any pin from kFlexIOPins is a candidate; the
    // constraint is that MOSI and SCLK must map to two distinct shifter /
    // timer routings that don't conflict.
    return false;
}

bool flexio_spi_init(const FlexIOSPIPinInfo& /*pin_info*/,
                     u32 /*clock_hz*/) FL_NO_EXCEPT {
    // TODO(#3428): FlexIO2 SPI master mode setup:
    //   - Shifter 0: TRANSMIT, PINCFG=3 drives MOSI, TIMSEL=0, TIMPOL=0
    //     (shift on rising SCLK), PWIDTH=0 (1-bit), MSB-first via INSRC=0.
    //   - Timer 0: dual 8-bit baud, generates SCLK on its output pin,
    //     baud divider = (FlexIO clock / clock_hz / 2) - 1, bit count = 15
    //     (8 SCLK edges = 8 data bits MSB-first).
    //   - Clock = peripheral clock 60-120 MHz via CCM_CSCMR2 / CS1CDR
    //     (re-using the clockless mode's clock setup -- the peripheral is
    //     shared, not duplicated).
    return false;
}

bool flexio_spi_show(const u8* /*buffer*/, u32 /*num_bytes*/) FL_NO_EXCEPT {
    // TODO(#3428): DMA-driven SPI transmit using the same DMAChannel
    // pattern as flexio_show() but with TCD configured for 1-byte beats
    // (SSIZE/DSIZE = 0) into SHIFTBUF[0] (byte-wide MSB-aligned).
    return false;
}

void flexio_spi_wait() FL_NO_EXCEPT {
    // TODO(#3428): block until DMA complete, mirror flexio_wait() bounded
    // 50 ms timeout pattern.
}

void flexio_spi_deinit() FL_NO_EXCEPT {
    // TODO(#3428): release shifter 0 + timer 0 + DMA channel, restore
    // MOSI/SCLK pin mux to safe input. Called when the engine needs to
    // hand the peripheral back to clockless mode.
}

}  // namespace fl

#endif  // FL_IS_TEENSY_4X

#endif  // CHANNEL_ENGINE_FLEXIO_SPI_MODE_CPP_HPP_
