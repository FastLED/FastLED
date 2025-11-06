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

// Need FL_WARN for validation warnings
#include "fl/warn.h"

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

// ============================================================================
// FastPinsSamePort<> implementations for ESP32
// ============================================================================

/// ESP32 same-port validation for FastPinsSamePort
/// Validates that all pins are in the same GPIO bank (0-31 or 32-63)
template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // ESP32: All pins must be in same bank (0-31 or 32-63)
    uint8_t first_bank = detail::getPinBank(pins[0]);
    for (uint8_t i = 1; i < count; i++) {
        uint8_t bank = detail::getPinBank(pins[i]);
        if (bank != first_bank) {
            FL_WARN("FastPinsSamePort: Pins span multiple GPIO banks - not all on same port!");
            return false;  // Cross-bank not allowed in same-port mode
        }
    }
    return true;
}

/// ESP32 same-port implementation for FastPinsSamePort::writeImpl()
/// Uses single-bank W1TS/W1TC for atomic writes
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
#ifndef GPIO_OUT1_REG
    // Single GPIO bank (ESP32-C2/C3/C6/H2)
    volatile uint32_t* w1ts = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TS_REG;
    volatile uint32_t* w1tc = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TC_REG;

    *w1ts = set_mask;
    *w1tc = clear_mask;
#else
    // Dual GPIO banks (ESP32/S2/S3/P4)
    // Use bank stored during buildLUT
    if (mBank == 0) {
        volatile uint32_t* w1ts = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TS_REG;
        volatile uint32_t* w1tc = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TC_REG;
        *w1ts = set_mask;
        *w1tc = clear_mask;
    } else {
        volatile uint32_t* w1ts = (volatile uint32_t*)(uintptr_t)GPIO_OUT1_W1TS_REG;
        volatile uint32_t* w1tc = (volatile uint32_t*)(uintptr_t)GPIO_OUT1_W1TC_REG;
        *w1ts = set_mask;
        *w1tc = clear_mask;
    }
#endif
}

/// ESP32 implementation for FastPinsSamePort::buildLUT()
/// Builds 256-entry LUT with single-bank masks
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Determine bank (validation ensures all same bank)
#ifdef GPIO_OUT1_REG
    mBank = detail::getPinBank(pins[0]);
#endif

    // Extract pin masks (adjusted for bank)
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

// ============================================================================
// FastPinsWithClock<> implementations for ESP32
// ============================================================================

/// ESP32 validation for FastPinsWithClock
/// Validates that all 9 pins (8 data + 1 clock) are in the same GPIO bank
template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {
    // Check that clock pin is in same bank as data pins
    uint8_t clock_bank = detail::getPinBank(clockPin);

    for (uint8_t i = 0; i < count; i++) {
        uint8_t data_bank = detail::getPinBank(dataPins[i]);
        if (data_bank != clock_bank) {
            FL_WARN("FastPinsWithClock: Clock and data pins must be on same GPIO bank!");
            return false;
        }
    }

    return true;
}

/// ESP32 implementation for FastPinsWithClock::buildClockMask()
/// Stores clock pin's register addresses and mask
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Extract clock pin mask
    mClockMask = detail::getPinMaskESP32(clockPin);

    // Determine which bank the clock pin is in
    uint8_t clock_bank = detail::getPinBank(clockPin);

#ifndef GPIO_OUT1_REG
    // Single GPIO bank (ESP32-C2/C3/C6/H2)
    mClockSet = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TS_REG;
    mClockClear = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TC_REG;
#else
    // Dual GPIO banks (ESP32/S2/S3/P4)
    if (clock_bank == 0) {
        mClockSet = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TS_REG;
        mClockClear = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TC_REG;
    } else {
        mClockSet = (volatile uint32_t*)(uintptr_t)GPIO_OUT1_W1TS_REG;
        mClockClear = (volatile uint32_t*)(uintptr_t)GPIO_OUT1_W1TC_REG;
    }
#endif
}

/// ESP32 implementation for FastPinsWithClock::clockHighImpl()
/// Sets clock pin HIGH using W1TS register (~5ns)
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockHighImpl() {
    *mClockSet = mClockMask;
}

/// ESP32 implementation for FastPinsWithClock::clockLowImpl()
/// Sets clock pin LOW using W1TC register (~5ns)
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockLowImpl() {
    *mClockClear = mClockMask;
}

// ============================================================================
// FastPins<> multi-port implementations for ESP32
// ============================================================================

/// ESP32: Check if all pins are on same GPIO bank
template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    uint8_t first_bank = detail::getPinBank(pins[0]);
    for (uint8_t i = 1; i < count; i++) {
        if (detail::getPinBank(pins[i]) != first_bank) {
            return false;  // Cross-bank detected
        }
    }
    return true;
}

/// ESP32: Build same-port LUT (reuse FastPinsSamePort logic)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskESP32(pins[i]);
    }

    // Build LUT
    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                set_mask |= pin_masks[bit];
            } else {
                clear_mask |= pin_masks[bit];
            }
        }

        mSamePortLUT[pattern].set_mask = set_mask;
        mSamePortLUT[pattern].clear_mask = clear_mask;
    }

    // Fill remaining entries with zeros
    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        mSamePortLUT[pattern].set_mask = 0;
        mSamePortLUT[pattern].clear_mask = 0;
    }
}

/// ESP32: Build multi-port LUT for cross-bank operation
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

#ifdef GPIO_OUT1_REG
    // Dual-bank ESP32 - build per-bank LUT

    // Categorize pins by bank
    struct PinInfo {
        uint8_t bank;
        uint32_t mask;
    };
    PinInfo pin_info[MAX_PINS];

    for (uint8_t i = 0; i < count; i++) {
        pin_info[i].bank = detail::getPinBank(pins[i]);
        pin_info[i].mask = detail::getPinMaskESP32(pins[i]);
    }

    // Get register addresses for both banks
    volatile uint32_t* w1ts_bank0 = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TS_REG;
    volatile uint32_t* w1tc_bank0 = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TC_REG;
    volatile uint32_t* w1ts_bank1 = (volatile uint32_t*)(uintptr_t)GPIO_OUT1_W1TS_REG;
    volatile uint32_t* w1tc_bank1 = (volatile uint32_t*)(uintptr_t)GPIO_OUT1_W1TC_REG;

    // Build LUT with per-bank masks
    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
        // Calculate masks for each bank
        uint32_t bank0_set = 0, bank0_clear = 0;
        uint32_t bank1_set = 0, bank1_clear = 0;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                // Bit is HIGH
                if (pin_info[bit].bank == 0) {
                    bank0_set |= pin_info[bit].mask;
                } else {
                    bank1_set |= pin_info[bit].mask;
                }
            } else {
                // Bit is LOW
                if (pin_info[bit].bank == 0) {
                    bank0_clear |= pin_info[bit].mask;
                } else {
                    bank1_clear |= pin_info[bit].mask;
                }
            }
        }

        // Store in LUT (using first 2 port entries)
        auto& entry = mMultiPortLUT[pattern];
        entry.port_count = 2;  // Always use both banks for simplicity

        // Bank 0
        entry.ports[0].port_set = (void*)w1ts_bank0;
        entry.ports[0].port_clear = (void*)w1tc_bank0;
        entry.ports[0].set_mask = bank0_set;
        entry.ports[0].clear_mask = bank0_clear;

        // Bank 1
        entry.ports[1].port_set = (void*)w1ts_bank1;
        entry.ports[1].port_clear = (void*)w1tc_bank1;
        entry.ports[1].set_mask = bank1_set;
        entry.ports[1].clear_mask = bank1_clear;

        // Unused ports
        entry.ports[2].port_set = nullptr;
        entry.ports[3].port_set = nullptr;
    }

    // Fill remaining entries with zeros
    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        auto& entry = mMultiPortLUT[pattern];
        entry.port_count = 0;
        for (int i = 0; i < 4; i++) {
            entry.ports[i].port_set = nullptr;
            entry.ports[i].port_clear = nullptr;
            entry.ports[i].set_mask = 0;
            entry.ports[i].clear_mask = 0;
        }
    }
#else
    // Single-bank ESP32 - shouldn't reach here, but handle gracefully
    buildSamePortLUT(pins, count);
#endif
}

/// ESP32: Same-port write implementation
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Reuse FastPinsSamePort logic - write to bank 0 by default
    volatile uint32_t* w1ts = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TS_REG;
    volatile uint32_t* w1tc = (volatile uint32_t*)(uintptr_t)GPIO_OUT_W1TC_REG;

    *w1ts = set_mask;
    *w1tc = clear_mask;
}

/// ESP32: Multi-port write implementation
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // Write to each port in sequence
    for (uint8_t i = 0; i < entry.port_count && i < 4; i++) {
        if (entry.ports[i].port_set != nullptr) {
            volatile uint32_t* w1ts = (volatile uint32_t*)entry.ports[i].port_set;
            volatile uint32_t* w1tc = (volatile uint32_t*)entry.ports[i].port_clear;

            *w1ts = entry.ports[i].set_mask;
            *w1tc = entry.ports[i].clear_mask;
        }
    }
}

} // namespace fl
