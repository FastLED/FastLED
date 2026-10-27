#pragma once

// IWYU pragma: private

/// @file hspi_clock.h
/// ESP8266 HSPI clock-divider math.
///
/// Kept free of the ESP register headers so the divider search can be unit-
/// tested on the host; fastspi_esp8266_hspi.h writes the result into SPI1CLK.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace esp8266_hspi {

/// HSPI's source clock: the 80 MHz APB clock (also at 160 MHz CPU).
constexpr u32 kApbHz = 80000000;

/// SPI1CLK value for the fastest clock that does not exceed `hz`.
///
/// The divider is 80 MHz / (pre * (n + 1)), where SPI_CLKDIV_PRE is 13 bits
/// and SPI_CLKCNT_N is 6 bits, and the read and write half-periods must be
/// halves of the same period. Anything above the APB clock is the undivided
/// clock (SPI_CLK_EQU_SYSCLK), which also needs the pad mux bit set.
inline u32 clockRegister(u32 hz) FL_NO_EXCEPT {
    if (hz >= kApbHz) {
        return 0x80000000;  // SPI_CLK_EQU_SYSCLK
    }
    if (hz == 0) {
        hz = 1;
    }
    u32 best_reg = (0x1FFFu << 18) | (63u << 12) | (31u << 6) | 63u;  // slowest
    u32 best_hz = 0;
    for (u32 n = 1; n <= 63; ++n) {
        // Smallest prescaler that keeps the clock at or below `hz`.
        const u64 step = u64(hz) * (n + 1);  // u64: hz * 64 overflows u32 above 67 MHz
        u32 pre = u32((kApbHz + step - 1) / step);
        if (pre < 1) {
            pre = 1;
        }
        if (pre > 0x2000) {
            continue;
        }
        const u32 f = kApbHz / (pre * (n + 1));
        if (f > best_hz) {
            best_hz = f;
            best_reg = ((pre - 1) << 18) | (n << 12) | ((((n + 1) / 2) - 1) << 6) | n;
        }
    }
    return best_reg;
}

/// Clock the register value in `clockRegister()` describes.
inline u32 clockRegisterToHz(u32 reg) FL_NO_EXCEPT {
    if (reg == 0x80000000) {
        return kApbHz;
    }
    const u32 pre = ((reg >> 18) & 0x1FFFu) + 1;
    const u32 n = ((reg >> 12) & 0x3Fu) + 1;
    return kApbHz / (pre * n);
}

}  // namespace esp8266_hspi
}  // namespace fl
