/// @file flexio_driver.h
/// @brief Low-level FlexIO2 WS2812 driver for IMXRT1062 (Teensy 4.x)
///
/// Uses the 4-timer + 1-shifter architecture from RESEARCH.md §8:
/// - Shifter 0: Transmit mode, feeds pixel data via DMA
/// - Timer 0: Shift clock (baud mode), driven by shifter status flag
/// - Timer 1: Low-bit PWM (always fires, short HIGH pulse for '0' bit)
/// - Timer 2: High-bit PWM (fires only when data=1, extends HIGH for '1' bit)
/// - Timer 3: Latch timer (>50µs LOW reset signal)
///
/// Timer 1 and Timer 2 outputs are OR'd on the same output pin by FlexIO hardware.
/// DMA refills SHIFTBUFBIS (bit-swapped buffer) from pixel data in RAM.

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief FlexIO2 pin mapping entry
struct FlexIOPinInfo {
    u8 teensy_pin;       ///< Teensy digital pin number
    u8 flexio_pin;       ///< FlexIO2 pin number (for PINSEL fields)
    volatile u32* mux_reg;  ///< IOMUXC mux register address
    volatile u32* pad_reg;  ///< IOMUXC pad control register address
};

/// @brief Look up FlexIO2 pin info for a Teensy pin
/// @param teensy_pin Teensy digital pin number
/// @param[out] info Filled with pin mapping if found
/// @return true if pin has a FlexIO2 mapping, false otherwise
bool flexio_lookup_pin(u8 teensy_pin, FlexIOPinInfo* info) FL_NOEXCEPT;

/// @brief Initialize FlexIO2 hardware for WS2812 waveform generation
/// @param pin_info Pin mapping info (from flexio_lookup_pin)
/// @param t0h_ns T0H timing in nanoseconds (high time for '0' bit)
/// @param t1h_ns T1H timing in nanoseconds (high time for '1' bit)
/// @param period_ns Total bit period in nanoseconds
/// @param reset_us Reset/latch time in microseconds
/// @return true on success, false on failure
bool flexio_init(const FlexIOPinInfo& pin_info, u32 t0h_ns, u32 t1h_ns,
                 u32 period_ns, u32 reset_us) FL_NOEXCEPT;

/// @brief Start DMA transfer of pixel data to FlexIO2
/// @param pixel_data Pointer to encoded pixel bytes (must be 32-bit aligned)
/// @param num_bytes Number of bytes to transmit
/// @return true if DMA transfer started, false on error
bool flexio_show(const u8* pixel_data, u32 num_bytes) FL_NOEXCEPT;

/// @brief Check if FlexIO2 DMA transfer is complete
/// @return true if transfer is done (or no transfer active), false if still running
bool flexio_is_done() FL_NOEXCEPT;

/// @brief Block until FlexIO2 DMA transfer completes
void flexio_wait() FL_NOEXCEPT;

/// @brief Shut down FlexIO2 and release resources
void flexio_deinit() FL_NOEXCEPT;

} // namespace fl
