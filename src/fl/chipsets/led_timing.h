/// @file led_timing.h
/// @brief Centralized LED chipset timing definitions with nanosecond precision
///
/// This file provides unified timing definitions for all supported LED chipsets
/// using nanosecond-based T1, T2, T3 timing model (WS28xx three-phase protocol).
///
/// Timing Convention (per chipset):
///   At T=0        : the line is raised hi to start a bit
///   At T=T1       : the line is dropped low to transmit a zero bit (start of low time)
///   At T=T1+T2    : the line is dropped low to transmit a one bit (start of low time)
///   At T=T1+T2+T3 : the cycle is concluded (next bit can be sent)
///
/// All timings are specified in nanoseconds (ns) for clarity and portability.
/// Platform-specific drivers convert these to CPU cycles as needed.

#pragma once

#include "fl/namespace.h"
#include "fl/stdint.h"
#include "fl/compiler_control.h"

namespace fl {

// ============================================================================
// Centralized Nanosecond Timing Definitions
// ============================================================================

/// @brief Generic chipset timing entry
/// Provides T1, T2, T3 timing parameters in nanoseconds for any LED protocol
struct ChipsetTiming {
    uint32_t T1;        ///< High time for bit 0 (nanoseconds)
    uint32_t T2;        ///< Additional high time for bit 1 (nanoseconds)
    uint32_t T3;        ///< Low tail duration (nanoseconds)
    uint32_t RESET;     ///< Reset/latch time (microseconds)
    const char* name;   ///< Human-readable chipset name
};

// ============================================================================
// Fast-Speed Chipsets (800kHz - 1600kHz range)
// ============================================================================

/// GE8822 RGB controller @ 800 kHz
/// Timing: 350ns, 660ns, 350ns (total 1360ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_GE8822_800KHZ = {350, 660, 350, 0, "GE8822_800KHZ"};

/// WS2812 RGB controller @ 800 kHz (most common, overclockable)
/// Timing: 250ns, 625ns, 375ns (total 1250ns, fast variant)
/// Also available: 320ns, 320ns, 640ns (legacy variant)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_WS2812_800KHZ = {250, 625, 375, 280, "WS2812_800KHZ"};
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_WS2812_800KHZ_LEGACY = {320, 320, 640, 280, "WS2812_800KHZ_LEGACY"};

/// WS2813 RGB controller (same timing as WS2812)
/// Timing: 320ns, 320ns, 640ns (total 1280ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_WS2813 = {320, 320, 640, 300, "WS2813"};

/// SK6812 RGBW controller @ 800 kHz
/// Timing: 300ns, 600ns, 300ns (total 1200ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_SK6812 = {300, 600, 300, 80, "SK6812"};

/// SK6822 RGB controller @ 800 kHz
/// Timing: 375ns, 1000ns, 375ns (total 1750ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_SK6822 = {375, 1000, 375, 0, "SK6822"};

/// UCS1903B controller @ 800 kHz
/// Timing: 400ns, 450ns, 450ns (total 1300ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_UCS1903B_800KHZ = {400, 450, 450, 0, "UCS1903B_800KHZ"};

/// UCS1904 controller @ 800 kHz
/// Timing: 400ns, 400ns, 450ns (total 1250ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_UCS1904_800KHZ = {400, 400, 450, 0, "UCS1904_800KHZ"};

/// UCS2903 controller @ 800 kHz
/// Timing: 250ns, 750ns, 250ns (total 1250ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_UCS2903 = {250, 750, 250, 0, "UCS2903"};

/// TM1809 RGB controller @ 800 kHz
/// Timing: 350ns, 350ns, 450ns (total 1150ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_TM1809_800KHZ = {350, 350, 450, 0, "TM1809_800KHZ"};

/// TM1829 RGB controller @ 800 kHz
/// Timing: 340ns, 340ns, 550ns (total 1230ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_TM1829_800KHZ = {340, 340, 550, 500, "TM1829_800KHZ"};

/// TM1829 RGB controller @ 1600 kHz (high-speed variant)
/// Timing: 100ns, 300ns, 200ns (total 600ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_TM1829_1600KHZ = {100, 300, 200, 500, "TM1829_1600KHZ"};

/// LPD1886 RGB controller @ 1250 kHz
/// Timing: 200ns, 400ns, 200ns (total 800ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_LPD1886_1250KHZ = {200, 400, 200, 0, "LPD1886_1250KHZ"};

/// PL9823 RGB controller @ 800 kHz
/// Timing: 350ns, 1010ns, 350ns (total 1710ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_PL9823 = {350, 1010, 350, 0, "PL9823"};

/// SM16703 RGB controller @ 800 kHz
/// Timing: 300ns, 600ns, 300ns (total 1200ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_SM16703 = {300, 600, 300, 0, "SM16703"};

/// SM16824E RGB controller (high-speed variant)
/// Timing: 300ns, 900ns, 100ns (total 1300ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_SM16824E = {300, 900, 100, 200, "SM16824E"};

// ============================================================================
// Medium-Speed Chipsets (400kHz - 600kHz range)
// ============================================================================

/// WS2811 RGB controller @ 400 kHz (slower variant)
/// Timing: 800ns, 800ns, 900ns (total 2500ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_WS2811_400KHZ = {800, 800, 900, 50, "WS2811_400KHZ"};

/// WS2815 RGB controller @ 400 kHz
/// Timing: 250ns, 1090ns, 550ns (total 1890ns) - Note: Can be overclocked to 800kHz
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_WS2815 = {250, 1090, 550, 0, "WS2815"};

/// UCS1903 controller @ 400 kHz
/// Timing: 500ns, 1500ns, 500ns (total 2500ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_UCS1903_400KHZ = {500, 1500, 500, 0, "UCS1903_400KHZ"};

/// DP1903 controller @ 400 kHz
/// Timing: Multiple variants available - see FMUL-based definitions
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_DP1903_400KHZ = {800, 1600, 800, 0, "DP1903_400KHZ"};

/// TM1803 controller @ 400 kHz
/// Timing: 700ns, 1100ns, 700ns (total 2500ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_TM1803_400KHZ = {700, 1100, 700, 0, "TM1803_400KHZ"};

/// GW6205 controller @ 400 kHz
/// Timing: 800ns, 800ns, 800ns (total 2400ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_GW6205_400KHZ = {800, 800, 800, 0, "GW6205_400KHZ"};

/// UCS1912 controller @ 800 kHz
/// Timing: 250ns, 1000ns, 350ns (total 1600ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_UCS1912 = {250, 1000, 350, 0, "UCS1912"};

// ============================================================================
// Legacy/Special Chipsets
// ============================================================================

/// WS2811 RGB controller @ 800 kHz (fast variant)
/// Timing: 500ns, 2000ns, 2000ns (total 4500ns) - Legacy definition
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_WS2811_800KHZ_LEGACY = {500, 2000, 2000, 50, "WS2811_800KHZ_LEGACY"};

/// GW6205 controller @ 800 kHz (fast variant)
/// Timing: 400ns, 400ns, 400ns (total 1200ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_GW6205_800KHZ = {400, 400, 400, 0, "GW6205_800KHZ"};

/// DP1903 controller @ 800 kHz
/// Timing: 400ns, 1000ns, 400ns (total 1800ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_DP1903_800KHZ = {400, 1000, 400, 0, "DP1903_800KHZ"};

// ============================================================================
// RGBW Chipsets (16-bit color depth variants)
// ============================================================================

/// TM1814 RGBW controller @ 800 kHz
/// Timing: 360ns, 600ns, 340ns (total 1300ns)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_TM1814 = {360, 600, 340, 300, "TM1814"};

// ============================================================================
// UCS7604 Special 16-Bit RGBW Controller
// ============================================================================

/// UCS7604 RGBW controller @ 800 kHz (16-bit color depth)
/// Timing: 250ns, 625ns, 375ns (WS2812-compatible timing)
/// Special protocol with preamble support
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_UCS7604_800KHZ = {250, 625, 375, 280, "UCS7604_800KHZ"};

/// UCS7604 RGBW controller @ 1600 kHz (16-bit color depth, high-speed)
/// Timing: 125ns, 312ns, 188ns (half the 800kHz timings, roughly)
FL_INLINE_CONSTEXPR ChipsetTiming TIMING_UCS7604_1600KHZ = {125, 312, 188, 280, "UCS7604_1600KHZ"};

// ============================================================================
// Helper Functions
// ============================================================================

/// @brief Get total bit period (T1 + T2 + T3) in nanoseconds
/// @param timing Chipset timing structure
/// @return Total bit period in nanoseconds
constexpr uint32_t get_bit_period_ns(const fl::ChipsetTiming& timing) {
    return timing.T1 + timing.T2 + timing.T3;
}

/// @brief Extract T1 (high time for bit 0) from timing constant
/// @param timing Chipset timing structure
/// @return T1 value in nanoseconds
constexpr uint32_t get_timing_t1(const fl::ChipsetTiming& timing) {
    return timing.T1;
}

/// @brief Extract T2 (additional high time for bit 1) from timing constant
/// @param timing Chipset timing structure
/// @return T2 value in nanoseconds
constexpr uint32_t get_timing_t2(const fl::ChipsetTiming& timing) {
    return timing.T2;
}

/// @brief Extract T3 (low tail duration) from timing constant
/// @param timing Chipset timing structure
/// @return T3 value in nanoseconds
constexpr uint32_t get_timing_t3(const fl::ChipsetTiming& timing) {
    return timing.T3;
}

/// @brief Get timing by name (for dynamic lookup if needed)
/// @param name Chipset name string
/// @return Pointer to ChipsetTiming or nullptr if not found
/// Note: This is a runtime function and should only be used during initialization
const ChipsetTiming* get_timing_by_name(const char* name);

}  // namespace fl
