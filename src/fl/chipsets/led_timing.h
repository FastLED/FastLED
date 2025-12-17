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
///
/// @note Want to convert from data sheet timings to three phase T1,T2,T3 timings?
///       Use this calculator at: ci/tools/led_timing_conversions.py

#pragma once

#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"

// ============================================================================
// Overclock Factor Configuration
// ============================================================================

// Allow overclocking of the clockless family of LEDs. 1.2 would be
// 20% overclocking. In tests WS2812 can be overclocked at 20%, but
// various manufacturers may be different. This is a global value
// which is overridable by each supported chipset.

#ifdef FASTLED_LED_OVERCLOCK
#warning "FASTLED_LED_OVERCLOCK has been changed to FASTLED_OVERCLOCK. Please update your code."
#define FASTLED_OVERCLOCK FASTLED_LED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK
#define FASTLED_OVERCLOCK 1.0
#else
#ifndef FASTLED_OVERCLOCK_SUPPRESS_WARNING
#warning "FASTLED_OVERCLOCK is now active, #define FASTLED_OVERCLOCK_SUPPRESS_WARNING to disable this warning"
#endif
#endif

// Per-chipset overclock factors (default to global FASTLED_OVERCLOCK)
// These allow fine-grained control of overclocking for specific chipsets

#ifndef FASTLED_OVERCLOCK_WS2812
#define FASTLED_OVERCLOCK_WS2812 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_WS2811
#define FASTLED_OVERCLOCK_WS2811 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_WS2813
#define FASTLED_OVERCLOCK_WS2813 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_WS2815
#define FASTLED_OVERCLOCK_WS2815 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_SK6822
#define FASTLED_OVERCLOCK_SK6822 FASTLED_OVERCLOCK
#endif

#ifndef FASTLED_OVERCLOCK_SK6812
#define FASTLED_OVERCLOCK_SK6812 FASTLED_OVERCLOCK
#endif

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
/// Four-phase: TH0=350ns, TH1=1010ns, TL0=1010ns, TL1=350ns
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
/// Four-phase: TH0=250ns, TH1=875ns, TL0=1000ns, TL1=375ns
/// @note Timings can be overridden at compile time using FASTLED_WS2812_T1, FASTLED_WS2812_T2, FASTLED_WS2812_T3
struct TIMING_WS2812_800KHZ {
    enum : uint32_t {
        T1 = FASTLED_WS2812_T1,
        T2 = FASTLED_WS2812_T2,
        T3 = FASTLED_WS2812_T3,
        RESET = 280
    };
    static constexpr const char* name() { return "WS2812_800KHZ"; }
};

/// Convenience alias for WS2812 timing (commonly used name)
using WS2812ChipsetTiming = TIMING_WS2812_800KHZ;

// User-overridable WS2812B-V5 timing macros
// These allow compile-time customization for WS2812B-V5 variants
#ifndef FASTLED_WS2812B_V5_T1
#define FASTLED_WS2812B_V5_T1 225
#endif

#ifndef FASTLED_WS2812B_V5_T2
#define FASTLED_WS2812B_V5_T2 355
#endif

#ifndef FASTLED_WS2812B_V5_T3
#define FASTLED_WS2812B_V5_T3 645
#endif

/// WS2812B-Mini-V3 / WS2812B-V5 RGB controller @ 800 kHz
/// Four-phase: TH0=225ns, TH1=580ns, TL0=1000ns, TL1=645ns
/// These newer variants share identical timing specifications with tighter tolerances
/// @note WS2812B-V5 and WS2812B-Mini-V3 use the same protocol timing
/// @note Based on official datasheets from World Semi
/// @note Timing values adjusted for WS2812B-V5 compatibility
/// @note Timings can be overridden at compile time using FASTLED_WS2812B_V5_T1, FASTLED_WS2812B_V5_T2, FASTLED_WS2812B_V5_T3
/// @see https://www.peace-corp.co.jp/data/WS2812B-Mini-V3_V3.0_EN.pdf (Mini-V3)
/// @see https://www.laskakit.cz/user/related_files/ws2812b.pdf (V5)
struct TIMING_WS2812B_MINI_V3 {
    enum : uint32_t {
        T1 = FASTLED_WS2812B_V5_T1,
        T2 = FASTLED_WS2812B_V5_T2,
        T3 = FASTLED_WS2812B_V5_T3,
        RESET = 280
    };
};

/// Convenience alias - WS2812B-V5 uses identical timing to Mini-V3
using TIMING_WS2812B_V5 = TIMING_WS2812B_MINI_V3;

/// WS2812 RGB controller @ 800 kHz legacy variant
/// Four-phase: TH0=320ns, TH1=640ns, TL0=960ns, TL1=640ns
struct TIMING_WS2812_800KHZ_LEGACY {
    enum : uint32_t {
        T1 = 320,
        T2 = 320,
        T3 = 640,
        RESET = 280
    };
};

/// WS2813 RGB controller (same timing as WS2812)
/// Four-phase: TH0=320ns, TH1=640ns, TL0=960ns, TL1=640ns
struct TIMING_WS2813 {
    enum : uint32_t {
        T1 = 320,
        T2 = 320,
        T3 = 640,
        RESET = 300
    };
};

/// SK6812 RGBW controller @ 800 kHz
/// Four-phase: TH0=300ns, TH1=900ns, TL0=900ns, TL1=300ns
struct TIMING_SK6812 {
    enum : uint32_t {
        T1 = 300,
        T2 = 600,
        T3 = 300,
        RESET = 80
    };
};

/// SK6822 RGB controller @ 800 kHz
/// Four-phase: TH0=375ns, TH1=1375ns, TL0=1375ns, TL1=375ns
struct TIMING_SK6822 {
    enum : uint32_t {
        T1 = 375,
        T2 = 1000,
        T3 = 375,
        RESET = 0
    };
};

/// UCS1903B controller @ 800 kHz
/// Four-phase: TH0=400ns, TH1=850ns, TL0=900ns, TL1=450ns
struct TIMING_UCS1903B_800KHZ {
    enum : uint32_t {
        T1 = 400,
        T2 = 450,
        T3 = 450,
        RESET = 0
    };
};

/// UCS1904 controller @ 800 kHz
/// Four-phase: TH0=400ns, TH1=800ns, TL0=850ns, TL1=450ns
struct TIMING_UCS1904_800KHZ {
    enum : uint32_t {
        T1 = 400,
        T2 = 400,
        T3 = 450,
        RESET = 0
    };
};

/// UCS2903 controller @ 800 kHz
/// Four-phase: TH0=250ns, TH1=1000ns, TL0=1000ns, TL1=250ns
struct TIMING_UCS2903 {
    enum : uint32_t {
        T1 = 250,
        T2 = 750,
        T3 = 250,
        RESET = 0
    };
};

/// TM1809 RGB controller @ 800 kHz
/// Four-phase: TH0=350ns, TH1=700ns, TL0=800ns, TL1=450ns
struct TIMING_TM1809_800KHZ {
    enum : uint32_t {
        T1 = 350,
        T2 = 350,
        T3 = 450,
        RESET = 0
    };
};

/// TM1829 RGB controller @ 800 kHz
/// Four-phase: TH0=340ns, TH1=680ns, TL0=890ns, TL1=550ns
struct TIMING_TM1829_800KHZ {
    enum : uint32_t {
        T1 = 340,
        T2 = 340,
        T3 = 550,
        RESET = 500
    };
};

/// TM1829 RGB controller @ 1600 kHz (high-speed variant)
/// Four-phase: TH0=100ns, TH1=400ns, TL0=500ns, TL1=200ns
struct TIMING_TM1829_1600KHZ {
    enum : uint32_t {
        T1 = 100,
        T2 = 300,
        T3 = 200,
        RESET = 500
    };
};

/// LPD1886 RGB controller @ 1250 kHz
/// Four-phase: TH0=200ns, TH1=600ns, TL0=600ns, TL1=200ns
struct TIMING_LPD1886_1250KHZ {
    enum : uint32_t {
        T1 = 200,
        T2 = 400,
        T3 = 200,
        RESET = 0
    };
};

/// PL9823 RGB controller @ 800 kHz
/// Four-phase: TH0=350ns, TH1=1360ns, TL0=1360ns, TL1=350ns
struct TIMING_PL9823 {
    enum : uint32_t {
        T1 = 350,
        T2 = 1010,
        T3 = 350,
        RESET = 0
    };
};

/// SM16703 RGB controller @ 800 kHz
/// Four-phase: TH0=300ns, TH1=900ns, TL0=900ns, TL1=300ns
struct TIMING_SM16703 {
    enum : uint32_t {
        T1 = 300,
        T2 = 600,
        T3 = 300,
        RESET = 0
    };
};

/// SM16824E RGB controller (high-speed variant)
/// Four-phase: TH0=300ns, TH1=1200ns, TL0=1000ns, TL1=100ns
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

/// WS2811 @ 400kHz (standard mode, datasheet specification)
/// @see WS2811 400kHz: T0H=500ns, T0L=2000ns, T1H=1200ns, T1L=1300ns (datasheet spec)
/// @see Conversion: T1=T0H=500, T2=(T1H-T0H)=700, T3=T1L=1300
/// @see Actual frequency: 2500ns cycle = 400kHz
/// @note WS2811 supports both 400kHz and 800kHz modes (configurable via pins 7&8)
/// @note Reset time increased to 280us for reliability (datasheet minimum is 50us)
struct TIMING_WS2811_400KHZ {
    enum : uint32_t {
        T1 = 500,   // T0H: 500ns (datasheet: 500ns ±150ns)
        T2 = 700,   // T1H - T0H: 700ns (T1H=1200ns per datasheet)
        T3 = 1300,  // T1L: 1300ns (datasheet: 1300ns ±150ns)
        RESET = 280 // Reset time: 280µs (>50µs per datasheet)
    };
};

/// WS2815 RGB controller @ 400 kHz
/// Four-phase: TH0=250ns, TH1=1340ns, TL0=1640ns, TL1=550ns
/// @note Can be overclocked to 800kHz
struct TIMING_WS2815 {
    enum : uint32_t {
        T1 = 250,
        T2 = 1090,
        T3 = 550,
        RESET = 0
    };
};

/// UCS1903 controller @ 400 kHz
/// Four-phase: TH0=500ns, TH1=2000ns, TL0=2000ns, TL1=500ns
struct TIMING_UCS1903_400KHZ {
    enum : uint32_t {
        T1 = 500,
        T2 = 1500,
        T3 = 500,
        RESET = 0
    };
};

/// DP1903 controller @ 400 kHz
/// Four-phase: TH0=800ns, TH1=2400ns, TL0=2400ns, TL1=800ns
struct TIMING_DP1903_400KHZ {
    enum : uint32_t {
        T1 = 800,
        T2 = 1600,
        T3 = 800,
        RESET = 0
    };
};

/// TM1803 controller @ 400 kHz
/// Four-phase: TH0=700ns, TH1=1800ns, TL0=1800ns, TL1=700ns
struct TIMING_TM1803_400KHZ {
    enum : uint32_t {
        T1 = 700,
        T2 = 1100,
        T3 = 700,
        RESET = 0
    };
};

/// GW6205 controller @ 400 kHz
/// Four-phase: TH0=800ns, TH1=1600ns, TL0=1600ns, TL1=800ns
struct TIMING_GW6205_400KHZ {
    enum : uint32_t {
        T1 = 800,
        T2 = 800,
        T3 = 800,
        RESET = 0
    };
};

/// UCS1912 controller @ 800 kHz
/// Four-phase: TH0=250ns, TH1=1250ns, TL0=1350ns, TL1=350ns
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

/// WS2811 @ 800kHz (fast mode, half the timing of 400kHz mode)
/// @see WS2811 800kHz: T0H=250ns, T0L=1000ns, T1H=600ns, T1L=650ns (half of 400kHz spec)
/// @see Conversion: T1=T0H=250, T2=(T1H-T0H)=350, T3=T1L=650
/// @see Actual frequency: 1250ns cycle = 800kHz
/// @note WS2811 supports both 400kHz and 800kHz modes (configurable via pins 7&8)
/// @note Reset time increased to 280us for reliability (datasheet minimum is 50us)
struct TIMING_WS2811_800KHZ_LEGACY {
    enum : uint32_t {
        T1 = 250,   // T0H: 250ns (half of 500ns @ 400kHz)
        T2 = 350,   // T1H - T0H: 350ns (T1H=600ns, half of 1200ns @ 400kHz)
        T3 = 650,   // T1L: 650ns (half of 1300ns @ 400kHz)
        RESET = 280 // Reset time: 280µs (>50µs per datasheet)
    };
};

/// GW6205 controller @ 800 kHz (fast variant)
/// Four-phase: TH0=400ns, TH1=800ns, TL0=800ns, TL1=400ns
struct TIMING_GW6205_800KHZ {
    enum : uint32_t {
        T1 = 400,
        T2 = 400,
        T3 = 400,
        RESET = 0
    };
};

/// DP1903 controller @ 800 kHz
/// Four-phase: TH0=400ns, TH1=1400ns, TL0=1400ns, TL1=400ns
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
/// Four-phase: TH0=360ns, TH1=960ns, TL0=940ns, TL1=340ns
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
/// Four-phase: TH0=420ns, TH1=840ns, TL0=580ns, TL1=160ns
/// @note Special protocol with preamble support
struct TIMING_UCS7604_800KHZ {
    enum : uint32_t {
        T1 = 420,
        T2 = 420,
        T3 = 160,
        RESET = 280
    };
};

/// UCS7604 RGBW controller @ 1600 kHz (16-bit color depth, high-speed)
/// Four-phase: TH0=210ns, TH1=420ns, TL0=380ns, TL1=170ns
/// @note Exactly half the 800kHz timings
struct TIMING_UCS7604_1600KHZ {
    enum : uint32_t {
        T1 = 210,
        T2 = 210,
        T3 = 170,
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
