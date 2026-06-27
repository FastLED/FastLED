/// @file flexio_spi_mode.h
/// @brief FlexIO2 SPI-mode hardware helpers (paired with flexio_driver.h's
/// clockless WS2812 mode). Same FlexIO2 peripheral, different shifter/timer
/// configuration. The unified ChannelEngineFlexIO chooses between the two
/// modes per channel based on `ChannelData::isSpi()`. See #3428.

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

struct FlexIOSPIPinInfo {
    u8 mosi_pin;
    u8 sclk_pin;
    u8 mosi_flexio_pin;
    u8 sclk_flexio_pin;
    volatile u32* mosi_mux_reg;
    volatile u32* mosi_pad_reg;
    volatile u32* sclk_mux_reg;
    volatile u32* sclk_pad_reg;
};

/// Look up a (MOSI, SCLK) pin pair in the FlexIO2 pin table. Returns true
/// when both pins are FlexIO2-routable and the info struct is populated.
bool flexio_spi_lookup_pins(u8 mosi_pin, u8 sclk_pin, FlexIOSPIPinInfo* info) FL_NO_EXCEPT;

/// Initialize FlexIO2 in SPI master mode: shifter 0 = MOSI transmit,
/// timer 0 = SCLK baud generator. Mode 0 (CPOL=0, CPHA=0), MSB-first,
/// 8-bit byte beats. `clock_hz` is clamped to [100 kHz, 30 MHz] for
/// testing reliability per #3428 (FlexIO peripheral can do up to 45 MHz
/// per lane but APA102 spec caps at 25-30 MHz).
bool flexio_spi_init(const FlexIOSPIPinInfo& pin_info, u32 clock_hz) FL_NO_EXCEPT;

/// Begin a DMA-driven transmit of `num_bytes` from `buffer`. Returns false
/// if not initialized or arguments invalid. Caller should `flexio_spi_wait()`
/// before re-arming.
bool flexio_spi_show(const u8* buffer, u32 num_bytes) FL_NO_EXCEPT;

/// Block until the in-flight DMA completes (50ms bounded timeout).
void flexio_spi_wait() FL_NO_EXCEPT;

/// Tear down the SPI mode — disables FlexIO2, frees the DMA channel.
/// Call before re-initializing in clockless mode on the same peripheral.
void flexio_spi_deinit() FL_NO_EXCEPT;

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
