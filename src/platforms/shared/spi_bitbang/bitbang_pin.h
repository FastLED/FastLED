#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/system/delay.h"

namespace fl {

/// Additional phase time after known GPIO work, without unsigned underflow.
/// Both the requested half-period and the minimum pulse must be satisfied.
constexpr cycle_t bitbangPhaseDelay(cycle_t requested, cycle_t minimum,
                                    cycle_t work) FL_NO_EXCEPT {
    return requested > minimum
        ? (requested > work ? requested - work : 0)
        : (minimum > work ? minimum - work : 0);
}

/// Per-phase delay for a rate in Hz: a quarter of the bit period, in ns. Divides
/// the rate directly so sub-MHz and fractional-MHz rates stay exact. Zero has no
/// period; the Teensy 4 writer rejects it at compile time.
constexpr u32 bitbangPhaseNsFromHz(u32 hz) FL_NO_EXCEPT {
    return hz == 0 ? 0 : 1000000000UL / hz / 4;
}

/// GenericSoftwareSPIOutput divider for a rate in Hz, at the given CPU clock, on
/// divider-based platforms. Teensy 4 takes Hz directly, so do not use this there. Rounds
/// up, and stays above 10 because that class reads 10 or less as a half period, which
/// would make faster requests run slower. @p hz must be nonzero.
constexpr u32 bitbangDividerFromHz(u32 hz, u32 cpu_hz) FL_NO_EXCEPT {
    return cpu_hz / hz + (cpu_hz % hz != 0 ? 1 : 0) < 11
        ? 11
        : cpu_hz / hz + (cpu_hz % hz != 0 ? 1 : 0);
}

/// One atomic GPIO update, without a data-dependent branch. Platforms with
/// write-one-to-set/clear registers can use this instead of a port snapshot.
template <u8 BIT>
FASTLED_FORCE_INLINE void writeBitSetClear(u8 value, u32 mask,
                                         volatile u32* set,
                                         volatile u32* clear) FL_NO_EXCEPT {
    volatile u32* target = (value & (1U << BIT)) ? set : clear;
    *target = mask;
}

/// writeBitSetClear for 16-bit set/clear registers, keeping the vendor access width.
template <u8 BIT>
FASTLED_FORCE_INLINE void writeBitSetClear(u8 value, u16 mask,
                                         volatile u16* set,
                                         volatile u16* clear) FL_NO_EXCEPT {
    volatile u16* target = (value & (1U << BIT)) ? set : clear;
    *target = mask;
}

/// writeBitSetClear without a data-dependent branch, for targets where the
/// ternary compiles to one. The asm keeps GCC from restoring the branch, so this
/// costs extra instructions even when the bit is known at compile time.
template <u8 BIT>
FASTLED_FORCE_INLINE void writeBitSetClearNoBranch(u8 value, u32 mask,
                                                   volatile u32* set,
                                                   volatile u32* clear) FL_NO_EXCEPT {
    uptr selected = static_cast<uptr>(0) - ((value >> BIT) & 1U);
    __asm__ __volatile__("" : "+r"(selected));
    // Plain casts: at -Os, fl::bit_cast is an out-of-line call per address.
    const uptr setAddr = reinterpret_cast<uptr>(set); // ok reinterpret cast
    const uptr clearAddr = reinterpret_cast<uptr>(clear); // ok reinterpret cast
    volatile u32* target = reinterpret_cast<volatile u32*>(clearAddr ^ ((setAddr ^ clearAddr) & selected)); // ok reinterpret cast
    *target = mask;
}

/// Default pin access; a platform may specialize the data-write operation
/// while the generic SPI writer continues to own bit order and clock timing.
template <class Pin> struct BitbangPinDefaults {
    static constexpr bool kAtomicWrite = false;
    static constexpr bool kTwoWriteStream = false;
    static constexpr cycle_t kMinWriteCycles = 0;
    /// ESP32 with its fast policy off: GenericSoftwareSPIOutput::writeBit()
    /// emits the FastLED 3.10.9 ESP32 sequence (see fastpin_esp32.h).
    static constexpr bool kEsp32LegacyWriteBit = false;
    template <fl::cycle_t CYCLES>
    FASTLED_FORCE_INLINE static void delay() FL_NO_EXCEPT {
        fl::delaycycles<CYCLES>();
    }
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        if (value & (1U << BIT)) Pin::hi();
        else Pin::lo();
    }
};

template <class Pin> struct BitbangPin : BitbangPinDefaults<Pin> {};

} // namespace fl
