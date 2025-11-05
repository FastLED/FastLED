/// @file platforms/arm/rp/fast_pins_rp.h
/// @brief RP2040/RP2350-specific implementation of FastPins using SIO registers
///
/// This implementation uses the RP2040/RP2350's Single-Cycle I/O (SIO) hardware
/// registers for atomic multi-pin operations. The SIO provides dedicated SET, CLEAR,
/// and TOGGLE registers for GPIO control with zero read-modify-write overhead.
///
/// Hardware Registers:
/// - sio_hw->gpio_set: Write 1 to set pins HIGH (W1S)
/// - sio_hw->gpio_clr: Write 1 to clear pins LOW (W1C)
/// - sio_hw->gpio_togl: Write 1 to toggle pins (not used here)
///
/// Pin Support:
/// - RP2040: 30 pins (0-29)
/// - RP2350: up to 48 pins (0-47)
///
/// Performance: ~30ns per write (direct hardware register access)

#pragma once

#include "hardware/structs/sio.h"
#include "fl/stdint.h"

namespace fl {

namespace detail {

/// Runtime pin mask lookup helper for RP2040/RP2350
/// Generates mask as (1 << pin) for single GPIO bank
inline uint32_t getPinMaskRP(uint8_t pin) {
    // RP2040 has 30 pins (0-29), RP2350 has up to 48 pins (0-47)
    // All fit in single 32-bit mask (well, RP2350 needs 48 bits but we use 32-bit for now)
    if (pin >= 32) return 0;  // Safety check
    return 1UL << pin;
}

} // namespace detail

/// RP2040/RP2350 implementation of FastPins::writeImpl()
/// Uses Pico SDK's SIO hardware structure for atomic GPIO operations
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Use Single-Cycle I/O (SIO) hardware registers
    // These are write-only registers - writing 1 sets/clears the corresponding pin
    sio_hw->gpio_set = set_mask;
    sio_hw->gpio_clr = clear_mask;
}

/// RP2040/RP2350 implementation of FastPins::buildLUT()
/// Constructs lookup table mapping bit patterns to GPIO masks
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Extract pin masks for each configured pin
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskRP(pins[i]);
    }

    // Build LUT: For each possible bit pattern (0 to 2^count - 1)
    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        // Map each bit in pattern to corresponding pin mask
        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                set_mask |= pin_masks[bit];
            } else {
                clear_mask |= pin_masks[bit];
            }
        }

        mLUT[pattern].set_mask = set_mask;
        mLUT[pattern].clear_mask = clear_mask;
    }

    // Fill remaining LUT entries (if any) with zeros
    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        mLUT[pattern].set_mask = 0;
        mLUT[pattern].clear_mask = 0;
    }
}

} // namespace fl
