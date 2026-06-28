/// @file flexio_internal.h
/// @brief Internal helpers shared between flexio_driver.cpp.hpp (clockless
/// WS2812 mode) and flexio_spi_mode.cpp.hpp (SPI master mode). The two TUs
/// drive the SAME FlexIO2 peripheral and need to share the pin lookup
/// table + the CCM clock-gate setup so they don't fight over the divider
/// values or duplicate the entries. See #3428.

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Raw FlexIO2 pin-table entry, exposed for cross-TU lookups
/// (kept structurally identical to the kFlexIOPins[] entry defined in
/// flexio_driver.cpp.hpp).
struct FlexIOPinEntry {
    u8 teensy_pin;
    u8 flexio_pin;
    u32 mux_reg_offset;  // byte offset from IOMUXC base (0x401F8000)
    u32 pad_reg_offset;  // byte offset from IOMUXC base
};

/// @brief Look up the raw FlexIO2 pin-table entry for a Teensy digital pin.
/// @return true if the pin is FlexIO2-routable, false otherwise.
bool flexio_lookup_pin_entry(u8 teensy_pin, FlexIOPinEntry* out) FL_NO_EXCEPT;

/// @brief Bring the CCM FlexIO2 clock gate up (gate, source, dividers) so
/// the FlexIO2 base clock is the standard 120 MHz both modes assume.
/// Idempotent: re-calling after the clock is already running is harmless
/// (the gate is briefly cycled off then back on, but the existing divider
/// pair is preserved by re-writing the same values).
void flexio_ensure_clock() FL_NO_EXCEPT;

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
