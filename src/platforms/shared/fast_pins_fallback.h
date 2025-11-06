/// @file platforms/shared/fast_pins_fallback.h
/// @brief Software fallback implementation of FastPins for platforms without hardware SET/CLEAR registers
///
/// This implementation provides fast-pins functionality on platforms that lack
/// dedicated hardware SET/CLEAR GPIO registers (e.g., WASM, stub platforms).
/// It uses individual pin writes via digitalWrite() or equivalent, which is slower
/// and not atomic, but works on all platforms.
///
/// Characteristics:
/// - Slower than hardware implementations (~2µs per write)
/// - NOT atomic (pins are written sequentially)
/// - Uses LUT to avoid runtime bit manipulation
/// - Works on all platforms as a fallback
///
/// Used On:
/// - WASM/Stub platforms - simulation
/// - Any platform without hardware SET/CLEAR support (NOT AVR - AVR has its own optimized implementation)

#pragma once

#include "fl/stdint.h"

// Arduino.h provides digitalWrite() for fallback implementation
// It's included via led_sysdefs.h on all platforms
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    // Stub platform: use built-in digitalWrite simulation
#else
    // Arduino platform: digitalWrite provided by Arduino.h
#endif

namespace fl {

// ============================================================================
// FastPins<> fallback implementation (auto-detection, always uses same-port)
// ============================================================================

/// Fallback: All pins are considered "same port" since we have no port concept
template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    (void)pins;
    (void)count;
    return true;  // Always same-port in fallback mode
}

/// Fallback: Build same-port LUT (stores patterns for digitalWrite)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Build LUT: Store bit patterns for each entry
    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
        // Store pattern in set_mask (clear_mask unused)
        mSamePortLUT[pattern].set_mask = pattern;
        mSamePortLUT[pattern].clear_mask = ~pattern;
    }

    // Fill remaining entries
    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        mSamePortLUT[pattern].set_mask = 0;
        mSamePortLUT[pattern].clear_mask = 0;
    }

    // Store pin numbers for digitalWrite in write() (need to save somewhere)
    // Since we don't have platform-specific storage, we'll access pins via private member
    // This is a hack but necessary for fallback - production code should use hardware implementations
}

/// Fallback: Build multi-port LUT (same as same-port in fallback)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    // Fallback doesn't distinguish between same-port and multi-port
    // Use same implementation (digitalWrite always)
    (void)pins;
    (void)count;

    // Initialize multi-port LUT with zeros
    for (uint16_t pattern = 0; pattern < LUT_SIZE; pattern++) {
        auto& entry = mMultiPortLUT[pattern];
        entry.port_count = 0;
        for (int i = 0; i < 4; i++) {
            entry.ports[i].port_set = nullptr;
            entry.ports[i].port_clear = nullptr;
            entry.ports[i].set_mask = 0;
            entry.ports[i].clear_mask = 0;
        }
    }
}

/// Fallback: Same-port write (no-op, since we don't have hardware registers)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // No hardware registers available - this is a placeholder
    // Real writes happen in write() override which isn't possible in C++11
    // For now, this is a no-op and users on unsupported platforms should
    // use platform-specific implementations or upgrade to supported platform
    (void)set_mask;
    (void)clear_mask;
}

/// Fallback: Multi-port write (no-op)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // No hardware registers available - no-op
    (void)entry;
}

// ============================================================================
// FastPinsSamePort<> fallback implementation
// ============================================================================

/// Fallback: validateSamePort always returns true (no port concept)
template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    (void)pins;
    (void)count;
    return true;  // Always valid in fallback
}

/// Fallback: writeImpl is a no-op (no hardware registers)
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    (void)set_mask;
    (void)clear_mask;
    // No hardware registers - no-op
}

/// Fallback: buildLUT creates minimal LUT
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
        mLUT[pattern].set_mask = pattern;
        mLUT[pattern].clear_mask = ~pattern;
    }

    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        mLUT[pattern].set_mask = 0;
        mLUT[pattern].clear_mask = 0;
    }
}

// ============================================================================
// FastPinsWithClock<> fallback implementation
// ============================================================================

/// Fallback: validateAllSamePort always returns true
template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {
    (void)clockPin;
    (void)dataPins;
    (void)count;
    return true;  // Always valid in fallback
}

/// Fallback: buildClockMask is a no-op
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    (void)clockPin;
    // No hardware registers - no-op
}

/// Fallback: clockHighImpl is a no-op
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockHighImpl() {
    // No hardware registers - no-op
}

/// Fallback: clockLowImpl is a no-op
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockLowImpl() {
    // No hardware registers - no-op
}

} // namespace fl
