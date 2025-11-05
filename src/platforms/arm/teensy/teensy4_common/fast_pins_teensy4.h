/// @file platforms/arm/teensy/teensy4_common/fast_pins_teensy4.h
/// @brief Teensy 4.x-specific implementation of FastPins using GPIO_DR_SET/CLEAR registers
///
/// This implementation uses Teensy 4.x (MXRT1062) GPIO Data Register SET/CLEAR
/// registers for atomic multi-pin operations. The MXRT1062 has 9 GPIO ports
/// (GPIO1-GPIO9), each with dedicated SET, CLEAR, and TOGGLE registers.
///
/// Hardware Registers (per GPIO port):
/// - GPIOx_DR: Data register (full read/write)
/// - GPIOx_DR_SET: Write 1 to set pins HIGH (W1S)
/// - GPIOx_DR_CLEAR: Write 1 to clear pins LOW (W1C)
/// - GPIOx_DR_TOGGLE: Write 1 to toggle pins
///
/// Teensy Pin Mapping:
/// - Teensy uses "fast" GPIO ports (GPIO6-GPIO9) for most pins
/// - Each pin maps to a specific GPIO port + bit position
/// - See fastpin_arm_mxrt1062.h for complete pin mapping table
///
/// Limitations:
/// - Current implementation assumes all pins are on GPIO6 (most common)
/// - Cross-port scenarios require enhanced LUT with per-port tracking (TODO)
///
/// Performance: ~35ns per write (direct hardware register access)

#pragma once

#include "fl/stdint.h"

namespace fl {

namespace detail {

/// Runtime pin mask lookup helper for Teensy 4.x
/// NOTE: This simplified version assumes pin number = bit position within GPIO6
/// Real implementation would need full pin-to-GPIO-port mapping from FastPin<>
inline uint32_t getPinMaskTeensy4(uint8_t pin) {
    // Teensy 4.0/4.1 has up to 40 pins mapping to various GPIO ports
    // For simplicity, use pin as bit position (pins 0-31)
    // TODO: Proper pin-to-GPIO-port mapping using FastPin<>::pinbit()
    if (pin >= 32) return 0;  // Safety check
    return 1UL << pin;
}

} // namespace detail

/// Teensy 4.x implementation of FastPins::writeImpl()
/// Uses GPIO_DR_SET/GPIO_DR_CLEAR registers for atomic operations
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // TODO: This simplified version writes to GPIO6 only
    // Full implementation needs to:
    // 1. Determine which GPIO port(s) the pins belong to (GPIO1-GPIO9)
    // 2. Issue separate SET/CLEAR writes per port
    // 3. Store port information in enhanced LUT structure

    // Assume all pins on GPIO6 (Teensy's "fast" port for many pins)
    // GPIO6_DR_SET and GPIO6_DR_CLEAR are defined in Teensy core
    GPIO6_DR_SET = set_mask;
    GPIO6_DR_CLEAR = clear_mask;
}

/// Teensy 4.x implementation of FastPins::buildLUT()
/// Constructs lookup table mapping bit patterns to GPIO masks
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Extract pin masks for each configured pin
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskTeensy4(pins[i]);
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
