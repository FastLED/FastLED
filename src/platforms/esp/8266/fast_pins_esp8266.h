/// @file platforms/esp/8266/fast_pins_esp8266.h
/// @brief ESP8266-specific implementation of FastPins using GPOS/GPOC registers
///
/// This implementation uses ESP8266's GPIO Output Set (GPOS) and GPIO Output Clear (GPOC)
/// registers for atomic multi-pin operations. These registers allow setting or clearing
/// multiple GPIO pins with a single 32-bit write, with zero read-modify-write overhead.
///
/// Register Addresses:
/// - GPIO Output Register: 0x60000300 (_GPO)
/// - GPIO Output Set: 0x60000304 (_GPOS) - Write 1 to set pin HIGH
/// - GPIO Output Clear: 0x60000308 (_GPOC) - Write 1 to set pin LOW
///
/// GPIO Architecture:
/// - Single GPIO bank (pins 0-15)
/// - Pin 16 is special (separate control register GP16O)
/// - Pins 6-11 are typically reserved for flash (SPI)
/// - Usable pins: 0-5, 12-16 (10 pins total on most boards)
///
/// Performance: ~30ns per write (direct MMIO, zero overhead)
///
/// Important Notes:
/// - Pin 16 cannot be used with FastPins (no atomic set/clear)
/// - Avoid pins 6-11 (flash interface, causes crashes)
/// - For multi-pin operations, all pins must be 0-15 (same bank)

#pragma once

#include "platforms/esp/8266/fastpin_esp8266.h"
#include "fl/stdint.h"

// Need FL_WARN for validation warnings
#include "fl/warn.h"

namespace fl {

namespace detail {

/// Runtime pin mask lookup helper for ESP8266
/// ESP8266 has pins 0-15 (+ special pin 16)
inline uint32_t getPinMaskESP8266(uint8_t pin) {
    // ESP8266: pins 0-15 use bit position
    if (pin >= 16) return 0;  // Pin 16 not supported for FastPins
    return 1UL << pin;
}

/// Check if pin is valid for FastPins on ESP8266
inline bool isValidPinESP8266(uint8_t pin) {
    // Pin 16 is not supported (no atomic set/clear)
    if (pin >= 16) return false;

    // Warn about flash pins (6-11) but don't block
    // Users might be using custom flash configurations
    if (pin >= 6 && pin <= 11) {
        FL_WARN("FastPins: Pin %d is typically reserved for flash interface!", pin);
    }

    return true;
}

} // namespace detail

// ============================================================================
// FastPinsSamePort<> implementations for ESP8266
// ============================================================================

/// ESP8266 same-port validation for FastPinsSamePort
/// All ESP8266 pins 0-15 are on the same GPIO bank
template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // ESP8266: All usable pins (0-15) are on single GPIO bank
    // Only need to validate that no pin is >= 16
    for (uint8_t i = 0; i < count; i++) {
        if (!detail::isValidPinESP8266(pins[i])) {
            FL_WARN("FastPinsSamePort: Pin %d is not valid for FastPins!", pins[i]);
            return false;
        }
    }
    return true;
}

/// ESP8266 same-port implementation for FastPinsSamePort::writeImpl()
/// Uses GPOS/GPOC registers for atomic writes
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // ESP8266: Use GPOS (set) and GPOC (clear) registers
    // These are atomic write operations (no read-modify-write)
    _GPB._GPOS = set_mask;   // Set HIGH pins
    _GPB._GPOC = clear_mask;  // Set LOW pins
}

/// Build LUT for ESP8266
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskESP8266(pins[i]);
    }

    // Build 256-entry LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                set_mask |= pin_masks[bit];    // Pin should be HIGH
            } else {
                clear_mask |= pin_masks[bit];  // Pin should be LOW
            }
        }

        mLUT[pattern].set_mask = set_mask;
        mLUT[pattern].clear_mask = clear_mask;
    }
}

// ============================================================================
// FastPinsWithClock<> implementations for ESP8266
// ============================================================================

/// ESP8266 validation for FastPinsWithClock
/// All pins (data + clock) must be 0-15
template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {
    // Validate clock pin
    if (!detail::isValidPinESP8266(clockPin)) {
        FL_WARN("FastPinsWithClock: Clock pin %d is not valid!", clockPin);
        return false;
    }

    // Validate data pins
    for (uint8_t i = 0; i < count; i++) {
        if (!detail::isValidPinESP8266(dataPins[i])) {
            FL_WARN("FastPinsWithClock: Data pin %d is not valid!", dataPins[i]);
            return false;
        }
    }

    return true;
}

/// Build clock mask for ESP8266
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Store clock mask
    mClockMask = detail::getPinMaskESP8266(clockPin);

    // Store register pointers for clock control
    // ESP8266: Use GPOS/GPOC for atomic set/clear
    mClockSet = &_GPB._GPOS;
    mClockClear = &_GPB._GPOC;
}

// ============================================================================
// FastPins<> implementations for ESP8266 (auto-detecting mode)
// ============================================================================

/// ESP8266 validation for FastPins<> - check if all pins are on same port
/// (Always true for ESP8266 since there's only one GPIO bank for pins 0-15)
template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // ESP8266: All pins 0-15 are on single bank
    // Just validate that all pins are valid
    for (uint8_t i = 0; i < count; i++) {
        if (!detail::isValidPinESP8266(pins[i])) {
            return false;  // Invalid pin
        }
    }
    return true;  // All valid pins are on same bank
}

/// ESP8266 same-port write implementation for FastPins<>
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Same as FastPinsSamePort
    _GPB._GPOS = set_mask;
    _GPB._GPOC = clear_mask;
}

/// ESP8266 same-port LUT builder for FastPins<>
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskESP8266(pins[i]);
    }

    // Build 256-entry LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
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
}

/// ESP8266 multi-port write implementation
/// Note: ESP8266 only has one GPIO bank, so this is never actually used
/// Included for API completeness
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // ESP8266: All pins are on same bank, but implement for completeness
    for (uint8_t i = 0; i < entry.port_count; i++) {
        volatile uint32_t* set_reg = (volatile uint32_t*)entry.ports[i].port_set;
        volatile uint32_t* clear_reg = (volatile uint32_t*)entry.ports[i].port_clear;

        *set_reg = entry.ports[i].set_mask;
        *clear_reg = entry.ports[i].clear_mask;
    }
}

/// ESP8266 multi-port LUT builder
/// Note: ESP8266 only has one GPIO bank, so this treats everything as same-bank
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskESP8266(pins[i]);
    }

    // Build 256-entry LUT
    // ESP8266: All pins use same bank (GPOS/GPOC)
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                set_mask |= pin_masks[bit];
            } else {
                clear_mask |= pin_masks[bit];
            }
        }

        // Store in multi-port LUT format (but only one port)
        mMultiPortLUT[pattern].port_count = 1;
        mMultiPortLUT[pattern].ports[0].port_set = &_GPB._GPOS;
        mMultiPortLUT[pattern].ports[0].port_clear = &_GPB._GPOC;
        mMultiPortLUT[pattern].ports[0].set_mask = set_mask;
        mMultiPortLUT[pattern].ports[0].clear_mask = clear_mask;
    }
}

} // namespace fl
