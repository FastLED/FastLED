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
struct TIMING_GE8822_800KHZ {
    enum : uint32_t {
        T1 = 350,
        T2 = 660,
        T3 = 350,
        RESET = 0
    };
};

// User-overridable WS2812 timing macros
// These allow compile-time customization for WS2812 variants (e.g., V5B)
#ifndef FASTLED_WS2812_T1
#define FASTLED_WS2812_T1 250
#endif

#ifndef FASTLED_WS2812_T2
#define FASTLED_WS2812_T2 625
#endif

#ifndef FASTLED_WS2812_T3
#define FASTLED_WS2812_T3 375
#endif

/// WS2812 RGB controller @ 800 kHz (most common, overclockable)
/// Timing: 250ns, 625ns, 375ns (total 1250ns, fast variant)
/// @note Timings can be overridden at compile time using FASTLED_WS2812_T1, FASTLED_WS2812_T2, FASTLED_WS2812_T3
struct TIMING_WS2812_800KHZ {
    enum : uint32_t {
        T1 = FASTLED_WS2812_T1,
        T2 = FASTLED_WS2812_T2,
        T3 = FASTLED_WS2812_T3,
        RESET = 280
    };
};

/// WS2812 RGB controller @ 800 kHz legacy variant
/// Timing: 320ns, 320ns, 640ns
struct TIMING_WS2812_800KHZ_LEGACY {
    enum : uint32_t {
        T1 = 320,
        T2 = 320,
        T3 = 640,
        RESET = 280
    };
};

/// WS2813 RGB controller (same timing as WS2812)
/// Timing: 320ns, 320ns, 640ns (total 1280ns)
struct TIMING_WS2813 {
    enum : uint32_t {
        T1 = 320,
        T2 = 320,
        T3 = 640,
        RESET = 300
    };
};

/// SK6812 RGBW controller @ 800 kHz
/// Timing: 300ns, 600ns, 300ns (total 1200ns)
struct TIMING_SK6812 {
    enum : uint32_t {
        T1 = 300,
        T2 = 600,
        T3 = 300,
        RESET = 80
    };
};

/// SK6822 RGB controller @ 800 kHz
/// Timing: 375ns, 1000ns, 375ns (total 1750ns)
struct TIMING_SK6822 {
    enum : uint32_t {
        T1 = 375,
        T2 = 1000,
        T3 = 375,
        RESET = 0
    };
};

/// UCS1903B controller @ 800 kHz
/// Timing: 400ns, 450ns, 450ns (total 1300ns)
struct TIMING_UCS1903B_800KHZ {
    enum : uint32_t {
        T1 = 400,
        T2 = 450,
        T3 = 450,
        RESET = 0
    };
};

/// UCS1904 controller @ 800 kHz
/// Timing: 400ns, 400ns, 450ns (total 1250ns)
struct TIMING_UCS1904_800KHZ {
    enum : uint32_t {
        T1 = 400,
        T2 = 400,
        T3 = 450,
        RESET = 0
    };
};

/// UCS2903 controller @ 800 kHz
/// Timing: 250ns, 750ns, 250ns (total 1250ns)
struct TIMING_UCS2903 {
    enum : uint32_t {
        T1 = 250,
        T2 = 750,
        T3 = 250,
        RESET = 0
    };
};

/// TM1809 RGB controller @ 800 kHz
/// Timing: 350ns, 350ns, 450ns (total 1150ns)
struct TIMING_TM1809_800KHZ {
    enum : uint32_t {
        T1 = 350,
        T2 = 350,
        T3 = 450,
        RESET = 0
    };
};

/// TM1829 RGB controller @ 800 kHz
/// Timing: 340ns, 340ns, 550ns (total 1230ns)
struct TIMING_TM1829_800KHZ {
    enum : uint32_t {
        T1 = 340,
        T2 = 340,
        T3 = 550,
        RESET = 500
    };
};

/// TM1829 RGB controller @ 1600 kHz (high-speed variant)
/// Timing: 100ns, 300ns, 200ns (total 600ns)
struct TIMING_TM1829_1600KHZ {
    enum : uint32_t {
        T1 = 100,
        T2 = 300,
        T3 = 200,
        RESET = 500
    };
};

/// LPD1886 RGB controller @ 1250 kHz
/// Timing: 200ns, 400ns, 200ns (total 800ns)
struct TIMING_LPD1886_1250KHZ {
    enum : uint32_t {
        T1 = 200,
        T2 = 400,
        T3 = 200,
        RESET = 0
    };
};

/// PL9823 RGB controller @ 800 kHz
/// Timing: 350ns, 1010ns, 350ns (total 1710ns)
struct TIMING_PL9823 {
    enum : uint32_t {
        T1 = 350,
        T2 = 1010,
        T3 = 350,
        RESET = 0
    };
};

/// SM16703 RGB controller @ 800 kHz
/// Timing: 300ns, 600ns, 300ns (total 1200ns)
struct TIMING_SM16703 {
    enum : uint32_t {
        T1 = 300,
        T2 = 600,
        T3 = 300,
        RESET = 0
    };
};

/// SM16824E RGB controller (high-speed variant)
/// Timing: 300ns, 900ns, 100ns (total 1300ns)
struct TIMING_SM16824E {
    enum : uint32_t {
        T1 = 300,
        T2 = 900,
        T3 = 100,
        RESET = 200
    };
};

// ============================================================================
// Medium-Speed Chipsets (400kHz - 600kHz range)
// ============================================================================

/// WS2811 RGB controller @ 400 kHz (slower variant)
/// Timing: 800ns, 800ns, 900ns (total 2500ns)
struct TIMING_WS2811_400KHZ {
    enum : uint32_t {
        T1 = 800,
        T2 = 800,
        T3 = 900,
        RESET = 50
    };
};

/// WS2815 RGB controller @ 400 kHz
/// Timing: 250ns, 1090ns, 550ns (total 1890ns) - Note: Can be overclocked to 800kHz
struct TIMING_WS2815 {
    enum : uint32_t {
        T1 = 250,
        T2 = 1090,
        T3 = 550,
        RESET = 0
    };
};

/// UCS1903 controller @ 400 kHz
/// Timing: 500ns, 1500ns, 500ns (total 2500ns)
struct TIMING_UCS1903_400KHZ {
    enum : uint32_t {
        T1 = 500,
        T2 = 1500,
        T3 = 500,
        RESET = 0
    };
};

/// DP1903 controller @ 400 kHz
struct TIMING_DP1903_400KHZ {
    enum : uint32_t {
        T1 = 800,
        T2 = 1600,
        T3 = 800,
        RESET = 0
    };
};

/// TM1803 controller @ 400 kHz
/// Timing: 700ns, 1100ns, 700ns (total 2500ns)
struct TIMING_TM1803_400KHZ {
    enum : uint32_t {
        T1 = 700,
        T2 = 1100,
        T3 = 700,
        RESET = 0
    };
};

/// GW6205 controller @ 400 kHz
/// Timing: 800ns, 800ns, 800ns (total 2400ns)
struct TIMING_GW6205_400KHZ {
    enum : uint32_t {
        T1 = 800,
        T2 = 800,
        T3 = 800,
        RESET = 0
    };
};

/// UCS1912 controller @ 800 kHz
/// Timing: 250ns, 1000ns, 350ns (total 1600ns)
struct TIMING_UCS1912 {
    enum : uint32_t {
        T1 = 250,
        T2 = 1000,
        T3 = 350,
        RESET = 0
    };
};

// ============================================================================
// Legacy/Special Chipsets
// ============================================================================

/// WS2811 RGB controller @ 800 kHz (fast variant)
/// Timing: 500ns, 2000ns, 2000ns (total 4500ns) - Legacy definition
struct TIMING_WS2811_800KHZ_LEGACY {
    enum : uint32_t {
        T1 = 500,
        T2 = 2000,
        T3 = 2000,
        RESET = 50
    };
};

/// GW6205 controller @ 800 kHz (fast variant)
/// Timing: 400ns, 400ns, 400ns (total 1200ns)
struct TIMING_GW6205_800KHZ {
    enum : uint32_t {
        T1 = 400,
        T2 = 400,
        T3 = 400,
        RESET = 0
    };
};

/// DP1903 controller @ 800 kHz
/// Timing: 400ns, 1000ns, 400ns (total 1800ns)
struct TIMING_DP1903_800KHZ {
    enum : uint32_t {
        T1 = 400,
        T2 = 1000,
        T3 = 400,
        RESET = 0
    };
};

// ============================================================================
// RGBW Chipsets (16-bit color depth variants)
// ============================================================================

/// TM1814 RGBW controller @ 800 kHz
/// Timing: 360ns, 600ns, 340ns (total 1300ns)
struct TIMING_TM1814 {
    enum : uint32_t {
        T1 = 360,
        T2 = 600,
        T3 = 340,
        RESET = 300
    };
};

// ============================================================================
// UCS7604 Special 16-Bit RGBW Controller
// ============================================================================

/// UCS7604 RGBW controller @ 800 kHz (16-bit color depth)
/// Timing: 250ns, 625ns, 375ns (WS2812-compatible timing)
/// Special protocol with preamble support
struct TIMING_UCS7604_800KHZ {
    enum : uint32_t {
        T1 = 250,
        T2 = 625,
        T3 = 375,
        RESET = 280
    };
};

/// UCS7604 RGBW controller @ 1600 kHz (16-bit color depth, high-speed)
/// Timing: 125ns, 312ns, 188ns (half the 800kHz timings, roughly)
struct TIMING_UCS7604_1600KHZ {
    enum : uint32_t {
        T1 = 125,
        T2 = 312,
        T3 = 188,
        RESET = 280
    };
};

// ============================================================================
// Helper Functions
// ============================================================================

/// @brief Convert enum-based timing type to runtime ChipsetTiming struct
/// @tparam TIMING Timing type with enum-based T1, T2, T3, RESET values
/// @return Runtime ChipsetTiming struct initialized from timing type
template <typename TIMING>
constexpr ChipsetTiming to_runtime_timing() {
    return {TIMING::T1, TIMING::T2, TIMING::T3, TIMING::RESET, "timing"};
}

/// @brief Get total bit period (T1 + T2 + T3) in nanoseconds
/// @param timing Chipset timing structure
/// @return Total bit period in nanoseconds
constexpr uint32_t get_bit_period_ns(const ChipsetTiming& timing) {
    return timing.T1 + timing.T2 + timing.T3;
}

/// @brief Extract T1 (high time for bit 0) from timing constant
/// @param timing Chipset timing structure
/// @return T1 value in nanoseconds
constexpr uint32_t get_timing_t1(const ChipsetTiming& timing) {
    return timing.T1;
}

/// @brief Extract T2 (additional high time for bit 1) from timing constant
/// @param timing Chipset timing structure
/// @return T2 value in nanoseconds
constexpr uint32_t get_timing_t2(const ChipsetTiming& timing) {
    return timing.T2;
}

/// @brief Extract T3 (low tail duration) from timing constant
/// @param timing Chipset timing structure
/// @return T3 value in nanoseconds
constexpr uint32_t get_timing_t3(const ChipsetTiming& timing) {
    return timing.T3;
}

/// @brief Get timing by name (for dynamic lookup if needed)
/// @param name Chipset name string
/// @return Pointer to ChipsetTiming or nullptr if not found
/// Note: This is a runtime function and should only be used during initialization
const ChipsetTiming* get_timing_by_name(const char* name);

}  // namespace fl
