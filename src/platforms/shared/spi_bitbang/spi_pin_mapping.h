#pragma once

// IWYU pragma: private

/// @file spi_pin_mapping.h
/// @brief Validates the pin indices parallel software SPI shifts into 32-bit masks.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Pins index one 32-bit GPIO bank, so 32 and above have no bit to map to.
constexpr u8 kSpiMaxPinIndex = 31;

/// A byte carries 8 bits, so the 256-entry tables drive lanes 0-7 and only ever
/// clear lanes 8 and up.
constexpr fl::size kSpiDrivenLanes = 8;

/// True when every data pin is in the bank, the driven lanes (0-7) are distinct
/// and stay off `reserved` (the clock), and no always-cleared lane (8+) lands on
/// a driven lane. Always-cleared lanes may repeat or share the clock: the clock
/// is cleared in the same write. On false the caller must change nothing.
inline bool spiDataPinsValid(const u8* pins, fl::size count, u32 reserved) FL_NO_EXCEPT {
    u32 driven = 0;
    for (fl::size i = 0; i < count; ++i) {
        if (pins[i] > kSpiMaxPinIndex) {
            return false;
        }
        const u32 bit = u32(1) << pins[i];
        if (i < kSpiDrivenLanes) {
            if ((driven | reserved) & bit) {
                return false;
            }
            driven |= bit;
        }
    }
    for (fl::size i = kSpiDrivenLanes; i < count; ++i) {
        if (driven & (u32(1) << pins[i])) {
            return false;
        }
    }
    return true;
}

/// Index form: the clock must be in the bank and off every driven lane.
inline bool spiPinMappingValid(const u8* pins, fl::size count, u8 clock) FL_NO_EXCEPT {
    return clock <= kSpiMaxPinIndex && spiDataPinsValid(pins, count, u32(1) << clock);
}

/// Mask form: the clock mask must name at least one pin and share none with a
/// driven lane.
inline bool spiPinMappingValidWithMask(const u8* pins, fl::size count, u32 clockMask) FL_NO_EXCEPT {
    return clockMask != 0 && spiDataPinsValid(pins, count, clockMask);
}

/// Array forms take the count from the array type.
template <fl::size N>
inline bool spiPinMappingValid(const u8 (&pins)[N], u8 clock) FL_NO_EXCEPT {
    return spiPinMappingValid(pins, N, clock);
}

template <fl::size N>
inline bool spiPinMappingValidWithMask(const u8 (&pins)[N], u32 clockMask) FL_NO_EXCEPT {
    return spiPinMappingValidWithMask(pins, N, clockMask);
}

}  // namespace fl
