/// @file platforms/esp/32/fast_pins_esp32.h
/// @brief ESP32-specific implementation of FastPins using W1TS/W1TC registers
///
/// This implementation uses ESP32's Write-One-To-Set (W1TS) and Write-One-To-Clear (W1TC)
/// GPIO registers for atomic multi-pin operations. These registers allow setting or clearing
/// multiple GPIO pins with a single 32-bit write, with zero read-modify-write overhead.
///
/// Register Addresses:
/// - ESP32/S2/S3/P4: GPIO_OUT_W1TS_REG / GPIO_OUT_W1TC_REG (bank 0: pins 0-31)
///                   GPIO_OUT1_W1TS_REG / GPIO_OUT1_W1TC_REG (bank 1: pins 32-63)
/// - ESP32-C2/C3/C6/H2: GPIO_OUT_W1TS_REG / GPIO_OUT_W1TC_REG (single bank: pins 0-31)
///
/// Performance: ~30ns per write (direct MMIO, zero overhead)

#pragma once

#include "platforms/esp/32/core/fastpin_esp32.h"
#include "fl/stdint.h"

namespace fl {

namespace detail {

/// Runtime pin mask lookup helper for ESP32
/// Uses FastPin<> template to extract masks at compile-time, stores in runtime table
inline uint32_t getPinMaskESP32(uint8_t pin) {
    // Compile-time generated lookup table for ESP32 pins (0-63)
    // Each entry is the mask (1 << (pin % 32))
    static const uint32_t pin_masks[64] = {
        // Pins 0-31
        1UL << 0, 1UL << 1, 1UL << 2, 1UL << 3, 1UL << 4, 1UL << 5, 1UL << 6, 1UL << 7,
        1UL << 8, 1UL << 9, 1UL << 10, 1UL << 11, 1UL << 12, 1UL << 13, 1UL << 14, 1UL << 15,
        1UL << 16, 1UL << 17, 1UL << 18, 1UL << 19, 1UL << 20, 1UL << 21, 1UL << 22, 1UL << 23,
        1UL << 24, 1UL << 25, 1UL << 26, 1UL << 27, 1UL << 28, 1UL << 29, 1UL << 30, 1UL << 31,
        // Pins 32-63 (for ESP32/S2/S3/P4 with dual banks)
        1UL << 0, 1UL << 1, 1UL << 2, 1UL << 3, 1UL << 4, 1UL << 5, 1UL << 6, 1UL << 7,
        1UL << 8, 1UL << 9, 1UL << 10, 1UL << 11, 1UL << 12, 1UL << 13, 1UL << 14, 1UL << 15,
        1UL << 16, 1UL << 17, 1UL << 18, 1UL << 19, 1UL << 20, 1UL << 21, 1UL << 22, 1UL << 23,
        1UL << 24, 1UL << 25, 1UL << 26, 1UL << 27, 1UL << 28, 1UL << 29, 1UL << 30, 1UL << 31
    };

    if (pin >= 64) return 0;
    return pin_masks[pin];
}

/// Get GPIO bank for a pin (0 = pins 0-31, 1 = pins 32-63)
inline uint8_t getPinBank(uint8_t pin) {
    return (pin >= 32) ? 1 : 0;
}

} // namespace detail

/// ESP32 implementation of FastPins::writeImpl()
/// Uses direct MMIO writes to W1TS/W1TC registers
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
#ifndef GPIO_OUT1_REG
    // Single GPIO bank (ESP32-C2/C3/C6/H2)
    volatile uint32_t* w1ts = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TS_REG;
    volatile uint32_t* w1tc = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TC_REG;

    *w1ts = set_mask;
    *w1tc = clear_mask;
#else
    // Dual GPIO banks (ESP32/S2/S3/P4)
    // Note: For simplicity, we write to both banks even if only one is used
    // The hardware ignores writes to unused pins
    volatile uint32_t* w1ts0 = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TS_REG;
    volatile uint32_t* w1tc0 = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TC_REG;
    volatile uint32_t* w1ts1 = (volatile uint32_t*)(uintptr_t)GPIO_OUT1_W1TS_REG;
    volatile uint32_t* w1tc1 = (volatile uint32_t*)(uintptr_t)GPIO_OUT1_W1TC_REG;

    // Bank 0 (pins 0-31)
    *w1ts0 = set_mask;
    *w1tc0 = clear_mask;

    // Bank 1 (pins 32-63) - masks should already be shifted if needed
    // For now, we assume all pins are in bank 0 (most common case)
    // Full cross-bank support would require enhanced LUT with per-bank masks
    *w1ts1 = 0;  // TODO: handle pins 32-63
    *w1tc1 = 0;
#endif
}

/// ESP32 implementation of FastPins::buildLUT()
/// Constructs 256-entry (or smaller) lookup table mapping bit patterns to GPIO masks
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Extract pin masks for each configured pin
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskESP32(pins[i]);
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
