/// @file parlio_timing.h
/// @brief PARLIO timing generator for LED chipsets
///
/// This file provides compile-time generation of PARLIO bitpatterns and clock
/// frequencies based on LED chipset timing specifications (T1, T2, T3).
///
/// The timing generator calculates optimal clock frequencies and generates
/// waveform patterns that match the chipset's timing requirements within
/// the constraints of the ESP32-P4 PARLIO peripheral.

#pragma once

#include "fl/stdint.h"
#include "fl/chipsets/led_timing.h"
#include "fl/compiler_control.h"

namespace fl {
namespace parlio_detail {

/// @brief PARLIO timing configuration and bitpattern generator
///
/// This trait generates:
/// - Clock frequency matching chipset timing
/// - 16-entry lookup table mapping 4-bit values to waveform patterns
/// - Compile-time validation of timing feasibility
///
/// @tparam CHIPSET Chipset timing trait (must have T1, T2, T3 enum members in ns)
template<typename CHIPSET>
struct ParlioTimingGenerator {
    // Extract timing values from chipset (in nanoseconds)
    static constexpr uint32_t T1_NS = CHIPSET::T1;  ///< T0H: High time for bit 0
    static constexpr uint32_t T2_NS = CHIPSET::T2;  ///< T1H-T0H: Additional high time for bit 1
    static constexpr uint32_t T3_NS = CHIPSET::T3;  ///< T0L: Low tail duration

    // Calculate total bit period
    static constexpr uint32_t TOTAL_PERIOD_NS = T1_NS + T2_NS + T3_NS;

    // Calculate high/low times for bit 0 and bit 1
    static constexpr uint32_t T0H_NS = T1_NS;           ///< Bit 0 high time
    static constexpr uint32_t T0L_NS = T2_NS + T3_NS;   ///< Bit 0 low time
    static constexpr uint32_t T1H_NS = T1_NS + T2_NS;   ///< Bit 1 high time
    static constexpr uint32_t T1L_NS = T3_NS;           ///< Bit 1 low time

    // PARLIO timing parameters
    // We want to divide each LED bit into N clock cycles for accurate timing
    // A good default is 4 clocks per LED bit (provides reasonable resolution)
    static constexpr uint32_t CLOCKS_PER_BIT = 4;

    // Calculate required clock frequency
    // Clock freq = CLOCKS_PER_BIT / TOTAL_PERIOD (in Hz)
    // Example: WS2812 @ 1250ns → 4 / 1250e-9 = 3.2 MHz
    static constexpr uint32_t CLOCK_FREQ_HZ =
        (CLOCKS_PER_BIT * 1000000000ULL) / TOTAL_PERIOD_NS;

    // Calculate clock period in nanoseconds
    static constexpr uint32_t CLOCK_PERIOD_NS =
        (1000000000ULL + CLOCK_FREQ_HZ / 2) / CLOCK_FREQ_HZ;  // Round for accuracy

    // Calculate high/low durations in clock cycles (rounded)
    static constexpr uint32_t T0H_CLOCKS = (T0H_NS + CLOCK_PERIOD_NS / 2) / CLOCK_PERIOD_NS;
    static constexpr uint32_t T0L_CLOCKS = CLOCKS_PER_BIT - T0H_CLOCKS;
    static constexpr uint32_t T1H_CLOCKS = (T1H_NS + CLOCK_PERIOD_NS / 2) / CLOCK_PERIOD_NS;
    static constexpr uint32_t T1L_CLOCKS = CLOCKS_PER_BIT - T1H_CLOCKS;

    /// @brief Validate timing parameters at compile time
    ///
    /// Checks:
    /// - Clock frequency is achievable by PARLIO (1 MHz - 20 MHz typical range)
    /// - Timing resolution is adequate (at least 1 clock per timing phase)
    /// - Patterns fit within 4-bit encoding
    static constexpr bool validate_timing() {
        // Check clock frequency range (PARLIO supports ~1-40 MHz)
        if (CLOCK_FREQ_HZ < 1000000 || CLOCK_FREQ_HZ > 40000000) {
            return false;  // Clock frequency out of range
        }

        // Check that we have at least 1 clock for each phase
        if (T0H_CLOCKS < 1 || T0L_CLOCKS < 1 ||
            T1H_CLOCKS < 1 || T1L_CLOCKS < 1) {
            return false;  // Insufficient timing resolution
        }

        // Check that timing fits within 4-bit pattern (max 4 clocks)
        if (T0H_CLOCKS > CLOCKS_PER_BIT || T1H_CLOCKS > CLOCKS_PER_BIT) {
            return false;  // Pattern too long
        }

        return true;
    }

    /// @brief Generate 4-bit pattern for a single LED bit
    ///
    /// Creates a pattern where 1=high, 0=low for each clock cycle.
    /// Pattern is read LSB-first (bit 0 is first clock cycle).
    ///
    /// @param bit_value LED bit value (0 or 1)
    /// @return 4-bit pattern for PARLIO transmission
    static constexpr uint16_t generate_bit_pattern(uint8_t bit_value) {
        uint16_t pattern = 0;
        uint32_t high_clocks = bit_value ? T1H_CLOCKS : T0H_CLOCKS;

        // Set bits to 1 for high duration
        for (uint32_t i = 0; i < high_clocks && i < CLOCKS_PER_BIT; ++i) {
            pattern |= (1 << i);
        }

        return pattern;
    }

    /// @brief Generate bitpattern lookup table entry
    ///
    /// Each 4-bit nibble represents 1 LED bit encoded as CLOCKS_PER_BIT pattern.
    /// A 4-bit value (0-15) encodes 4 consecutive LED bits.
    ///
    /// @param nibble 4-bit value (0-15)
    /// @return 16-bit pattern encoding 4 LED bits
    static constexpr uint16_t generate_nibble_pattern(uint8_t nibble) {
        uint16_t pattern = 0;

        // Process each of 4 LED bits in the nibble (MSB-first)
        for (int bit_pos = 3; bit_pos >= 0; --bit_pos) {
            uint8_t led_bit = (nibble >> bit_pos) & 1;
            uint16_t bit_pattern = generate_bit_pattern(led_bit);

            // Shift and combine (LSB of pattern is first in time)
            pattern = (pattern << CLOCKS_PER_BIT) | bit_pattern;
        }

        return pattern;
    }

    /// @brief Generate complete bitpattern lookup table
    ///
    /// Creates a 16-entry table where each entry maps a 4-bit nibble to
    /// its corresponding 16-bit waveform pattern.
    ///
    /// @param patterns Output array (must have 16 entries)
    static constexpr void generate_bitpatterns(uint16_t (&patterns)[16]) {
        for (uint8_t i = 0; i < 16; ++i) {
            patterns[i] = generate_nibble_pattern(i);
        }
    }

    // Compile-time validation
    static_assert(validate_timing(),
        "CHIPSET timing cannot be accurately represented with PARLIO clock constraints. "
        "The chipset may be too fast or too slow for PARLIO's capabilities.");
};

/// @brief Helper to get bitpatterns for a chipset at compile time
///
/// This creates a static constexpr array that can be used for waveform generation.
///
/// @tparam CHIPSET Chipset timing trait
template<typename CHIPSET>
struct ParlioChipsetBitpatterns {
    static constexpr uint16_t patterns[16] = {
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(0),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(1),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(2),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(3),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(4),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(5),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(6),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(7),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(8),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(9),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(10),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(11),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(12),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(13),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(14),
        ParlioTimingGenerator<CHIPSET>::generate_nibble_pattern(15)
    };
};

// Template static member definition
template<typename CHIPSET>
constexpr uint16_t ParlioChipsetBitpatterns<CHIPSET>::patterns[16];

} // namespace parlio_detail
} // namespace fl
