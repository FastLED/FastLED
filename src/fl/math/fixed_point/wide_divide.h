#pragma once

/// @file wide_divide.h
/// A 64/32 -> 32 division built from 32-bit divides.
///
/// Q16.16 division is `(a << 16) / b`, and with `a` a full-range 32-bit value
/// the numerator genuinely needs 48 bits -- narrowing it would silently
/// overflow, which is worse than being slow. So the width is not the problem;
/// what the width *costs* is.
///
/// A target with a 32-bit hardware divider and no 64-bit one has to call into
/// libgcc for that division. Measured on the toolchain the RP2350 core ships,
/// for Cortex-M33 at `-Os`:
///
/// | | |
/// |---|---|
/// | `(i64)a * 65536 / b` | 10 instructions, then a call to `__aeabi_ldivmod` |
/// | `__aeabi_ldivmod` | 34 instructions, tail-calling `__divdi3` (230) |
/// | `__udivdi3` (the unsigned worker) | 184 instructions, 19 backward branches |
/// | this | **62 instructions inline, two `udiv`, no call** |
///
/// FastLED#4307 measured the runtime cost of that call on hardware: division
/// on `s16x16` ran 47x slower than on `s8x8`, whose 32-bit intermediate
/// lowers to a single `SDIV`, and 36x slower than scalar `float`.

#include "fl/stl/int.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

/// 1 when the target divides 32 bits in hardware but not 64.
///
/// Both halves matter. Without a hardware divider at all -- Cortex-M0/M0+,
/// AVR -- the routine below would make two libgcc calls where the wide path
/// makes one, so those targets keep the wide path. With a *64-bit* divider,
/// as on every host this builds on, the wide path is one instruction and
/// nothing here could improve it.
///
/// `__ARM_FEATURE_IDIV` is GCC and clang's own answer to "does this core have
/// SDIV/UDIV", so this asks the compiler rather than enumerating cores:
/// Cortex-M3/M4/M7/M33 define it, Cortex-M0+ does not.
///
/// Paired with `__arm__`, which is AArch32 only. AArch64 defines
/// `__ARM_FEATURE_IDIV` too -- it has SDIV and UDIV -- but its divider is 64
/// bits wide, so the wide path is already one instruction there and this
/// would replace it with fifty. Every 64-bit ARM host and any Raspberry Pi
/// running a 64-bit kernel is in that set, so the omission was not
/// theoretical.
///
/// C++14 is required as well, because `operator/` is `constexpr` and the
/// routine below cannot be under C++11's rule against local variables in a
/// constexpr function -- the repo builds at C++11 to match AVR. A C++11 build
/// for a core with a divider therefore keeps the wide path: correct, just not
/// accelerated. The Arduino-Pico core passes `-std=gnu++17`, so the RP2350
/// this was measured on is not that build.
#ifndef FL_FIXED_POINT_NARROW_DIVIDE
#if __cplusplus < 201402L
#define FL_FIXED_POINT_NARROW_DIVIDE 0
#elif defined(__arm__) && defined(__ARM_FEATURE_IDIV) && \
    (__ARM_FEATURE_IDIV + 0) == 1
#define FL_FIXED_POINT_NARROW_DIVIDE 1
#elif defined(__riscv_div) && defined(__riscv_xlen) && (__riscv_xlen + 0) == 32
#define FL_FIXED_POINT_NARROW_DIVIDE 1
#else
#define FL_FIXED_POINT_NARROW_DIVIDE 0
#endif
#endif

/// `constexpr` where the language allows it, always force-inlined.
///
/// Spelled out rather than written as `FL_CONSTEXPR14 FASTLED_FORCE_INLINE`,
/// because `FL_CONSTEXPR14` is `inline` under C++11 and `FASTLED_FORCE_INLINE`
/// ends in `inline` too -- the pair expands to `inline inline`, which GCC
/// rejects. That broke the `clearcore` platform build, and a case now pins
/// the C++11 configuration it broke on.
#if __cplusplus >= 201402L
#define FL_FIXED_POINT_DIVIDE_INLINE constexpr FASTLED_FORCE_INLINE
#else
#define FL_FIXED_POINT_DIVIDE_INLINE FASTLED_FORCE_INLINE
#endif

namespace fl {

namespace detail {

/// Leading zeros of a non-zero 32-bit value; 32 for zero.
FL_FIXED_POINT_DIVIDE_INLINE int fixedPointLeadingZeros(u32 value) FL_NO_EXCEPT {
#if defined(__GNUC__) || defined(__clang__)
    // One CLZ instruction on every core this file's fast path is enabled for.
    return value == 0 ? 32 : __builtin_clz(value);
#else
    int count = 0;
    while (count < 32 && (value & 0x80000000u) == 0u) {
        value <<= 1;
        ++count;
    }
    return count;
#endif
}

}  // namespace detail

/// `(hi:lo) / divisor`, for a quotient that fits 32 bits.
///
/// Knuth's Algorithm D over two base-2^16 digits, which is what lets a
/// 64-bit numerator be divided using the 32-bit divider twice. The two
/// correction loops run at most twice each and usually not at all; they are
/// what make the base-2^16 estimate exact rather than approximate.
///
/// Defined, not undefined, on the two inputs the wide path leaves to the
/// compiler: a zero divisor and a quotient too large for 32 bits both return
/// `0xFFFFFFFF`. That is a deliberate difference and is not something a
/// caller may rely on -- the wide path wraps instead -- so `operator/`'s
/// contract is unchanged and the divergence is pinned by a test rather than
/// left to be discovered.
///
/// Kept out of `#if FL_FIXED_POINT_NARROW_DIVIDE` on purpose. Gating the
/// definition would leave it compiled by no host and tested by nothing, which
/// is how `simd_noop.hpp` went two rounds of tuning against a build that
/// excluded it (FastLED#4216).
FL_FIXED_POINT_DIVIDE_INLINE u32 divide64By32(u32 hi, u32 lo,
                                             u32 divisor) FL_NO_EXCEPT {
    constexpr u32 kBase = 65536u;
    if (divisor == 0u || hi >= divisor) {
        return 0xFFFFFFFFu;
    }

    // Normalising the divisor is what makes the digit estimate below good to
    // within one, which is the whole reason two corrections suffice.
    const int shift = detail::fixedPointLeadingZeros(divisor);
    const u32 d = divisor << shift;
    const u32 d_high = d >> 16;
    const u32 d_low = d & 0xFFFFu;

    // A shift of 32 is undefined, so the unshifted case is spelled out rather
    // than folded in with a mask.
    const u32 n_high = shift == 0 ? hi : ((hi << shift) | (lo >> (32 - shift)));
    const u32 n_low = lo << shift;
    const u32 n_low_high = n_low >> 16;
    const u32 n_low_low = n_low & 0xFFFFu;

    u32 quotient_high = n_high / d_high;
    u32 remainder = n_high - quotient_high * d_high;
    while (quotient_high >= kBase ||
           static_cast<u64>(quotient_high) * d_low >
               static_cast<u64>(kBase) * remainder + n_low_high) {
        --quotient_high;
        remainder += d_high;
        if (remainder >= kBase) {
            break;
        }
    }

    // Deliberate 32-bit wraparound: the true value needs 33 bits and the
    // low 32 are the ones the next digit is taken from.
    const u32 partial =
        static_cast<u32>(n_high * kBase + n_low_high - quotient_high * d);

    u32 quotient_low = partial / d_high;
    remainder = partial - quotient_low * d_high;
    while (quotient_low >= kBase ||
           static_cast<u64>(quotient_low) * d_low >
               static_cast<u64>(kBase) * remainder + n_low_low) {
        --quotient_low;
        remainder += d_high;
        if (remainder >= kBase) {
            break;
        }
    }

    return quotient_high * kBase + quotient_low;
}

}  // namespace fl
