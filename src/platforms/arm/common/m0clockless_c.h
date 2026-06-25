// IWYU pragma: private

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
 * - Compile-time conversion of nanosecond timings to CPU cycles
 * - Cycle-exact delays via compile-time NOP counting (no runtime cycle counter)
 * - Timing-specific optimizations for cycle-accurate LED protocol
 * - Easier to understand, maintain, and port than assembly
 *
 * TIMING APPROACH:
 * - T1/T2/T3 converted from nanoseconds to cycles at compile-time
 * - Bit delays emitted as compile-time NOP runs (fl_delay_cycles_ct<N>()), which
 *   are cycle-exact and need no timer. M0/M0+ has no DWT cycle counter, and
 *   SysTick is typically owned by the millis() tick (a periodic down-counter, not
 *   a free-running one), so a runtime counter cannot time sub-microsecond WS2812
 *   phases reliably -- this matches what the assembly driver does.
 *
 * See m0clockless_asm.h for detailed protocol documentation and timing analysis.
 ******************************************************************************/

#include "fl/stl/compiler_control.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/stdint.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/is_arm.h"  // FL_IS_ARM_M0_PLUS (loop-delay period selection)

// CMSIS interrupt-control intrinsics used by `showLedData`. These live as
// `__STATIC_INLINE` functions in `cmsis_gcc.h` per Cortex-M CMSIS bundle.
// gcc 15.2+ `-Wtemplate-body` errors on the function-template instantiation
// when `__get_PRIMASK` is an undeclared name at template-body parse time,
// so provide static-inline fallbacks here that match the canonical CMSIS
// semantics (inline asm against the PRIMASK SCS register). Guarded with
// `#ifndef` so a transitively-included `cmsis_gcc.h` wins -- on platforms
// where CMSIS is on the include path (LPC8xx, SAMD, nRF, STM32) the local
// definitions below are skipped. On Arduino-AVR where `__enable_irq` is a
// preprocessor macro from `WString.h`, the macro guard also wins.
// Skip entirely when a CMSIS device header is on the include path (it declares
// these as functions, which the #ifndef guards below cannot detect, so defining
// them here would clash). LPC builds set FASTLED_HAS_CMSIS in led_sysdefs.
#if !defined(FASTLED_HAS_CMSIS)
#ifndef __get_PRIMASK
static inline fl::u32 __get_PRIMASK(void) FL_NO_EXCEPT {
    fl::u32 primask;
    __asm volatile ("MRS %0, primask" : "=r" (primask) :: "memory");
    return primask;
}
#endif
#ifndef __enable_irq
static inline void __enable_irq(void) FL_NO_EXCEPT {
    __asm volatile ("cpsie i" ::: "memory");
}
#endif
#ifndef __disable_irq
static inline void __disable_irq(void) FL_NO_EXCEPT {
    __asm volatile ("cpsid i" ::: "memory");
}
#endif
#endif  // !FASTLED_HAS_CMSIS

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
    volatile fl::u32 CTRL;
    volatile fl::u32 LOAD;
    volatile fl::u32 VAL;
    volatile const fl::u32 CALIB;
} SysTick_Type;

#endif
// END SysTick DEFINITION

FL_EXTERN_C_END

// Reuse the M0ClocklessData structure from m0clockless.h
struct M0ClocklessData {
  fl::u8 d[3];      // Dither values for R, G, B
  fl::u8 e[3];      // Error accumulation (Floyd-Steinberg-style dithering)
  fl::u8 adj;       // Bytes to advance LED pointer (3 for RGB, 4 for RGBW)
  fl::u8 pad;       // Padding for alignment
  fl::u32 s[3];     // Fixed-point scale factors for color adjustment
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
    // Use inline assembly for maximum compatibility across ARM toolchains
    // __builtin_arm_dsb is not available on all GCC/Clang versions for ARM
    #define FL_DSB() __asm__ volatile ("dsb 0xF" ::: "memory")
  #else
    #define FL_DSB() do {} while(0)
  #endif
#endif

/******************************************************************************
 * HELPER FUNCTIONS - C++ equivalents of assembly macros
 *
 * TIMING: Bit delays use a compile-time, NOP-counted delay (see FlNopDelay /
 * fl_delay_cycles_ct below), NOT a runtime cycle counter. M0/M0+ has no DWT
 * CYCCNT, and reusing SysTick as a counter does not work (it is owned by the
 * millis() tick: a periodic down-counter, not a free-running one). A peripheral
 * counter read also costs more cycles than a sub-microsecond WS2812 bit phase,
 * so it cannot time these delays accurately. Compile-time NOP counting is
 * cycle-exact and needs no timer -- the same approach the assembly driver uses.
 *
 * OPTIMIZATION: Use timing-specific optimization settings that disable
 * instruction scheduling and other transformations that affect cycle accuracy.
 * Critical for deterministic timing in bit-banging operations.
 ******************************************************************************/

FL_BEGIN_OPTIMIZE_FOR_EXACT_TIMING

/**
 * Compile-time, NOP-counted cycle delay used by the WS2812 bit timing.
 *
 * NON-RECURSIVE on purpose. The repetition is done by the *assembler* via the
 * `.rept`/`.endr` directive, not by the C++ template engine, so:
 *
 *  1. There is no recursive template instantiation. The earlier approaches --
 *     both `fl::delaycycles<>` (binary-split recursion) and the hand-rolled
 *     `FlNopDelay<N>` (decrement recursion) -- OOM the cpptools/EDG IntelliSense
 *     engine, which speculatively instantiates the primary template to full
 *     depth. `fl_nop_run<N>` instantiates exactly one trivial function per N:
 *     instantiation depth 1, nothing for the analyzer to explode on.
 *
 *  2. It is fully inlined NOP emission, NOT a call. `fl::delaycycles<N>`'s
 *     positive-N specializations are deliberately out-of-line (they live in
 *     fl/system/delay.cpp.hpp and must emit external symbols for LTO), so calling
 *     it from showLedData branches back to FLASH -- defeating the whole point of
 *     running the driver from RAM (FL_RAMFUNC). `.rept nop` expands in place, so
 *     the delay stays inside the .ramfunc copy with zero flash access.
 *
 * Each `nop` is one cycle on Cortex-M0/M0+. N is clamped to >= 0 here so callers
 * may pass (T_CYCLES - overhead) without guarding against underflow on fast
 * clocks; `.rept 0` emits nothing. K is forced to an assembler-immediate via the
 * "i" constraint and printed with %c0 (no `#` prefix).
 */
template <int N> FL_FORCE_INLINE void fl_nop_run() FL_NO_EXCEPT {
    constexpr int K = (N > 0) ? N : 0;
    __asm__ __volatile__(".rept %c0\n\tnop\n\t.endr\n" : : "i"(K));
}

/**
 * Loop-based variant of the bit-timing delay. Opt in with FASTLED_M0_DELAY_LOOP.
 *
 * WHY: the .rept variant above unrolls to exactly N `nop`s, which is cycle-exact
 * but bulky -- on the 16 KB-SRAM LPC845, with showLedData running from RAM
 * (FL_RAMFUNC), ~48 unrolled delay sites cost a meaningful slice of SRAM. This
 * variant emits a 3-instruction counted loop (near-constant size regardless of N)
 * plus a few remainder nops, trading code size for coarser timing granularity.
 *
 * Per-iteration cost is the branch period: 3 cycles on Cortex-M0+ (subs=1,
 * bne-taken=2) and 4 on Cortex-M0 (bne-taken=3). The single-cycle counter init
 * (movs, emitted by the compiler) absorbs the final not-taken branch, so the
 * loop delays PERIOD*ITERS cycles; the leftover (N % PERIOD) is emitted as
 * unrolled nops. Like the .rept variant these are starting estimates -- the
 * loop's setup overhead differs, so re-verify the waveform on a scope and adjust
 * the OUTPUT_BIT overhead constants / T1,T2,T3 when switching modes.
 */
template <int N> FL_FORCE_INLINE void fl_nop_loop() FL_NO_EXCEPT {
#if defined(FL_IS_ARM_M0_PLUS)
    constexpr int PERIOD = 3;   // Cortex-M0+: bne-taken = 2 cycles
#else
    constexpr int PERIOD = 4;   // Cortex-M0: bne-taken = 3 cycles
#endif
    constexpr int K = (N > 0) ? N : 0;
    constexpr int ITERS = K / PERIOD;
    constexpr int REM = K - ITERS * PERIOD;   // 0 <= REM < PERIOD
    fl_nop_run<REM>();                         // remainder via .rept nops
    if (ITERS > 0) {
        fl::u32 cnt = ITERS;
        // Numeric local label 1: each inlined instance gets its own; safe to
        // repeat. Low-register ("l") counter -- M0 subs requires r0-r7.
        // .syntax unified: GAS is in divided mode for inline-asm strings here,
        // where `subs Rd,#imm` is rejected; the M0 asm driver does the same dance.
        __asm__ __volatile__(
            ".syntax unified\n\t"
            "1:\n\t"
            "subs %0, #1\n\t"
            "bne 1b\n\t"
            ".syntax divided\n\t"
            : "+l"(cnt) : : "cc");
    }
}

// Select the bit-timing delay strategy: unrolled nops (default, cycle-exact) or
// a counted loop (smaller code, for RAM-constrained RAMFUNC builds).
#if defined(FASTLED_M0_DELAY_LOOP)
  #define fl_delay_cycles_ct fl_nop_loop
#else
  #define fl_delay_cycles_ct fl_nop_run
#endif

/**
 * gpio_set_high - Set GPIO pin HIGH
 * Equivalent to: qset2 with HI_OFFSET
 *
 * The store goes through a `volatile` port pointer, so the compiler can neither
 * elide nor reorder it, and the surrounding FL_COMPILER_BARRIER() pins its
 * position. No FL_DSB(): a full data-sync barrier after every GPIO write costs
 * several cycles we cannot spare on a 24 MHz M0+ (~30 cycles per WS2812 bit),
 * and the single-core AHB store posts without it.
 */
#define gpio_set_high(port, bitmask, hi_offset) do { \
    (port)[(hi_offset) / 4] = (bitmask); \
} while(0)

/**
 * gpio_set_low - Set GPIO pin LOW
 * Equivalent to: qset2 with LO_OFFSET
 *
 * See gpio_set_high for the no-FL_DSB() rationale.
 */
#define gpio_set_low(port, bitmask, lo_offset) do { \
    (port)[(lo_offset) / 4] = (bitmask); \
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
FL_FORCE_INLINE fl::u8 gpio_conditional_low(fl::u8 byte, volatile fl::u32* port,
                                           fl::u32 bitmask, int lo_offset) FL_NO_EXCEPT {
    // Shift left to get bit 7 into bit 8 position, check if it was set
    fl::u16 temp = (fl::u16)byte << 1;
    fl::u8 shifted_byte = (fl::u8)temp;  // Get shifted value

    // If bit 7 was 0 (bit 8 of temp is 0), set pin LOW
    FL_COMPILER_BARRIER();
    if ((temp & 0x100) == 0) {
        port[lo_offset / 4] = bitmask;  // volatile store; no FL_DSB (see gpio_set_high)
    }
    FL_COMPILER_BARRIER();
    // Otherwise (bit 7 was 1), do nothing - pin stays HIGH

    return shifted_byte;  // Return shifted byte for next iteration
}

/**
 * load_led_byte - Load byte from LED array
 * Equivalent to: loadleds3 macro
 */
FL_FORCE_INLINE fl::u8 load_led_byte(const fl::u8* leds, int offset) FL_NO_EXCEPT {
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
FL_FORCE_INLINE fl::u8 load_and_prepare_dither(fl::u8 pixel, M0ClocklessData* data, int channel) FL_NO_EXCEPT {
    fl::u8 dither = data->d[channel];

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
FL_FORCE_INLINE fl::u8 apply_scale(fl::u8 pixel, fl::u32 scale_factor) FL_NO_EXCEPT {
    fl::u32 result = (fl::u32)pixel * scale_factor;
    // Extract high byte (bits 23:16) as the scaled result
    return (fl::u8)(result >> 16);
}

/**
 * adjust_dither - Update dither value for next pixel
 * Equivalent to: adjdither7 macro
 *
 * Implements Floyd-Steinberg-style error diffusion:
 * new_dither = error - old_dither
 */
FL_FORCE_INLINE void adjust_dither(M0ClocklessData* data, int channel) FL_NO_EXCEPT {
    // Calculate: d = e - d
    fl::i16 new_dither = (fl::i16)data->e[channel] - (fl::i16)data->d[channel];
    data->d[channel] = (fl::u8)new_dither;
}

/**
 * advance_led_pointer - Move to next pixel in LED array
 * Equivalent to: incleds3 macro
 */
FL_FORCE_INLINE const fl::u8* advance_led_pointer(const fl::u8* leds, M0ClocklessData* data) FL_NO_EXCEPT {
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
FL_FORCE_INLINE fl::u8 prepare_byte_for_output(fl::u8 byte) FL_NO_EXCEPT {
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


static constexpr fl::u32 ns_to_cycles(fl::u32 ns) FL_NO_EXCEPT {
  // C++14 digit separators (e.g. 999'999'999) break under -std=gnu++11 — the
  // single-quote is read as a character-literal delimiter and the next 3
  // digits become a stray multibyte char. Some downstream PlatformIO
  // environments (LPC8xx in particular, see issue #3329) still default to
  // gnu++11. Keep the constants in plain digits so the header stays portable
  // across both C++11 and C++14+ builds. Enforced by BareDigitSeparatorChecker
  // in ci/lint_cpp_rs/src/checkers/.
  return (fl::u32)(((u64)ns * (u64)F_CPU + 999999999ULL) / 1000000000ULL);
}

template<int HI_OFFSET, int LO_OFFSET, typename TIMING, EOrder RGB_ORDER, int WAIT_TIME>
FL_RAMFUNC int showLedData(volatile fl::u32* port, fl::u32 bitmask,
                const fl::u8* leds, fl::u32 num_leds,
                M0ClocklessData* pData) FL_NO_EXCEPT {

    // Compile-time validation of GPIO offsets
    FL_STATIC_ASSERT((HI_OFFSET & 3) == 0 && (LO_OFFSET & 3) == 0,
                  "HI_OFFSET and LO_OFFSET must be 4-byte aligned");
    FL_STATIC_ASSERT(HI_OFFSET != LO_OFFSET,
                  "HI_OFFSET and LO_OFFSET must be different");

    // Convert timing values from nanoseconds to CPU cycles at compile-time
    // Formula: cycles = (nanoseconds * CPU_MHz + 500) / 1000 (with rounding)
    // static constexpr uint32_t T1_CYCLES = (TIMING::T1 * (F_CPU / 1000000UL) + 500) / 1000;
    // static constexpr uint32_t T2_CYCLES = (TIMING::T2 * (F_CPU / 1000000UL) + 500) / 1000;
    // static constexpr uint32_t T3_CYCLES = (TIMING::T3 * (F_CPU / 1000000UL) + 500) / 1000;
    static constexpr fl::u32 T1_CYCLES = ns_to_cycles(TIMING::T1);
    static constexpr fl::u32 T2_CYCLES = ns_to_cycles(TIMING::T2);
    static constexpr fl::u32 T3_CYCLES = ns_to_cycles(TIMING::T3);


    // RGB channel ordering macro (use unique name to avoid collision with pixel_controller.h)
    #define M0_RO(X) (RGB_ORDER == RGB ? X : (RGB_ORDER == RBG && X == 1 ? 2 : (RGB_ORDER == RBG && X == 2 ? 1 : \
                   (RGB_ORDER == GRB && X == 0 ? 1 : (RGB_ORDER == GRB && X == 1 ? 0 : \
                   (RGB_ORDER == GBR && X == 0 ? 1 : (RGB_ORDER == GBR && X == 1 ? 2 : \
                   (RGB_ORDER == GBR && X == 2 ? 0 : (RGB_ORDER == BRG && X == 0 ? 2 : \
                   (RGB_ORDER == BRG && X == 1 ? 0 : (RGB_ORDER == BGR && X == 0 ? 2 : X)))))))))))

    // Local variables
    fl::u32 counter = num_leds;
    fl::u8 b0 = 0, b1 = 0, b2 = 0;  // Bytes for current pixel (positioned for output)

#if (FASTLED_SCALE8_FIXED == 1)
    ++pData->s[0];
    ++pData->s[1];
    ++pData->s[2];
#endif

    /////////////////////////////////////////////////////////////////////////////
    // Helper macro: Process a byte (load, dither, scale)
    /////////////////////////////////////////////////////////////////////////////
    #define PROCESS_BYTE(channel, bn) do { \
        fl::u8 pixel = load_led_byte(leds, M0_RO(channel)); \
        fl::u8 dither = load_and_prepare_dither(pixel, pData, M0_RO(channel)); \
        pixel = qadd8(pixel, dither); \
        pixel = apply_scale(pixel, pData->s[M0_RO(channel)]); \
        adjust_dither(pData, M0_RO(channel)); \
        bn = prepare_byte_for_output(pixel); \
    } while(0)

    /////////////////////////////////////////////////////////////////////////////
    // Helper macro: Output one bit of a byte
    // This is the core WS2812 protocol implementation
    //
    // Timing uses fl::delaycycles<N>() -- a compile-time NOP-counted delay (T1/T2/
    // T3_CYCLES are constexpr) -- NOT the get_cycle_count()/delay_cycles() busy-
    // wait. On M0/M0+ that busy-wait reads a peripheral counter (SysTick/MRT) per
    // iteration; the MMIO read costs more cycles than a WS2812 bit phase (~6/15/9
    // cycles at 24 MHz), so it always overshoots and stretches every bit. NOP
    // delays have zero read overhead and are cycle-exact. fl::delaycycles<N>() is
    // a no-op for N <= 0, so the overhead subtraction below is allowed to go
    // non-positive on very fast clocks without a guard.
    //
    // Overhead subtracted = cycles consumed by the GPIO ops inside each phase:
    // - gpio_set_high: ~2 cycles (store)        -> subtracted from T1
    // - gpio_conditional_low: ~4 cycles + work  -> subtracted from T2
    // - gpio_set_low: ~2 cycles (store)         -> subtracted from T3
    // These are starting estimates; verify the waveform on a scope and adjust.
    /////////////////////////////////////////////////////////////////////////////
    #define OUTPUT_BIT(byte, work_cycles, work_code) do { \
        FL_COMPILER_BARRIER(); \
        gpio_set_high(port, bitmask, HI_OFFSET); \
        FL_COMPILER_BARRIER(); \
        fl_delay_cycles_ct<(int)T1_CYCLES - 1>(); \
        FL_COMPILER_BARRIER(); \
        byte = gpio_conditional_low(byte, port, bitmask, LO_OFFSET); \
        FL_COMPILER_BARRIER(); \
        work_code; \
        fl_delay_cycles_ct<(int)T2_CYCLES - 3 - (int)(work_cycles)>(); \
        FL_COMPILER_BARRIER(); \
        gpio_set_low(port, bitmask, LO_OFFSET); \
        FL_COMPILER_BARRIER(); \
        fl_delay_cycles_ct<(int)T3_CYCLES - 1>(); \
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

        // Open a brief window for pending interrupts between pixels, then
        // restore the caller's interrupt state. PRIMASK == 1 means interrupts
        // were disabled on entry (e.g. showPixels() called cli()), so they must
        // be re-disabled here; PRIMASK == 0 means they were enabled, so leave
        // them enabled. (Previously this tested `prim == 0`, which was inverted:
        // entered with interrupts disabled, it left them enabled for the rest of
        // the frame, so ISRs fired mid-bit and corrupted the WS2812 timing.)
        // Check interrupt timing using SysTick
        fl::u32 ticksBeforeInterrupts = SysTick->VAL;
        fl::u32 prim = __get_PRIMASK();
        __enable_irq();
        --counter;
        if (prim != 0) __disable_irq();

        // Calculate elapsed time and check if it exceeds 45μs
        const fl::u32 kTicksPerMs = VARIANT_MCK / 1000;
        const fl::u32 kTicksPerUs = kTicksPerMs / 1000;
        const fl::u32 kTicksIn45us = kTicksPerUs * 45;

        const fl::u32 currentTicks = SysTick->VAL;

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
    #undef M0_RO
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
 * - Compile-time conversion: nanoseconds → CPU cycles
 * - Bit delays emitted as compile-time NOP runs (fl_delay_cycles_ct<N>()):
 *   cycle-exact, no runtime cycle counter, no timer dependency
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
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2812_800KHZ, fl::GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2812_800KHZ, fl::BGR, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2812_800KHZ, fl::BRG, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4> (SAMD platforms)
template int showLedData<8, 4, fl::TIMING_WS2812_800KHZ, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2812_800KHZ, fl::GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2812_800KHZ, fl::BGR, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2812_800KHZ, fl::BRG, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

//-----------------------------------------------------------------------------
// WS2813 @ 800kHz - WS2812 variant with signal amplification
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_WS2813, RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2813, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_WS2813, RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2813, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

//-----------------------------------------------------------------------------
// WS2811 @ 400kHz - Slower variant
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_WS2811_400KHZ, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2811_400KHZ, fl::GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_WS2811_400KHZ, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2811_400KHZ, fl::GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

//-----------------------------------------------------------------------------
// WS2815 @ 400kHz - High voltage variant (12V LEDs)
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_WS2815, RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_WS2815, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_WS2815, RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_WS2815, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

//-----------------------------------------------------------------------------
// SK6812 @ 800kHz - Alternative to WS2812 (often RGBW)
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_SK6812, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_SK6812, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_SK6812, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_SK6812, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

//-----------------------------------------------------------------------------
// SK6822 @ 800kHz - Another SK series variant
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_SK6822, RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_SK6822, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_SK6822, RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_SK6822, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

//-----------------------------------------------------------------------------
// TM1809 @ 800kHz - Alternative chipset
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_TM1809_800KHZ, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_TM1809_800KHZ, fl::GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_TM1809_800KHZ, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_TM1809_800KHZ, fl::GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

//-----------------------------------------------------------------------------
// TM1829 @ 800kHz - Another TM series chipset
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_TM1829_800KHZ, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_TM1829_800KHZ, fl::GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_TM1829_800KHZ, fl::RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_TM1829_800KHZ, fl::GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

//-----------------------------------------------------------------------------
// TM1814 RGBW @ 800kHz - 4-channel (RGBW) chipset
// NOTE: Current implementation is RGB-only (3 bytes), RGBW needs modifications
//-----------------------------------------------------------------------------

// GPIO offsets <4, 8>
template int showLedData<4, 8, fl::TIMING_TM1814, RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<4, 8, fl::TIMING_TM1814, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

// GPIO offsets <8, 4>
template int showLedData<8, 4, fl::TIMING_TM1814, RGB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);
template int showLedData<8, 4, fl::TIMING_TM1814, GRB, 0>(
    volatile fl::u32*, fl::u32, const fl::u8*, fl::u32, M0ClocklessData*);

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
