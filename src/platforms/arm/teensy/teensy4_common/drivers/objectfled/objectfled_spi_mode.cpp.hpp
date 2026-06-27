// IWYU pragma: private

/// @file objectfled_spi_mode.cpp.hpp
/// @brief ObjectFLED SPI-mode hardware helpers -- SCAFFOLDING STUB
///
/// Architectural scaffolding for the unified ObjectFLED clockless+SPI
/// engine per the rule established in PR #3430 (see
/// `src/fl/channels/README.md` -> "Rule: Parallel-IO peripherals --
/// one engine for both clockless and SPI modes" and #3428).
///
/// The engine treats the FlexPWM + eDMA + GPIO bank as ONE peripheral
/// that can run in either mode, switching modes per-channel inside
/// `show()`. This file is the SPI-side hardware shim (the clockless
/// side is `clockless_objectfled.h` / `ObjectFLED*`).
///
/// **Hardware programming is intentionally not implemented yet.** All
/// functions return / behave as if the SPI hardware is unavailable so
/// `ChannelEngineObjectFLED::canHandle()` rejects SPI channels and they
/// fall through to the next bus via priority dispatch. A follow-up PR
/// (#3428) replaces these stubs with the real DMA-bit-banged SPI
/// implementation (two DMA writes per SCLK cycle to GPIO6_DR, FlexPWM
/// clocking the DMA at 2x the SCLK rate).

#ifndef CHANNEL_ENGINE_OBJECTFLED_SPI_MODE_CPP_HPP_
#define CHANNEL_ENGINE_OBJECTFLED_SPI_MODE_CPP_HPP_

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_spi_mode.h"

namespace fl {

bool objectfled_spi_lookup_pins(u8 /*mosi_pin*/, u8 /*sclk_pin*/,
                                ObjectFLEDSPIPinInfo* /*info*/) FL_NO_EXCEPT {
    // TODO(#3428): pin-pair lookup on the DMA-able GPIO bank used by
    // ObjectFLED (GPIO6 on Teensy 4.x). Both pins must be on the same
    // bank so one 32-bit DMA write to GPIO6_DR can flip MOSI and SCLK
    // together. Pin -> (bank, bit_mask) mapping is in Teensyduino's
    // core_pins.h.
    return false;
}

bool objectfled_spi_init(const ObjectFLEDSPIPinInfo& /*pin_info*/,
                         u32 /*clock_hz*/) FL_NO_EXCEPT {
    // TODO(#3428): FlexPWM + eDMA SPI mode setup:
    //   - Configure MOSI and SCLK pins as outputs in GPIO6_GDIR.
    //   - Allocate FlexPWM submodule (or reuse the same submodule used
    //     by clockless mode -- the peripheral is shared, not duplicated).
    //   - FlexPWM tick rate = 2 * clock_hz (two DMA writes per SCLK cycle).
    //   - Configure eDMA channel: source = pre-built bit-pattern buffer
    //     (each input byte expands to 16 32-bit words: 8 SCLK cycles x
    //     2 phases). Destination = GPIO6_DR.
    //   - Bit pattern encoder (offline): for each input byte b, write
    //     16 words alternating (MOSI=b[7], SCLK=0), (MOSI=b[7], SCLK=1),
    //     (MOSI=b[6], SCLK=0), ... preserving all other GPIO6 bits.
    return false;
}

bool objectfled_spi_show(const u8* /*buffer*/, u32 /*num_bytes*/) FL_NO_EXCEPT {
    // TODO(#3428): pre-encode `buffer` into a bit-pattern buffer (16x
    // expansion), then start eDMA transfer to GPIO6_DR. Use the same
    // ObjectFLEDDmaManager singleton ownership pattern as clockless mode
    // so the two modes can't race over DMA channel allocation.
    return false;
}

void objectfled_spi_wait() FL_NO_EXCEPT {
    // TODO(#3428): block until DMA complete, mirror flexio_wait()'s
    // bounded 50 ms timeout pattern.
}

void objectfled_spi_deinit() FL_NO_EXCEPT {
    // TODO(#3428): release FlexPWM submodule + DMA channel, restore
    // MOSI/SCLK GPIO bits to input. Called when the engine needs to
    // hand the peripheral back to clockless mode.
}

}  // namespace fl

#endif  // FL_IS_TEENSY_4X

#endif  // CHANNEL_ENGINE_OBJECTFLED_SPI_MODE_CPP_HPP_
