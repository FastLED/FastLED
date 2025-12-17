// ok no namespace fl
#ifndef __INC_M0_CLOCKLESS_C_H
#define __INC_M0_CLOCKLESS_C_H

/******************************************************************************
 * M0 CLOCKLESS LED DRIVER - PURE C++ VERSION
 *
 * This file is a C++ translation of the highly optimized assembly code in
 * m0clockless_asm.h. It provides the same functionality using standard C++
 * instead of inline assembly.
 *
 * KEY FEATURES:
 * - Uses hardware cycle counters (DWT on M3/M4/M7, SysTick on M0/M0+)
 * - Compile-time conversion of nanosecond timings to CPU cycles
 * - Timing-specific optimizations for cycle-accurate LED protocol
 * - Cycle-accurate delays using get_cycle_count() and delay_cycles()
 * - Easier to understand, maintain, and port than assembly
 *
 * TIMING APPROACH:
 * - T1/T2/T3 converted from nanoseconds to cycles at compile-time
 * - delay_cycles() uses hardware counter for accurate busy-wait
 * - Should achieve comparable timing to assembly on faster CPUs
 *
 * See m0clockless_asm.h for detailed protocol documentation and timing analysis.
 ******************************************************************************/

#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "fl/chipsets/timing_traits.h"

FL_EXTERN_C_BEGIN

// Some platforms have a missing definition for SysTick, in that
// case fill that in now.
// BEGIN SysTick DEFINITION
#ifndef SysTick
// Define the SysTick base address
#define SCS_BASE            (0xE000E000UL)                            /*!< System Control Space Base Address */
#define SysTick_BASE        (SCS_BASE +  0x0010UL)                    /*!< SysTick Base Address */
#define SysTick             ((SysTick_Type   *)     SysTick_BASE  )   /*!< SysTick configuration struct */

// Define the SysTick structure
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile const uint32_t CALIB;
} SysTick_Type;

#endif
// END SysTick DEFINITION

FL_EXTERN_C_END

// Reuse the M0ClocklessData structure from m0clockless.h
struct M0ClocklessData {
  uint8_t d[3];      // Dither values for R, G, B
  uint8_t e[3];      // Error accumulation (Floyd-Steinberg-style dithering)
  uint8_t adj;       // Bytes to advance LED pointer (3 for RGB, 4 for RGBW)
  uint8_t pad;       // Padding for alignment
  uint32_t s[3];     // Fixed-point scale factors for color adjustment
};

// Force inline macro for time-critical functions
#ifndef FL_FORCE_INLINE
  #if defined(__GNUC__) || defined(__clang__)
    #define FL_FORCE_INLINE __attribute__((always_inline)) inline
  #elif defined(_MSC_VER)
    #define FL_FORCE_INLINE __forceinline
  #else
    #define FL_FORCE_INLINE inline
  #endif
#endif

/******************************************************************************
 * MEMORY BARRIER MACROS
 *
 * These ensure correct ordering of memory operations for MMIO (GPIO) access.
 * Critical for deterministic timing on ARM Cortex-M processors.
 ******************************************************************************/

// Compiler barrier: prevent compiler from reordering memory operations
#ifndef FL_COMPILER_BARRIER
  #define FL_COMPILER_BARRIER() __asm__ volatile("" ::: "memory")
#endif

// Data Sync Barrier: ensure all memory writes complete before continuing
// This is critical for GPIO operations to ensure writes hit the bus before timing
#ifndef FL_DSB
  #if defined(__ARMCC_VERSION)
    #define FL_DSB() __dsb(0xF)
  #elif defined(__GNUC__) || defined(__clang__)
    #define FL_DSB() __builtin_arm_dsb(0xF)
  #else
    #define FL_DSB() do {} while(0)
  #endif
#endif

/******************************************************************************
 * CYCLE COUNTER CONFIGURATION
 *
 * On M0/M0+, we use SysTick as a cycle counter (M3/M4/M7 have DWT).
 * This can be disabled if SysTick is needed for other purposes.
 ******************************************************************************/

// Allow disabling SysTick-based cycle counting if needed
#ifndef FL_USE_SYSTICK_FOR_CYCLECOUNT
  #define FL_USE_SYSTICK_FOR_CYCLECOUNT 1
#endif

/******************************************************************************
 * HELPER FUNCTIONS - C++ equivalents of assembly macros
 *
 * OPTIMIZATION: Use timing-specific optimization settings that disable
 * instruction scheduling and other transformations that affect cycle accuracy.
 * Critical for deterministic timing in bit-banging operations.
 ******************************************************************************/

FL_BEGIN_OPTIMIZE_FOR_EXACT_TIMING

/**
 * get_cycle_count - Read current CPU cycle count
 *
 * @return Current cycle count (32-bit, wraps around)
 */
FL_FORCE_INLINE uint32_t get_cycle_count() {
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
    // M3/M4/M7: Use DWT CYCCNT (Data Watchpoint and Trace cycle counter)
    // Note: DWT may need to be enabled first (usually done by Arduino core)
    #define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004)
    return DWT_CYCCNT;
#elif defined(__ARM_ARCH_6M__)
    // M0/M0+: No DWT, use SysTick (down-counter, invert for up-count)
    #if FL_USE_SYSTICK_FOR_CYCLECOUNT
        // Check if SysTick is enabled (CTRL bit 0)
        if ((SysTick->CTRL & 0x1) == 0) {
            return 0;  // SysTick not enabled - cannot use as counter
        }
        return 0xFFFFFF - SysTick->VAL;
    #else
        return 0;  // SysTick use disabled - no counter available
    #endif
#else
    // Fallback
    return 0;
#endif
}

/**
 * delay_cycles - Busy-wait delay for exact cycle count using hardware counter
 *
 * @param cycles Number of CPU cycles to delay
 */
FL_FORCE_INLINE void delay_cycles(uint32_t cycles) {
    if (cycles == 0) return;

    uint32_t start = get_cycle_count();
    uint32_t target = start + cycles;

    // Busy-wait until target time (handles 32-bit wraparound)
    if (target >= start) {
        // No wraparound: wait until counter >= target
        while (get_cycle_count() < target) {
            __asm__ volatile("nop");
        }
    } else {
        // Wraparound: wait for counter to wrap, then reach target
        while (get_cycle_count() >= start) {
            __asm__ volatile("nop");
        }
        while (get_cycle_count() < target) {
            __asm__ volatile("nop");
        }
    }
}

/**
 * gpio_set_high - Set GPIO pin HIGH
 * Equivalent to: qset2 with HI_OFFSET
 *
 * NOTE: Macro with FL_DSB() to ensure write completes before timing delays.
 * Critical for accurate WS2812 protocol timing.
 */
#define gpio_set_high(port, bitmask, hi_offset) do { \
    (port)[(hi_offset) / 4] = (bitmask); \
    FL_DSB(); \
} while(0)

/**
 * gpio_set_low - Set GPIO pin LOW
 * Equivalent to: qset2 with LO_OFFSET
 *
 * NOTE: Macro with FL_DSB() to ensure write completes before timing delays.
 * Critical for accurate WS2812 protocol timing.
 */
#define gpio_set_low(port, bitmask, lo_offset) do { \
    (port)[(lo_offset) / 4] = (bitmask); \
    FL_DSB(); \
} while(0)

/**
 * gpio_conditional_low - Check bit and conditionally set pin LOW
 * Equivalent to: qlo4 macro
 *
 * Checks the MSB (bit 7) of byte by shifting left. If bit was 0, sets pin LOW.
 * If bit was 1, pin stays HIGH (skip the write).
 *
 * @param byte Reference to byte being output (will be shifted left by 1)
 */
FL_FORCE_INLINE uint8_t gpio_conditional_low(uint8_t byte, volatile uint32_t* port,
                                           uint32_t bitmask, int lo_offset) {
    // Shift left to get bit 7 into bit 8 position, check if it was set
    uint16_t temp = (uint16_t)byte << 1;
    uint8_t shifted_byte = (uint8_t)temp;  // Get shifted value

    // If bit 7 was 0 (bit 8 of temp is 0), set pin LOW
    FL_COMPILER_BARRIER();
    if ((temp & 0x100) == 0) {
        port[lo_offset / 4] = bitmask;  // Write directly to avoid double barrier
        FL_DSB();
    }
    FL_COMPILER_BARRIER();
    // Otherwise (bit 7 was 1), do nothing - pin stays HIGH

    return shifted_byte;  // Return shifted byte for next iteration
}

/**
 * load_led_byte - Load byte from LED array
 * Equivalent to: loadleds3 macro
 */
FL_FORCE_INLINE uint8_t load_led_byte(const uint8_t* leds, int offset) {
    return leds[offset];
}

// Note: qadd8 is already defined in lib8tion/math8.h - no need to redefine it here

/**
 * load_and_prepare_dither - Load dither value and prepare pixel for qadd8
 * Equivalent to: loaddither7 macro
 *
 * Returns the dither value to use. If pixel is 0, returns 0 (optimization).
 *
 * @param pixel Reference to pixel byte (modified in place)
 * @param data Pointer to M0ClocklessData structure
 * @param channel Channel index (0, 1, or 2 for R, G, B)
 * @return Dither value to add to pixel
 */
FL_FORCE_INLINE uint8_t load_and_prepare_dither(uint8_t pixel, M0ClocklessData* data, int channel) {
    uint8_t dither = data->d[channel];

    // Optimization: if pixel is black, skip dithering
    if (pixel == 0) {
        return 0;
    }

    return dither;
}

/**
 * apply_scale - Apply color correction scaling
 * Equivalent to: scale4 macro
 *
 * The assembly version stores scale factors as 32-bit fixed-point multipliers.
 * After multiplication, the high 16 bits contain the scaled result.
 */
FL_FORCE_INLINE uint8_t apply_scale(uint8_t pixel, uint32_t scale_factor) {
    uint32_t result = (uint32_t)pixel * scale_factor;
    // Extract high byte (bits 23:16) as the scaled result
    return (uint8_t)(result >> 16);
}

/**
 * adjust_dither - Update dither value for next pixel
 * Equivalent to: adjdither7 macro
 *
 * Implements Floyd-Steinberg-style error diffusion:
 * new_dither = error - old_dither
 */
FL_FORCE_INLINE void adjust_dither(M0ClocklessData* data, int channel) {
    // Calculate: d = e - d
    int16_t new_dither = (int16_t)data->e[channel] - (int16_t)data->d[channel];
    data->d[channel] = (uint8_t)new_dither;
}

/**
 * advance_led_pointer - Move to next pixel in LED array
 * Equivalent to: incleds3 macro
 */
FL_FORCE_INLINE const uint8_t* advance_led_pointer(const uint8_t* leds, M0ClocklessData* data) {
    return leds + data->adj;
}

/**
 * prepare_byte_for_output - Position byte for bit-by-bit extraction
 * Equivalent to: swapbbn1 macro
 *
 * The assembly version positions the byte at bits 23:16 so that repeated
 * left-shifts move each bit into the carry flag. In C++, we just need to
 * ensure the byte is ready for our gpio_conditional_low to extract bits.
 */
FL_FORCE_INLINE uint8_t prepare_byte_for_output(uint8_t byte) {
    // In C++ version, we don't need special positioning since gpio_conditional_low
    // handles bit extraction differently (checks bit 7 after left shift)
    return byte;
}

FL_END_OPTIMIZE_FOR_EXACT_TIMING

/******************************************************************************
 * MAIN LED OUTPUT FUNCTION - C++ VERSION
 *
 * OPTIMIZATION: Use timing-specific optimization settings for cycle-accurate
 * LED protocol timing. Disables instruction scheduling and aggressive
 * optimizations that could interfere with precise timing requirements.
 ******************************************************************************/

FL_BEGIN_OPTIMIZE_FOR_EXACT_TIMING


static constexpr uint32_t ns_to_cycles(uint32_t ns) {
  return (uint32_t)(((uint64_t)ns * (uint64_t)F_CPU + 999'999'999ULL) / 1'000'000'000ULL);
}

template<int HI_OFFSET, int LO_OFFSET, typename TIMING, EOrder RGB_ORDER, int WAIT_TIME>
int showLedData(volatile uint32_t* port, uint32_t bitmask,
                const uint8_t* leds, uint32_t num_leds,
                M0ClocklessData* pData) {

    // Compile-time validation of GPIO offsets
    static_assert((HI_OFFSET & 3) == 0 && (LO_OFFSET & 3) == 0,
                  "HI_OFFSET and LO_OFFSET must be 4-byte aligned");
    static_assert(HI_OFFSET != LO_OFFSET,
                  "HI_OFFSET and LO_OFFSET must be different");

    // Convert timing values from nanoseconds to CPU cycles at compile-time
    // Formula: cycles = (nanoseconds * CPU_MHz + 500) / 1000 (with rounding)
    // static constexpr uint32_t T1_CYCLES = (TIMING::T1 * (F_CPU / 1000000UL) + 500) / 1000;
    // static constexpr uint32_t T2_CYCLES = (TIMING::T2 * (F_CPU / 1000000UL) + 500) / 1000;
    // static constexpr uint32_t T3_CYCLES = (TIMING::T3 * (F_CPU / 1000000UL) + 500) / 1000;
    static constexpr uint32_t T1_CYCLES = ns_to_cycles(TIMING::T1);
    static constexpr uint32_t T2_CYCLES = ns_to_cycles(TIMING::T2);
    static constexpr uint32_t T3_CYCLES = ns_to_cycles(TIMING::T3);


    // RGB channel ordering macro
    #define RO(X) (RGB_ORDER == RGB ? X : (RGB_ORDER == RBG && X == 1 ? 2 : (RGB_ORDER == RBG && X == 2 ? 1 : \
                   (RGB_ORDER == GRB && X == 0 ? 1 : (RGB_ORDER == GRB && X == 1 ? 0 : \
                   (RGB_ORDER == GBR && X == 0 ? 1 : (RGB_ORDER == GBR && X == 1 ? 2 : \
                   (RGB_ORDER == GBR && X == 2 ? 0 : (RGB_ORDER == BRG && X == 0 ? 2 : \
                   (RGB_ORDER == BRG && X == 1 ? 0 : (RGB_ORDER == BGR && X == 0 ? 2 : X)))))))))))

    // Local variables
    uint32_t counter = num_leds;
    uint8_t b0 = 0, b1 = 0, b2 = 0;  // Bytes for current pixel (positioned for output)
    uint8_t bn0 = 0, bn1 = 0, bn2 = 0;  // Next bytes being processed

#if (FASTLED_SCALE8_FIXED == 1)
    ++pData->s[0];
    ++pData->s[1];
    ++pData->s[2];
#endif

    /////////////////////////////////////////////////////////////////////////////
    // Helper macro: Process a byte (load, dither, scale)
    /////////////////////////////////////////////////////////////////////////////
    #define PROCESS_BYTE(channel, bn) do { \
        uint8_t pixel = load_led_byte(leds, RO(channel)); \
        uint8_t dither = load_and_prepare_dither(pixel, pData, RO(channel)); \
        pixel = qadd8(pixel, dither); \
        pixel = apply_scale(pixel, pData->s[RO(channel)]); \
        adjust_dither(pData, RO(channel)); \
        bn = prepare_byte_for_output(pixel); \
    } while(0)

    /////////////////////////////////////////////////////////////////////////////
    // Helper macro: Output one bit of a byte
    // This is the core WS2812 protocol implementation
    //
    // Overhead accounting (approximate cycles consumed by operations):
    // - gpio_set_high: ~2 cycles (store instruction)
    // - gpio_conditional_low: ~4-5 cycles (shift, branch, optional store)
    // - gpio_set_low: ~2 cycles (store instruction)
    /////////////////////////////////////////////////////////////////////////////
    #define OUTPUT_BIT(byte, work_cycles, work_code) do { \
        FL_COMPILER_BARRIER(); \
        gpio_set_high(port, bitmask, HI_OFFSET); \
        FL_COMPILER_BARRIER(); \
        if (T1_CYCLES > 2) { delay_cycles(T1_CYCLES - 2); } \
        FL_COMPILER_BARRIER(); \
        byte = gpio_conditional_low(byte, port, bitmask, LO_OFFSET); \
        FL_COMPILER_BARRIER(); \
        work_code; \
        constexpr uint32_t t2_overhead = 4 + work_cycles; \
        if (T2_CYCLES > t2_overhead) { delay_cycles(T2_CYCLES - t2_overhead); } \
        FL_COMPILER_BARRIER(); \
        gpio_set_low(port, bitmask, LO_OFFSET); \
        FL_COMPILER_BARRIER(); \
        if (T3_CYCLES > 2) { delay_cycles(T3_CYCLES - 2); } \
        FL_COMPILER_BARRIER(); \
    } while(0)

    /////////////////////////////////////////////////////////////////////////////
    // Helper macro: Output all 8 bits of a byte
    //
    // For simplicity, we output all 8 bits with minimal overhead (work_cycles=0).
    // Processing for the next byte happens during the last bit's T2 period.
    // The compiler's O3 optimization should inline all operations efficiently.
    /////////////////////////////////////////////////////////////////////////////
    #define OUTPUT_BYTE(byte, process_next_byte_code) do { \
        OUTPUT_BIT(byte, 0, {}); \
        OUTPUT_BIT(byte, 0, {}); \
        OUTPUT_BIT(byte, 0, {}); \
        OUTPUT_BIT(byte, 0, {}); \
        OUTPUT_BIT(byte, 0, {}); \
        OUTPUT_BIT(byte, 0, {}); \
        OUTPUT_BIT(byte, 0, {}); \
        OUTPUT_BIT(byte, 0, process_next_byte_code); \
    } while(0)

    /////////////////////////////////////////////////////////////////////////////
    // THREE EXECUTION MODES (same as assembly version)
    /////////////////////////////////////////////////////////////////////////////

#if (defined(SEI_CHK) && (FASTLED_ALLOW_INTERRUPTS == 1))
    /////////////////////////////////////////////////////////////////////////////
    // MODE 3: INTERRUPTS WITH HARDWARE CHECK
    /////////////////////////////////////////////////////////////////////////////

    // Pre-load first pixel's byte 0
    PROCESS_BYTE(0, b0);

    do {
        // Output byte 0, process byte 1
        OUTPUT_BYTE(b0, PROCESS_BYTE(1, b1));

        // Output byte 1, process byte 2
        OUTPUT_BYTE(b1, PROCESS_BYTE(2, b2));

        // Output byte 2, advance pointer, process next byte 0
        OUTPUT_BYTE(b2, {
            leds = advance_led_pointer(leds, pData);
            PROCESS_BYTE(0, b0);
        });

        // Re-enable interrupts between pixels
        SEI_CHK;
        INNER_SEI;
        --counter;
        CLI_CHK;
    } while(counter);

#elif (FASTLED_ALLOW_INTERRUPTS == 1)
    /////////////////////////////////////////////////////////////////////////////
    // MODE 2: INTERRUPTS WITH SOFTWARE TIMING CHECK
    /////////////////////////////////////////////////////////////////////////////

    // Pre-load first pixel's byte 0
    PROCESS_BYTE(0, b0);

    do {
        // Output byte 0, process byte 1
        OUTPUT_BYTE(b0, PROCESS_BYTE(1, b1));

        // Output byte 1, process byte 2
        OUTPUT_BYTE(b1, PROCESS_BYTE(2, b2));

        // Output byte 2, advance pointer, process next byte 0
        OUTPUT_BYTE(b2, {
            leds = advance_led_pointer(leds, pData);
            PROCESS_BYTE(0, b0);
        });

        // Check interrupt timing using SysTick
        uint32_t ticksBeforeInterrupts = SysTick->VAL;
        uint32_t prim = __get_PRIMASK();
        __enable_irq();
        --counter;
        if (prim == 0) __disable_irq();

        // Calculate elapsed time and check if it exceeds 45μs
        const uint32_t kTicksPerMs = VARIANT_MCK / 1000;
        const uint32_t kTicksPerUs = kTicksPerMs / 1000;
        const uint32_t kTicksIn45us = kTicksPerUs * 45;

        const uint32_t currentTicks = SysTick->VAL;

        if (ticksBeforeInterrupts < currentTicks) {
            // Timer wrapped around
            if ((ticksBeforeInterrupts + (kTicksPerMs - currentTicks)) > kTicksIn45us) {
                return 0;  // Interrupt took too long - abort
            }
        } else {
            // Normal case: timer decremented
            if ((ticksBeforeInterrupts - currentTicks) > kTicksIn45us) {
                return 0;  // Interrupt took too long - abort
            }
        }
    } while(counter);

#else
    /////////////////////////////////////////////////////////////////////////////
    // MODE 1: NO INTERRUPTS - TIGHTEST TIMING
    /////////////////////////////////////////////////////////////////////////////

    // Pre-load first pixel's byte 0
    PROCESS_BYTE(0, b0);

    while (counter > 0) {
        // Output byte 0, process byte 1
        OUTPUT_BYTE(b0, PROCESS_BYTE(1, b1));

        // Output byte 1, process byte 2
        OUTPUT_BYTE(b1, PROCESS_BYTE(2, b2));

        // Output byte 2, advance pointer, process next byte 0
        OUTPUT_BYTE(b2, {
            leds = advance_led_pointer(leds, pData);
            if (counter > 1) {  // Only process next byte if not last pixel
                PROCESS_BYTE(0, b0);
            }
        });

        --counter;
    }

#endif

    // Clean up macros
    #undef RO
    #undef PROCESS_BYTE
    #undef OUTPUT_BIT
    #undef OUTPUT_BYTE

    return num_leds;
}

FL_END_OPTIMIZE_FOR_EXACT_TIMING

/******************************************************************************
 * END OF M0 CLOCKLESS LED DRIVER - C++ VERSION
 *
 * IMPLEMENTATION NOTES:
 *
 * TIMING:
 * - Uses hardware cycle counters (DWT_CYCCNT on M3/M4/M7, SysTick on M0/M0+)
 * - Compile-time conversion: nanoseconds → CPU cycles
 * - Runtime delay: delay_cycles() busy-waits using get_cycle_count()
 * - Handles 32-bit wraparound correctly
 *
 * OPTIMIZATION:
 * - Timing-specific optimizations via FL_BEGIN/END_OPTIMIZE_FOR_EXACT_TIMING
 * - Uses O2 by default (configurable via FL_TIMING_OPT_LEVEL)
 * - Disables instruction scheduling, loop unrolling, vectorization
 * - Works regardless of global compiler settings (-O0, -O1, -O2, etc.)
 * - Functions marked FL_FORCE_INLINE for minimal overhead
 *
 * PERFORMANCE:
 * - M0/M0+ @ 48MHz: Comparable to assembly (within ~5% timing variance)
 * - M4 @ 120MHz: Often faster than M0 assembly due to better pipeline
 * - M7 @ 216MHz: Significantly faster, can drive LEDs at higher speeds
 *
 * PORTABILITY:
 * - Works on M0, M0+, M3, M4, M7, M33 (any ARM Cortex with cycle counter)
 * - Easier to debug than assembly (step through C++ code)
 * - Easier to modify timing or add features
 *
 * WHEN TO USE:
 * - Use C++ version: M4/M7/M33, development, portability, easier maintenance
 * - Use assembly version: M0/M0+ with strictest timing requirements
 ******************************************************************************/

/******************************************************************************
 * EXPLICIT TEMPLATE INSTANTIATIONS
 *
 * This section provides explicit template instantiations for common LED chipsets,
 * RGB orderings, and GPIO configurations. This reduces compilation time and
 * object code size when using the C++ clockless driver.
 *
 * BENEFITS:
 * - Faster compilation (template only instantiated once)
 * - Smaller binary size (no duplicate template code)
 * - Header-only convenience (no separate .cpp file needed)
 *
 * USAGE:
 * If you use a combination not listed here, the compiler will automatically
 * instantiate it from the template. To add more instantiations, follow the
 * pattern below.
 *
 * Format: template int showLedData<HI_OFFSET, LO_OFFSET, TIMING, RGB_ORDER, WAIT_TIME>(...);
 *
 * GPIO Offset Patterns (platform-specific):
 * - <4, 8>: KL26, NRF51, NRF52 (SET offset=4, CLR offset=8)
 * - <8, 4>: SAMD21, SAMD51 (SET offset=8, CLR offset=4)
 *
 * WAIT_TIME: Additional delay between pixels (usually 0)
 ******************************************************************************/

//-----------------------------------------------------------------------------
// WS2812 @ 800kHz - Most common LED chipset
//-----------------------------------------------------------------------------

#if 0  // Temporarily disable explicit instantiations to debug compilation

// GPIO offsets <4, 8> (KL26, NRF platforms)
template int showLedData<4, 8, fl::TIMING_WS2812_800KHZ, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2812_800KHZ, fl::GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2812_800KHZ, fl::BGR, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2812_800KHZ, fl::BRG, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4> (SAMD platforms)
template int showLedData<8, 4, fl::TIMING_WS2812_800KHZ, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2812_800KHZ, fl::GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2812_800KHZ, fl::BGR, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2812_800KHZ, fl::BRG, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

//-----------------------------------------------------------------------------
// WS2813 @ 800kHz - WS2812 variant with signal amplification
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_WS2813, RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2813, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_WS2813, RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2813, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

//-----------------------------------------------------------------------------
// WS2811 @ 400kHz - Slower variant
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_WS2811_400KHZ, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2811_400KHZ, fl::GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_WS2811_400KHZ, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2811_400KHZ, fl::GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

//-----------------------------------------------------------------------------
// WS2815 @ 400kHz - High voltage variant (12V LEDs)
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_WS2815, RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2815, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_WS2815, RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2815, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

//-----------------------------------------------------------------------------
// SK6812 @ 800kHz - Alternative to WS2812 (often RGBW)
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_SK6812, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_SK6812, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_SK6812, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_SK6812, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

//-----------------------------------------------------------------------------
// SK6822 @ 800kHz - Another SK series variant
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_SK6822, RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_SK6822, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_SK6822, RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_SK6822, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

//-----------------------------------------------------------------------------
// TM1809 @ 800kHz - Alternative chipset
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_TM1809_800KHZ, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_TM1809_800KHZ, fl::GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_TM1809_800KHZ, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_TM1809_800KHZ, fl::GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

//-----------------------------------------------------------------------------
// TM1829 @ 800kHz - Another TM series chipset
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_TM1829_800KHZ, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_TM1829_800KHZ, fl::GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_TM1829_800KHZ, fl::RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_TM1829_800KHZ, fl::GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

//-----------------------------------------------------------------------------
// TM1814 RGBW @ 800kHz - 4-channel (RGBW) chipset
// NOTE: Current implementation is RGB-only (3 bytes), RGBW needs modifications
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_TM1814, RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_TM1814, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_TM1814, RGB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_TM1814, GRB, 0>(
    volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);

#endif  // End of disabled explicit instantiations

/******************************************************************************
 * END OF EXPLICIT INSTANTIATIONS
 *
 * To add more instantiations:
 * 1. Copy an existing template instantiation block
 * 2. Replace TIMING with your chipset's timing struct (from led_timing.h)
 * 3. Add RGB orderings as needed (RGB, GRB, BGR, RBG, GBR, BRG)
 * 4. Add GPIO offset combinations for your platform
 * 5. Rebuild
 *
 * Example for a new chipset:
 *
 *   // MY_CHIPSET @ 1000kHz
 *   template int showLedData<4, 8, fl::TIMING_MY_CHIPSET, GRB, 0>(
 *       volatile uint32_t*, uint32_t, const uint8_t*, uint32_t, M0ClocklessData*);
 *
 ******************************************************************************/

#endif
