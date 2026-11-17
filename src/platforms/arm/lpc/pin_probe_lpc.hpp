#pragma once

// IWYU pragma: private

/// @file platforms/arm/lpc/pin_probe_lpc.hpp
/// Pin-probe table for LPC845 (LPC845-BRK).
///
/// Runtime pin N is PIO0_N (pin_lpc.hpp); PIO1 is not in the grid, so pins
/// 32+ are "not-a-gpio". Pads a probe must not touch:
///   - PIO0_2 SWDIO, PIO0_3 SWCLK: enabled fixed functions at reset
///     (UM11029 PINENABLE0); the onboard CMSIS-DAP probe is wired to them.
///   - PIO0_5 RESET: driving it low resets the chip.
///   - PIO0_12 ISP entry: low at reset enters the ROM bootloader (UM11029 ISP).
///   - PIO0_24 U0_RXD / PIO0_25 U0_TXD: the console USART0 that carries the
///     RPC link to the LPC11U35 VCOM bridge (framework variant.h).

#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace platforms {

inline const char* pinProbeSkipReason(int pin) FL_NO_EXCEPT {
    switch (pin) {
    case 2:
    case 3:
        return "debug-swd";
    case 5:
        return "reset";
    case 12:
        return "isp-entry";
    case 24:
    case 25:
        return "console-link";
    default:
        break;
    }
    return (pin < 0 || pin >= 32) ? "not-a-gpio" : nullptr;
}

/// PIO0_10/PIO0_11 are true open-drain I2C pads (no pulls, sink only), but a
/// probe may still drive them LOW, so they are not listed here.
inline const char* pinProbeDriveSkipReason(int /*pin*/) FL_NO_EXCEPT { return nullptr; }

}  // namespace platforms
}  // namespace fl
