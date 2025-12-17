/// @file led_timing_legacy_avr.h
/// @brief Legacy AVR-specific timing definitions in CPU clock cycles
///
/// This file preserves the original AVR timing specifications based on
/// CPU clock cycles (FMUL multiplier) rather than nanoseconds.
///
/// These definitions are used by the AVR clockless driver (clockless_avr.h)
/// which requires cycle-accurate timing for bit manipulation.
///
/// @note These timings are for 8/16/24 MHz frequencies and use FMUL multipliers.
/// For nanosecond-based timings, see fl/chipsets/led_timing.h

#pragma once

#include "fl/stl/stdint.h"

namespace fl {

// ============================================================================
// AVR Frequency Multiplier System
// ============================================================================
//
// For AVR platforms running at 8MHz, 16MHz, or 24MHz:
// FMUL = CLOCKLESS_FREQUENCY / 8000000
//
// At 8MHz:  FMUL = 1 (125ns per cycle)
// At 16MHz: FMUL = 2 (62.5ns per cycle)
// At 24MHz: FMUL = 3 (41.67ns per cycle)
//
// These are converted from nanosecond values using:
//   cycles = nanoseconds * FMUL / 125

// ============================================================================
// Legacy AVR Timing Definitions (in clock cycle multipliers)
// ============================================================================

/// @brief Generic AVR timing entry using FMUL multiplier system
/// All values are multiples of FMUL for 8/16/24 MHz frequencies
struct AVRChipsetTimingLegacy {
    uint32_t T1;        ///< High time for bit 0 (in FMUL units)
    uint32_t T2;        ///< Additional high time for bit 1 (in FMUL units)
    uint32_t T3;        ///< Low tail duration (in FMUL units)
    const char* name;   ///< Human-readable chipset name
};

// ============================================================================
// Fast-Speed AVR Chipsets (800kHz - 1600kHz equivalent)
// ============================================================================

/// GE8822 @ 800 kHz
/// FMUL-based: 350ns/125 = 2.8 ≈ 3×FMUL, 660ns/125 = 5.28 ≈ 5×FMUL, 350ns/125 = 2.8 ≈ 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_GE8822_800KHZ = {3, 5, 3, "GE8822_800KHZ"};

/// WS2812 @ 800 kHz
/// FMUL-based: 250ns/125 = 2×FMUL, 625ns/125 = 5×FMUL, 375ns/125 = 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_WS2812_800KHZ = {2, 5, 3, "WS2812_800KHZ"};

/// WS2811 @ 800 kHz (fast variant)
/// FMUL-based: 320ns/125 = 2.56 ≈ 3×FMUL, 320ns/125 = 2.56 ≈ 4×FMUL (rounds up), 640ns/125 = 5.12 ≈ 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_WS2811_800KHZ = {3, 4, 3, "WS2811_800KHZ"};

/// WS2813 @ 800 kHz (same as WS2811)
/// FMUL-based: 320ns/125 = 2.56 ≈ 3×FMUL, 320ns/125 = 2.56 ≈ 4×FMUL, 640ns/125 = 5.12 ≈ 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_WS2813 = {3, 4, 3, "WS2813"};

/// SK6822 @ 800 kHz
/// FMUL-based: 375ns/125 = 3×FMUL, 1000ns/125 = 8×FMUL, 375ns/125 = 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_SK6822 = {3, 8, 3, "SK6822"};

/// SK6812 @ 800 kHz
/// FMUL-based: 300ns/125 = 2.4 ≈ 3×FMUL (rounds up), 600ns/125 = 4.8 ≈ 3×FMUL, 300ns/125 = 2.4 ≈ 4×FMUL (rounds up)
static constexpr AVRChipsetTimingLegacy AVR_TIMING_SK6812 = {3, 3, 4, "SK6812"};

/// SM16703 @ 800 kHz
/// FMUL-based: 300ns/125 = 2.4 ≈ 3×FMUL, 600ns/125 = 4.8 ≈ 4×FMUL, 300ns/125 = 2.4 ≈ 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_SM16703 = {3, 4, 3, "SM16703"};

/// UCS1903B @ 800 kHz
/// FMUL-based: 400ns/125 = 3.2 ≈ 2×FMUL, 450ns/125 = 3.6 ≈ 4×FMUL, 450ns/125 = 3.6 ≈ 4×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_UCS1903B_800KHZ = {2, 4, 4, "UCS1903B_800KHZ"};

/// UCS1904 @ 800 kHz
/// FMUL-based: 400ns/125 = 3.2 ≈ 3×FMUL (biased), 400ns/125 = 3.2 ≈ 3×FMUL, 450ns/125 = 3.6 ≈ 4×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_UCS1904_800KHZ = {3, 3, 4, "UCS1904_800KHZ"};

/// UCS2903 @ 800 kHz
/// FMUL-based: 250ns/125 = 2×FMUL, 750ns/125 = 6×FMUL, 250ns/125 = 2×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_UCS2903 = {2, 6, 2, "UCS2903"};

/// TM1809 @ 800 kHz
/// FMUL-based: 350ns/125 = 2.8 ≈ 2×FMUL, 350ns/125 = 2.8 ≈ 5×FMUL (combined), 450ns/125 = 3.6 ≈ 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_TM1809_800KHZ = {2, 5, 3, "TM1809_800KHZ"};

/// TM1829 @ 800 kHz
/// FMUL-based: 340ns/125 = 2.72 ≈ 2×FMUL, 340ns/125 = 2.72 ≈ 5×FMUL (combined), 550ns/125 = 4.4 ≈ 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_TM1829_800KHZ = {2, 5, 3, "TM1829_800KHZ"};

/// LPD1886 @ 1250 kHz
/// FMUL-based: 200ns/125 = 1.6 ≈ 2×FMUL, 400ns/125 = 3.2 ≈ 3×FMUL, 200ns/125 = 1.6 ≈ 2×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_LPD1886_1250KHZ = {2, 3, 2, "LPD1886_1250KHZ"};

/// PL9823 @ 800 kHz
/// FMUL-based: 350ns/125 = 2.8 ≈ 3×FMUL, 1010ns/125 = 8.08 ≈ 8×FMUL, 350ns/125 = 2.8 ≈ 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_PL9823 = {3, 8, 3, "PL9823"};

/// SM16824E high-speed variant
/// FMUL-based: 300ns/125 = 2.4 ≈ 3×FMUL, 900ns/125 = 7.2 ≈ 9×FMUL, 100ns/125 = 0.8 ≈ 1×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_SM16824E = {3, 9, 1, "SM16824E"};

// ============================================================================
// Medium-Speed AVR Chipsets (400kHz - 600kHz equivalent)
// ============================================================================

/// WS2811 @ 400 kHz (slow variant)
/// FMUL-based: 800ns/125 = 6.4 ≈ 4×FMUL, 800ns/125 = 6.4 ≈ 10×FMUL (combined), 900ns/125 = 7.2 ≈ 6×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_WS2811_400KHZ = {4, 10, 6, "WS2811_400KHZ"};

/// WS2815 @ 400 kHz
/// FMUL-based: 250ns/125 = 2×FMUL, 1090ns/125 = 8.72 ≈ 9×FMUL, 550ns/125 = 4.4 ≈ 4×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_WS2815 = {2, 9, 4, "WS2815"};

/// UCS1903 @ 400 kHz
/// FMUL-based: 500ns/125 = 4×FMUL, 1500ns/125 = 12×FMUL, 500ns/125 = 4×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_UCS1903_400KHZ = {4, 12, 4, "UCS1903_400KHZ"};

/// TM1803 @ 400 kHz
/// FMUL-based: 700ns/125 = 5.6 ≈ 6×FMUL, 1100ns/125 = 8.8 ≈ 9×FMUL, 700ns/125 = 5.6 ≈ 6×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_TM1803_400KHZ = {6, 9, 6, "TM1803_400KHZ"};

/// GW6205 @ 400 kHz
/// FMUL-based: 800ns/125 = 6.4 ≈ 6×FMUL, 800ns/125 = 6.4 ≈ 7×FMUL, 800ns/125 = 6.4 ≈ 6×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_GW6205_400KHZ = {6, 7, 6, "GW6205_400KHZ"};

/// GW6205 @ 800 kHz (fast variant)
/// FMUL-based: 400ns/125 = 3.2 ≈ 2×FMUL, 400ns/125 = 3.2 ≈ 4×FMUL, 400ns/125 = 3.2 ≈ 4×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_GW6205_800KHZ = {2, 4, 4, "GW6205_800KHZ"};

/// DP1903 @ 800 kHz
/// FMUL-based: 400ns/125 = 3.2 ≈ 2×FMUL, 1000ns/125 = 8×FMUL, 400ns/125 = 3.2 ≈ 2×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_DP1903_800KHZ = {2, 8, 2, "DP1903_800KHZ"};

/// DP1903 @ 400 kHz (slow variant)
/// FMUL-based: 800ns/125 = 6.4 ≈ 4×FMUL, 1600ns/125 = 12.8 ≈ 16×FMUL, 800ns/125 = 6.4 ≈ 4×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_DP1903_400KHZ = {4, 16, 4, "DP1903_400KHZ"};

/// UCS1912 @ 800 kHz
/// FMUL-based: 250ns/125 = 2×FMUL, 1000ns/125 = 8×FMUL, 350ns/125 = 2.8 ≈ 3×FMUL
static constexpr AVRChipsetTimingLegacy AVR_TIMING_UCS1912 = {2, 8, 3, "UCS1912"};

// ============================================================================
// Helper Functions
// ============================================================================

/// @brief Get total bit period in FMUL units
/// @param timing AVR chipset timing structure
/// @return Total bit period (T1 + T2 + T3) in FMUL units
constexpr uint32_t get_avr_bit_period_fmul(const AVRChipsetTimingLegacy& timing) {
    return timing.T1 + timing.T2 + timing.T3;
}

/// @brief Convert FMUL units to nanoseconds (approximate)
/// @param fmul_cycles FMUL multiplier value
/// @param frequency_hz CPU frequency in Hz
/// @return Approximate nanoseconds
constexpr uint32_t avr_fmul_to_ns(uint32_t fmul_cycles, uint32_t frequency_hz) {
    // At frequency_hz, each cycle = 1000000000 / frequency_hz nanoseconds
    // fmul represents cycles at 8MHz = 125ns per cycle
    // So: ns = fmul_cycles * 125 / FMUL = fmul_cycles * (8000000 / frequency_hz) * 125 / (frequency_hz / 8000000)
    //     ns = fmul_cycles * 1000000000 / frequency_hz
    return fmul_cycles * (1000000000UL / frequency_hz);
}

}  // namespace fl
