/// @file objectfled_spi_mode.h
/// @brief ObjectFLED SPI-mode hardware helpers (paired with the
/// clockless-mode DMA-to-GPIO bank used by `clockless_objectfled.h`).
/// Same FlexPWM + eDMA + GPIO bank peripheral, different DMA payload
/// layout. The unified `ChannelEngineObjectFLED` chooses between the
/// two modes per channel based on `ChannelData::isSpi()`. See #3428.
///
/// Approach (planned): DMA-bit-banged SPI. Each SCLK cycle is two DMA
/// writes to GPIO6_DR -- write A holds MOSI=data_bit, SCLK=0; write B
/// holds MOSI=data_bit, SCLK=1. FlexPWM clocks the DMA at twice the
/// target SCLK rate. APA102/SK9822 needs ~6-30 MHz SCLK, so DMA rate
/// ~12-60 MHz -- comfortably within Teensy 4.x eDMA bandwidth. MOSI
/// and SCLK must be on the SAME GPIO bank (typically GPIO6) so one
/// DMA write updates both bits simultaneously.

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Routing info for an (MOSI, SCLK) pin pair on the same GPIO bank.
struct ObjectFLEDSPIPinInfo {
    u8 mosi_pin;         ///< Teensy MOSI pin number
    u8 sclk_pin;         ///< Teensy SCLK pin number
    u8 gpio_bank;        ///< GPIO bank index (6 for GPIO6, etc.)
    u32 mosi_bit_mask;   ///< Bit mask in the bank's DR register for MOSI
    u32 sclk_bit_mask;   ///< Bit mask in the bank's DR register for SCLK
};

/// Look up an (MOSI, SCLK) pin pair on the ObjectFLED DMA-able GPIO bank.
/// Both pins must be on the same bank for the unified DMA write to flip
/// MOSI and SCLK together. Returns true when the pair is routable.
bool objectfled_spi_lookup_pins(u8 mosi_pin, u8 sclk_pin,
                                ObjectFLEDSPIPinInfo* info) FL_NO_EXCEPT;

/// Initialize the FlexPWM + eDMA peripherals for SPI-mode transmission.
/// `clock_hz` is clamped to [100 kHz, 25 MHz] for testing reliability.
/// Returns false on configuration error (pin pair invalid, clock out of
/// range, peripheral busy, etc.).
bool objectfled_spi_init(const ObjectFLEDSPIPinInfo& pin_info,
                         u32 clock_hz) FL_NO_EXCEPT;

/// Begin a DMA-driven SPI transmit of `num_bytes` from `buffer` (MSB-first,
/// 8-bit beats). Returns false if not initialized or arguments invalid.
/// Caller should `objectfled_spi_wait()` before re-arming.
bool objectfled_spi_show(const u8* buffer, u32 num_bytes) FL_NO_EXCEPT;

/// Block until the in-flight DMA completes (bounded 50 ms timeout).
void objectfled_spi_wait() FL_NO_EXCEPT;

/// Tear down the SPI mode -- releases FlexPWM, DMA channel, and GPIO bits.
/// Call before re-initializing the peripheral in clockless mode.
void objectfled_spi_deinit() FL_NO_EXCEPT;

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
