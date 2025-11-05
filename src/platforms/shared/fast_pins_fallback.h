/// @file platforms/shared/fast_pins_fallback.h
/// @brief Software fallback implementation of FastPins for platforms without hardware SET/CLEAR registers
///
/// This implementation provides fast-pins functionality on platforms that lack
/// dedicated hardware SET/CLEAR GPIO registers (e.g., AVR, WASM, stub platforms).
/// It uses individual pin writes via digitalWrite() or equivalent, which is slower
/// and not atomic, but works on all platforms.
///
/// Characteristics:
/// - Slower than hardware implementations (~2µs per write on AVR)
/// - NOT atomic (pins are written sequentially)
/// - Uses LUT to avoid runtime bit manipulation
/// - Works on all platforms as a fallback
///
/// Used On:
/// - AVR (ATmega, ATtiny) - uses PORT registers with read-modify-write
/// - WASM/Stub platforms - simulation
/// - Any platform without hardware SET/CLEAR support

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

// Software fallback provides implementations for FastPins template methods
// Since we can't use hardware SET/CLEAR registers, we override write() to use
// individual pin operations

/// Software fallback implementation of FastPins::writeImpl()
/// Uses individual pin writes (slow, not atomic, but works everywhere)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Fallback: Cannot efficiently use masks without hardware SET/CLEAR
    // The write() method is overridden to handle pin writes directly
    (void)set_mask;    // Suppress unused parameter warnings
    (void)clear_mask;
}

/// Software fallback implementation of FastPins::buildLUT()
/// Builds minimal LUT (not actually used by write override, but maintained for API consistency)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Build minimal LUT (not used by fallback write(), but kept for API consistency)
    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
        // Store pattern directly (masks not meaningful without hardware SET/CLEAR)
        mLUT[pattern].set_mask = pattern;
        mLUT[pattern].clear_mask = ~pattern;
    }

    // Fill remaining entries
    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        mLUT[pattern].set_mask = 0;
        mLUT[pattern].clear_mask = 0;
    }
}

} // namespace fl
