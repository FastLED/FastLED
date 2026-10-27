#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/spi_hspi_clock.h
/// @brief ESP8266 SPI1 (HSPI) clock divider computation
///
/// Pure compile-time arithmetic, deliberately free of SDK headers so it can be
/// unit tested on the host. The register layout and the frequency formula match
/// the ESP8266 Arduino core's SPI library:
///
///   SPI1CLK: [31] EQU | [30:18] CLKDIV_PRE | [17:12] CLKCNT_N | [11:6] CLKCNT_H | [5:0] CLKCNT_L
///   f_spi = 80 MHz / ((CLKDIV_PRE + 1) * (CLKCNT_N + 1))
///
/// CLKCNT_H stays zero and CLKCNT_L is (CLKCNT_N + 1) / 2, giving the 50% duty
/// cycle the Arduino core programs. The SPI peripheral is clocked from the
/// fixed 80 MHz source, independent of the CPU frequency setting.
///
/// CLKCNT_L is the clock's high phase, so it must never be zero: with the
/// CLKCNT_N *field* at 0 (a CLKCNT_N + 1 divider of 1) it is 0, and SCK then
/// stays low for the whole transfer while the shifter runs for the programmed
/// time. Measured on an ESP8266 -- a 25 kHz transfer with CLKCNT_N = CLKCNT_L =
/// 0 showed 0 edges and 0 high samples over 4691 polls of the pad. The
/// CLKCNT_N + 1 divider is therefore at least 2 (encoded CLKCNT_N = 1); ESP-IDF
/// starts its own search at 2 for the same reason.
///
/// Kept C++11-constexpr clean (single-return functions, no local variables):
/// the host test build rejects C++14 constexpr extensions with -Werror.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace esp8266 {

/// Computed SPI1CLK register value and the frequency it actually produces.
struct HspiClock {
    fl::u32 reg_value;  ///< Value to write to SPI1CLK
    fl::u32 actual_hz;  ///< Resulting clock frequency (0 = no valid divider)
};

/// SPI1 peripheral source clock (fixed, independent of the CPU frequency).
constexpr fl::u32 kHspiSourceHz = 80000000UL;

/// Largest CLKDIV_PRE + 1 (13-bit field).
constexpr fl::u32 kHspiMaxPreDivider = 8192UL;

/// Largest CLKCNT_N + 1 (6-bit field).
constexpr fl::u32 kHspiMaxNDivider = 64UL;

/// Smallest usable CLKCNT_N + 1: CLKCNT_L is half of it, and a zero high phase
/// never clocks the pad.
constexpr fl::u32 kHspiMinNDivider = 2UL;

/// SPICLK_EQU_SYSCLK: clock the SPI bus directly from the system clock.
constexpr fl::u32 kHspiEquSysClk = 0x80000000UL;

/// Ceiling division for unsigned values (b must be non-zero).
constexpr fl::u32 hspiCeilDiv(fl::u32 a, fl::u32 b) FL_NO_EXCEPT {
    return (a + b - 1) / b;
}

/// Lift a CLKCNT_N + 1 divider into the usable range.
constexpr fl::u32 hspiClampN(fl::u32 n) FL_NO_EXCEPT {
    return (n < kHspiMinNDivider) ? kHspiMinNDivider : n;
}

/// Build the SPI1CLK value for one (pre, n) divider pair.
constexpr HspiClock hspiCandidate(fl::u32 pre, fl::u32 n) FL_NO_EXCEPT {
    // CLKCNT_L = (CLKCNT_N + 1) / 2 with CLKCNT_H = 0 (50% duty). Fields store
    // (divider - 1).
    //
    // n is clamped to kHspiMinNDivider because CLKCNT_L is half of it: at n = 1
    // the high phase is 0 and the pad never rises. Clamping here rather than
    // only in the search keeps the register value and the reported frequency
    // consistent for every way of building a candidate.
    return HspiClock{
        (hspiClampN(n) / 2) | ((hspiClampN(n) - 1) << 12) | ((pre - 1) << 18),
        kHspiSourceHz / (pre * hspiClampN(n))};
}

/// Prefer the faster candidate (smaller total divider); keep the first on ties.
constexpr HspiClock hspiFaster(HspiClock a, HspiClock b) FL_NO_EXCEPT {
    return (b.actual_hz > a.actual_hz) ? b : a;
}

/// Slowest possible setting, used when no divider reaches the requested rate.
constexpr HspiClock hspiSlowest() FL_NO_EXCEPT {
    return hspiCandidate(kHspiMaxPreDivider, kHspiMaxNDivider);
}

/// Search n = kHspiMinNDivider..kHspiMaxNDivider for the smallest divider >=
/// kMinDiv.
constexpr HspiClock hspiSearch(fl::u32 min_div, fl::u32 n) FL_NO_EXCEPT {
    return (n > kHspiMaxNDivider)
        ? HspiClock{0, 0}
        : hspiFaster(
              (hspiCeilDiv(min_div, n) > kHspiMaxPreDivider)
                  ? HspiClock{0, 0}
                  : hspiCandidate(hspiCeilDiv(min_div, n), n),
              hspiSearch(min_div, n + 1));
}

/// Resolve a minimum divider to the best available setting.
constexpr HspiClock hspiResolve(fl::u32 min_div) FL_NO_EXCEPT {
    return (hspiSearch(min_div, kHspiMinNDivider).actual_hz == 0)
        ? hspiSlowest()
        : hspiSearch(min_div, kHspiMinNDivider);
}

/// Convert the software path's CPU-cycles-per-bit value to a minimum total
/// HSPI divider.
///
/// FL_DATA_RATE_MHZ(X) on ESP8266 is `(F_CPU / 1 MHz) / X` truncated, so the
/// sketch's original request is strictly above `F_CPU / cycles` (where
/// `cycles` is the truncated value plus one). A total hardware divider of at
/// least `ceil(cycles * 80 MHz / F_CPU)` therefore never clocks the bus faster
/// than the rate that was actually requested. The conversion is needed because
/// the HSPI source is fixed at 80 MHz while the CPU may run at 80 or 160 MHz.
///
/// @param cycles CPU cycles per bit from FL_DATA_RATE_MHZ (0 is treated as 1)
/// @param cpu_hz CPU frequency the divider was computed against
/// @return minimum total divider for hspiClockForMinDivider()
constexpr fl::u32 hspiDividerForCpuCycles(fl::u32 cycles, fl::u32 cpu_hz) FL_NO_EXCEPT {
    return static_cast<fl::u32>(
        (static_cast<fl::u64>((cycles == 0) ? 1 : cycles) * kHspiSourceHz + cpu_hz - 1) /
        cpu_hz);
}

/// Compute the SPI1CLK value for a minimum total divider.
///
/// Picks the smallest achievable divider that is at least @p min_div, so the
/// bus never clocks faster than the rate that divider stands for.
///
/// @param min_div minimum (CLKDIV_PRE + 1) * (CLKCNT_N + 1); 0 is treated as 1
/// @return the register value and the frequency it produces
constexpr HspiClock hspiClockForMinDivider(fl::u32 min_div) FL_NO_EXCEPT {
    return hspiResolve((min_div == 0) ? 1 : min_div);
}

/// Compute the SPI1CLK value for a target frequency.
///
/// Picks the smallest achievable divider whose resulting frequency does not
/// exceed @p target_hz, so the bus never runs faster than requested. The one
/// exception is a request below the hardware floor (80 MHz / (8192 * 64) =
/// 152 Hz); that returns the slowest possible setting rather than failing.
///
/// @param target_hz requested clock frequency in Hz (0 is treated as 1 Hz)
/// @return the register value and the frequency it produces
constexpr HspiClock hspiClockFor(fl::u32 target_hz) FL_NO_EXCEPT {
    return (target_hz >= kHspiSourceHz)
        ? HspiClock{kHspiEquSysClk, kHspiSourceHz}
        : hspiResolve(hspiCeilDiv(kHspiSourceHz, (target_hz == 0) ? 1 : target_hz));
}

}  // namespace esp8266
}  // namespace fl
