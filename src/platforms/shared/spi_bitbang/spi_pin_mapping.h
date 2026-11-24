#pragma once

// IWYU pragma: private

/// @file spi_pin_mapping.h
/// @brief Validates the pin indices and raw masks parallel software SPI writes
/// into one 32-bit GPIO bank.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "platforms/shared/spi_bitbang/spi_isr_engine.h"

namespace fl {

/// Pins index one GPIO bank written through u32 masks; shifts always use
/// `u32(1)`, so the limit is u32's width on every target, including those with
/// a 16-bit `int`. Pins 32 and above have no bit to map to.
constexpr u8 kSpiMaxPinIndex = sizeof(u32) * 8 - 1;

/// True when every data pin is in the bank, no two data pins share a pin, and
/// none lands on `reserved` (the clock). The 256-entry tables only drive lanes
/// 0-7 and always clear lanes 8+, but an aliased lane would still give one pin
/// contradictory roles, so every lane owns its own pin. 32 data pins therefore
/// leave no room for a clock in one 32-bit bank. On false the caller must
/// change nothing.
inline bool spiDataPinsValid(const u8* pins, fl::size count, u32 reserved) FL_NO_EXCEPT {
    u32 used = reserved;
    for (fl::size i = 0; i < count; ++i) {
        // Range check before the shift: the mask is u32, so pins 32+ have no bit.
        if (pins[i] > kSpiMaxPinIndex) {
            return false;
        }
        const u32 bit = u32(1) << pins[i];
        if (used & bit) {
            return false;
        }
        used |= bit;
    }
    return true;
}

/// True when `mask` names exactly one pin.
inline bool spiIsSinglePinMask(u32 mask) FL_NO_EXCEPT {
    return mask != 0 && (mask & (mask - 1)) == 0;
}

/// Index form: the clock must be in the bank and off every data pin.
inline bool spiPinMappingValid(const u8* pins, fl::size count, u8 clock) FL_NO_EXCEPT {
    return clock <= kSpiMaxPinIndex && spiDataPinsValid(pins, count, u32(1) << clock);
}

/// Mask form: the clock mask must name exactly one pin, off every data pin.
inline bool spiPinMappingValidWithMask(const u8* pins, fl::size count, u32 clockMask) FL_NO_EXCEPT {
    return spiIsSinglePinMask(clockMask) && spiDataPinsValid(pins, count, clockMask);
}

/// Raw table entry (SpiIsr8/16/32): it may not set and clear the same pin, and
/// may not touch the clock, which the engine drives itself.
inline bool spiLutEntryValid(u32 setMask, u32 clearMask, u32 clockMask) FL_NO_EXCEPT {
    return (setMask & clearMask) == 0 && ((setMask | clearMask) & clockMask) == 0;
}

/// Raw clock mask (SpiIsr8/16/32): exactly one pin, and not one that any of the
/// `count` loaded table entries sets or clears.
inline bool spiClockMaskValid(u32 clockMask, const PinMaskEntry* lut, fl::size count) FL_NO_EXCEPT {
    if (!spiIsSinglePinMask(clockMask)) {
        return false;
    }
    for (fl::size v = 0; v < count; ++v) {
        if ((lut[v].set_mask | lut[v].clear_mask) & clockMask) {
            return false;
        }
    }
    return true;
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
