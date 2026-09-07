#pragma once

#ifndef __INC_FASTLED_PLATFORMS_CYCLE_TYPE_H
#define __INC_FASTLED_PLATFORMS_CYCLE_TYPE_H

/// @file platforms/cycle_type.h
/// Platform-specific typedef for CPU clock cycle counts
/// Used as template parameter for delaycycles<cycle_t CYCLES>()

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Type for representing CPU clock cycle counts
/// - On AVR: int (16-bit signed) to minimize memory usage
/// - On other platforms: fl::i64 (64-bit signed) for high-frequency CPUs
#if defined(__AVR__)
typedef int cycle_t;
#else
typedef fl::i64 cycle_t;
#endif

/// Convert nanoseconds to CPU cycles, rounding up.
///
/// The obvious form, `((u64)ns * hz + 999999999) / 1000000000`, costs a
/// 64-bit divide by 1e9. Neither Cortex-M33 nor RV32 has a hardware 64-bit
/// divide, so it lowers to a software routine: measured at 2.533us of the
/// 3.020us that one `fl::delayNanoseconds(400)` call took on an RP2350W,
/// against 0.469us for the same delay with the conversion folded at compile
/// time. A clockless bit loop calls this three times per bit, which is what
/// made `Bus::BIT_BANG` emit undecodable WS2812 (FastLED#4203).
///
/// Scaling the clock to kHz first keeps the arithmetic in 32 bits for the
/// short delays this is used for, so the compiler can turn `/ 1000000` into
/// a multiply-shift. The fast path is bounded so `ns * (hz / 1000) + 999999`
/// cannot overflow u32: at the 1GHz ceiling that product is 4.001e9, just
/// under 2^32-1. Anything outside the bound takes the exact 64-bit form —
/// those callers are not in a hot loop and do not care about the divide.
///
/// Verified exhaustively against the 64-bit form for ns in [0, 4100] plus
/// long delays, across 16 clock rates from 8MHz to 1GHz: identical results,
/// no overflow.
///
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr u32 cycles_from_ns(u32 ns, u32 hz) FL_NO_EXCEPT {
    return (ns <= 4000u && hz <= 1000000000u)
        ? ((ns * (hz / 1000u)) + 999999u) / 1000000u
        : static_cast<u32>(((fl::u64)ns * (fl::u64)hz + 999999999ULL)
                           / 1000000000ULL);
}

} // namespace fl

#endif // __INC_FASTLED_PLATFORMS_CYCLE_TYPE_H
