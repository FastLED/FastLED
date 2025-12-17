// ok no namespace fl
#ifndef __INC_M0_CLOCKLESS_ASM_H
#define __INC_M0_CLOCKLESS_ASM_H

/******************************************************************************
 * M0 CLOCKLESS LED DRIVER - HIGHLY OPTIMIZED ASSEMBLY CODE
 *
 * This file contains cycle-accurate ARM Cortex-M0/M0+ assembly code for
 * driving WS2812-style LEDs using inline assembly with custom macros.
 *
 * WS2812 PROTOCOL REQUIREMENTS:
 * - T1 (T0H): High time for bit 0 (~350ns)
 * - T2 (T0L or T1H): Low time for bit 0 or high time for bit 1 (~800ns)
 * - T3 (T1L): Low time for bit 1 (~450ns)
 *
 * Each bit is transmitted by:
 * 1. Set pin HIGH
 * 2. Wait T1
 * 3. Set pin LOW if bit=0, keep HIGH if bit=1
 * 4. Wait remaining time
 *
 * KEY OPTIMIZATION:
 * While outputting 8 bits of one byte, we perform all processing for the
 * next byte (load, dither, scale) - achieving zero overhead and zero gaps
 * between pixels.
 *
 * PLATFORM DIFFERENCES:
 * - M0:  3-cycle branches, requires extra NOPs for timing
 * - M0+: 2-cycle branches, different timing compensation needed
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

/******************************************************************************
 * M0ClocklessData - Data structure for dithering and color correction
 *
 * Memory layout (10 bytes):
 *   Offset 0-2:  d[3]   - Dither values for R, G, B channels
 *   Offset 3-5:  e[3]   - Error accumulation for dithering (e-d calculation)
 *   Offset 6:    adj    - LED pointer advance amount (usually 3 for RGB)
 *   Offset 7:    pad    - Padding byte for alignment
 *   Offset 8-19: s[3]   - 32-bit scaling values for R, G, B (color adjustment)
 *
 * The assembly code accesses these fields using hardcoded offsets:
 *   d[x]:     offsets 0, 1, 2 (via RO macro for RGB ordering)
 *   e[x]:     offsets 3, 4, 5 (3+RO(x))
 *   adj:      offset 6 (used by incleds3 macro)
 *   s[x]:     offsets 8, 12, 16 (4*(2+RO(x)) - 32-bit values)
 ******************************************************************************/
struct M0ClocklessData {
  uint8_t d[3];      // Dither values for R, G, B
  uint8_t e[3];      // Error accumulation (Floyd-Steinberg-style dithering)
  uint8_t adj;       // Bytes to advance LED pointer (3 for RGB, 4 for RGBW)
  uint8_t pad;       // Padding for alignment
  uint32_t s[3];     // Fixed-point scale factors for color adjustment
};


template<int HI_OFFSET, int LO_OFFSET, typename TIMING, EOrder RGB_ORDER, int WAIT_TIME>int
showLedData(volatile uint32_t *_port, uint32_t _bitmask, const uint8_t *_leds, uint32_t num_leds, struct M0ClocklessData *pData) {
  /////////////////////////////////////////////////////////////////////////////
  // TIMING CALCULATION
  // Convert nanosecond timing values to CPU clock cycles
  // Formula: cycles = (nanoseconds * CPU_MHz + 500) / 1000
  // The +500 provides rounding to nearest integer
  //
  // Example: For WS2812 @ 48MHz:
  //   T1 (350ns) = (350 * 48 + 500) / 1000 = 17 cycles
  //   T2 (800ns) = (800 * 48 + 500) / 1000 = 38 cycles
  //   T3 (450ns) = (450 * 48 + 500) / 1000 = 22 cycles
  /////////////////////////////////////////////////////////////////////////////
  static constexpr uint32_t T1 = (TIMING::T1 * (F_CPU / 1000000UL) + 500) / 1000;
  static constexpr uint32_t T2 = (TIMING::T2 * (F_CPU / 1000000UL) + 500) / 1000;
  static constexpr uint32_t T3 = (TIMING::T3 * (F_CPU / 1000000UL) + 500) / 1000;

  /////////////////////////////////////////////////////////////////////////////
  // REGISTER ALLOCATION
  // ARM Cortex-M0 has constraints on which registers can be used with certain
  // instructions. Low registers (r0-r7) are required for most operations.
  // High registers (r8-r15) have limited instruction support.
  //
  // Low register variables (r0-r7):
  //   scratch  - Temporary workspace for calculations
  //   base     - Pointer to M0ClocklessData structure
  //   port     - GPIO port address for bit-banging
  //   d        - Dither working value
  //   counter  - Remaining pixel count
  //   bn       - Byte being processed (next byte)
  //   b        - Byte positioned for bit-by-bit output
  //   bitmask  - GPIO pin bitmask for setting/clearing pin
  //
  // High register variable (r8-r15):
  //   leds     - Pointer to LED data array (can use high register for ldrb)
  /////////////////////////////////////////////////////////////////////////////
  FASTLED_REGISTER uint32_t scratch=0;
  FASTLED_REGISTER struct M0ClocklessData *base = pData;
  FASTLED_REGISTER volatile uint32_t *port = _port;
  FASTLED_REGISTER uint32_t d=0;
  FASTLED_REGISTER uint32_t counter=num_leds;
  FASTLED_REGISTER uint32_t bn=0;
  FASTLED_REGISTER uint32_t b=0;
  FASTLED_REGISTER uint32_t bitmask = _bitmask;

  // High register variable
  FASTLED_REGISTER const uint8_t *leds = _leds;
#if (FASTLED_SCALE8_FIXED == 1)
  ++pData->s[0];
  ++pData->s[1];
  ++pData->s[2];
#endif
  asm __volatile__ (
    ///////////////////////////////////////////////////////////////////////////
    // ASSEMBLY MACRO DEFINITIONS
    // These macros are used to construct the cycle-accurate LED output code
    ///////////////////////////////////////////////////////////////////////////
    ".ifnotdef fl_delay_def;"

    ///////////////////////////////////////////////////////////////////////////
    // PLATFORM DETECTION: Cortex-M0 vs M0+
    //
    // M0 and M0+ have different branch timing:
    //   M0:  Branches take 3 cycles
    //   M0+: Branches take 2 cycles
    //
    // This affects timing calculations throughout the code. We set fl_is_m0p
    // to differentiate behavior, and define m0pad for timing compensation.
    //
    // m0pad macro: Inserts a NOP on M0+ where needed for timing balance.
    //              On M0, it expands to nothing (already slower).
    ///////////////////////////////////////////////////////////////////////////
#ifdef FASTLED_ARM_M0_PLUS
    "  .set fl_is_m0p, 1;"           // Mark as Cortex-M0+
    "  .macro m0pad;"
    "    nop;"                        // M0+ needs padding NOPs
    "  .endm;"
#else
    "  .set fl_is_m0p, 0;"           // Mark as Cortex-M0
    "  .macro m0pad;"
    "  .endm;"                        // M0 doesn't need padding
#endif
    ///////////////////////////////////////////////////////////////////////////
    // fl_delay MACRO - Precise cycle-accurate delays
    //
    // Parameters:
    //   dtime - Number of CPU cycles to delay
    //   reg   - Register to use for delay loop (default r0)
    //
    // Algorithm:
    //   1. Calculate loop iterations based on platform:
    //      - M0:  Each loop iteration = 4 cycles (sub=1, bne=3)
    //      - M0+: Each loop iteration = 3 cycles (sub=1, bne=2)
    //   2. Generate NOPs for remainder cycles that don't fit in loops
    //   3. Generate delay loop if needed
    //
    // Example: fl_delay 10, r0 on M0:
    //   - fl_delay_mod = 4 (cycles per loop)
    //   - dcycle = 10 / 4 = 2 loop iterations
    //   - dwork = 2 * 4 = 8 cycles from loops
    //   - drem = 10 - 8 = 2 remainder cycles
    //   - Generated code: 2 NOPs + loop with 2 iterations
    //
    // Loop structure:
    //   mov reg, #dcycle     ; Load counter (1 cycle)
    //   delayloop:
    //     sub reg, #1        ; Decrement (1 cycle)
    //     bne delayloop      ; Branch if not zero (2 cycles M0+, 3 cycles M0)
    //   [nop]                ; Extra NOP for M0 to reach 4 cycles/iteration
    ///////////////////////////////////////////////////////////////////////////
    "  .set fl_delay_def, 1;"
    "  .set fl_delay_mod, 4;"               // M0: 4 cycles per loop iteration
    "  .if fl_is_m0p == 1;"
    "    .set fl_delay_mod, 3;"             // M0+: 3 cycles per loop iteration
    "  .endif;"
    "  .macro fl_delay dtime, reg=r0;"
    "    .if (\\dtime > 0);"
    "      .set dcycle, (\\dtime / fl_delay_mod);"     // Full loop iterations
    "      .set dwork, (dcycle * fl_delay_mod);"       // Cycles from loops
    "      .set drem, (\\dtime - dwork);"              // Remainder cycles
    "      .rept (drem);"
    "        nop;"                                      // Emit NOPs for remainder
    "      .endr;"
    "      .if dcycle > 0;"
    "        mov \\reg, #dcycle;"                      // Load loop counter
    "        delayloop_\\@:;"                          // Unique label (\@ = unique number)
    "        sub \\reg, #1;"                           // Decrement counter
    "        bne delayloop_\\@;"                       // Loop if not zero
    "	     .if fl_is_m0p == 0;"
    "          nop;"                                   // M0 needs extra NOP (3-cycle branch + 1 NOP = 4)
    "        .endif;"
    "      .endif;"
    "    .endif;"
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // mod_delay MACRO - Adjusted delay accounting for other operations
    //
    // Parameters:
    //   dtime - Total delay time needed (cycles)
    //   b1    - Cycles consumed before this delay
    //   b2    - Cycles consumed after this delay
    //   reg   - Register for delay loop
    //
    // Purpose:
    //   When we need a specific total delay but other operations execute
    //   during that period, we only delay for the remaining time.
    //
    // Example: Need 10 cycle delay, but next operation takes 3 cycles
    //   mod_delay 10, 0, 3, r0  →  only delays 7 cycles
    ///////////////////////////////////////////////////////////////////////////
    "  .macro mod_delay dtime,b1,b2,reg;"
    "    .set adj, (\\b1 + \\b2);"                    // Total overhead cycles
    "    .if adj < \\dtime;"
    "      .set dtime2, (\\dtime - adj);"             // Remaining delay needed
    "      fl_delay dtime2, \\reg;"                   // Only delay the difference
    "    .endif;"
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // qlo4 MACRO - Check bit and conditionally set pin LOW (4 cycles)
    //
    // Parameters:
    //   b       - Byte containing bit to check (will be shifted left)
    //   bitmask - GPIO pin bitmask
    //   port    - GPIO port address
    //   loff    - LOW offset for GPIO (sets pin LOW)
    //
    // Operation:
    //   1. Left-shift byte by 1 bit (bit 7 → carry flag)
    //   2. If carry set (bit was 1): skip setting pin LOW
    //   3. If carry clear (bit was 0): set pin LOW by writing to GPIO
    //
    // This is the core of WS2812 encoding:
    //   - Bit 1: Pin stays HIGH (skip the LOW write)
    //   - Bit 0: Pin goes LOW early
    //
    // Timing: Exactly 4 cycles regardless of branch taken
    //   - Bit=1: lsl(1) + bcs-taken(2) + m0pad(1) = 4 cycles
    //   - Bit=0: lsl(1) + bcs-not-taken(1) + str(1) + m0pad(1) = 4 cycles
    ///////////////////////////////////////////////////////////////////////////
    "  .macro qlo4 b,bitmask,port,loff	;"
    "    lsl \\b, #1			;"                  // Shift left, bit 7 → carry
    "    bcs skip_\\@			;"                  // Branch if carry set (bit=1)
    "    str \\bitmask, [\\port, \\loff]	;"      // Bit=0: Set pin LOW
    "    skip_\\@:			;"
    "    m0pad;"                                     // Timing padding
    "  .endm				;"

    ///////////////////////////////////////////////////////////////////////////
    // qset2 MACRO - Unconditionally set pin HIGH or LOW (2 cycles + padding)
    //
    // Parameters:
    //   bitmask - GPIO pin bitmask
    //   port    - GPIO port address
    //   loff    - Offset: HI_OFFSET (set HIGH) or LO_OFFSET (set LOW)
    //
    // This is used at the start of each bit (set HIGH) and at the end
    // of T2 period (set LOW).
    ///////////////////////////////////////////////////////////////////////////
    "  .macro qset2 bitmask,port,loff;"
    "    str \\bitmask, [\\port, \\loff];"          // Write to GPIO port
    "    m0pad;"                                     // Timing padding
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // loadleds3 MACRO - Load next LED byte from memory (3 cycles)
    //
    // Parameters:
    //   leds    - Pointer to LED data array (may be high register r8-r15)
    //   bn      - Destination register for loaded byte (low register r0-r7)
    //   rled    - Offset index (RO(0), RO(1), or RO(2) for RGB ordering)
    //   scratch - Temporary low register for address calculation
    //
    // Why MOV first?
    //   ARM Cortex-M0 ldrb instruction requires base register to be a low
    //   register (r0-r7). The leds pointer may be in a high register, so we
    //   copy it to scratch (guaranteed low register) first.
    //
    // Cycles: 3 (mov=1, ldrb=2)
    ///////////////////////////////////////////////////////////////////////////
    "  .macro loadleds3 leds, bn, rled, scratch;"
    "    mov \\scratch, \\leds;"                    // Copy pointer to low register
    "    ldrb \\bn, [\\scratch, \\rled];"           // Load byte from (leds + offset)
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // loaddither7 MACRO - Load dither value and prepare for qadd8 (7 cycles)
    //
    // Parameters:
    //   bn      - Pixel byte to be dithered
    //   d       - Dither value (loaded from M0ClocklessData.d[x])
    //   base    - Pointer to M0ClocklessData structure
    //   rdither - Offset to dither value (0, 1, or 2)
    //
    // Why shift left 24 bits?
    //   ARM Cortex-M0 doesn't have a direct qadd8 (saturating 8-bit add)
    //   instruction. We simulate it by:
    //   1. Moving bytes to high positions (bits 31:24)
    //   2. Using 32-bit saturating add (adds instruction)
    //   3. Extracting result from high byte later
    //
    // Optimization:
    //   If pixel byte is 0, skip dithering (clear d to 0).
    //   This saves computation when the pixel is black.
    //
    // Cycles: 7 (ldrb=2, lsl=1, lsl=1, bne=1-2, eor=1, m0pad=0-1)
    ///////////////////////////////////////////////////////////////////////////
    "  .macro loaddither7 bn,d,base,rdither;"
    "    ldrb \\d, [\\base, \\rdither];"           // Load dither from M0ClocklessData.d[x]
    "    lsl \\d, #24;"                             // Shift dither to high byte (bits 31:24)
    "    lsl \\bn, #24;"                            // Shift pixel byte to high byte
    "    bne chkskip_\\@;"                          // If bn != 0, skip clearing dither
    "    eor \\d, \\d;"                             // If bn == 0, clear dither (XOR = 0)
    "    m0pad;"
    "    chkskip_\\@:;"
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // dither5 MACRO - Perform saturating 8-bit add for dithering (5 cycles)
    //
    // Parameters:
    //   bn - Pixel byte (in high position, bits 31:24)
    //   d  - Dither value (in high position, bits 31:24)
    //
    // Purpose:
    //   Implements qadd8 (saturating 8-bit add):
    //     if (bn + d > 255) return 255;
    //     else return bn + d;
    //
    // Algorithm:
    //   1. adds bn, d     - Add with carry flag set on overflow
    //   2. If carry:      - Overflow occurred (result > 255)
    //      mvns bn        - Invert bits: 0xFFFFFFFF → 0x00000000
    //      lsls bn, #24   - Shift: 0x000000FF in high byte (255)
    //   3. If no carry:   - No overflow, result is valid
    //      Keep bn as-is
    //
    // Platform differences:
    //   M0 (3-cycle branches):
    //     Can execute mvns+lsls in the taken branch path (3 cycles total).
    //     Uses NOP for timing balance when branch not taken.
    //
    //   M0+ (2-cycle branches):
    //     Must split mvns and lsls across two separate branch checks to
    //     maintain constant 5-cycle timing. Less efficient than M0!
    //
    // This is one of the few cases where M0 is more efficient than M0+.
    ///////////////////////////////////////////////////////////////////////////
    "  .macro dither5 bn,d;"
    "  .syntax unified;"
    "    .if fl_is_m0p == 0;"                       // M0 version (3-cycle branches)
    "      adds \\bn, \\d;"                          // Saturating add (sets carry on overflow)
    "      bcc dither5_1_\\@;"                       // Branch if no carry (no overflow)
    "      mvns \\bn, \\bn;"                         // Overflow: invert (0xFFFFFFFF → 0x00000000)
    "      lsls \\bn, \\bn, #24;"                    // Shift to get 0xFF in high byte
    "      dither5_1_\\@:;"
    "      nop;"                                     // NOP for timing balance
    "    .else;"                                    // M0+ version (2-cycle branches)
    "      adds \\bn, \\d;"                          // Saturating add
    "      bcc dither5_2_\\@;"
    "      mvns \\bn, \\bn;"                         // First part of saturation
    "      dither5_2_\\@:;"
    "      bcc dither5_3_\\@;"                       // Second branch for timing
    "      lsls \\bn, \\bn, #24;"                    // Second part of saturation
    "      dither5_3_\\@:;"
    "    .endif;"
    "  .syntax divided;"
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // scale4 MACRO - Apply color scaling/correction (4 cycles)
    //
    // Parameters:
    //   bn      - Pixel byte (currently in high position, bits 31:24)
    //   base    - Pointer to M0ClocklessData structure
    //   scale   - Offset to scale factor (offset 8, 12, or 16 for R, G, B)
    //   scratch - Temporary register for loading scale value
    //
    // Purpose:
    //   Apply color correction by multiplying pixel value with a scale factor.
    //   Scale factors are stored as 32-bit fixed-point multipliers in
    //   M0ClocklessData.s[0..2].
    //
    // Operation:
    //   1. Load 32-bit scale factor from memory
    //   2. Shift pixel byte back to low position (bits 7:0)
    //   3. Multiply pixel by scale factor
    //
    // Cycles: 4 (ldr=2, lsr=1, mul=1)
    ///////////////////////////////////////////////////////////////////////////
    "  .macro scale4 bn, base, scale, scratch;"
    "    ldr \\scratch, [\\base, \\scale];"           // Load 32-bit scale factor
    "    lsr \\bn, \\bn, #24;"                        // Shift pixel back to bits 7:0
    "    mul \\bn, \\scratch;"                        // Multiply: bn *= scale
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // swapbbn1 MACRO - Position processed byte for bit output (1 cycle)
    //
    // Parameters:
    //   b  - Destination register (will hold byte positioned for output)
    //   bn - Source byte (processed pixel byte)
    //
    // Purpose:
    //   Position the fully-processed byte so that qlo4 can extract bits 7-0
    //   using repeated left-shifts.
    //
    // Why bit 23?
    //   qlo4 uses "lsl b, #1" which shifts bit 23 → bit 24 → carry flag.
    //   Starting at bit 23 means we output bits 7-0 of the original byte in
    //   the correct order (MSB first).
    //
    // Cycles: 1
    ///////////////////////////////////////////////////////////////////////////
    "  .macro swapbbn1 b,bn;"
    "    lsl \\b, \\bn, #16;"                         // Position byte at bits 23:16
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // adjdither7 MACRO - Update dither for next pixel (7 cycles)
    //
    // Parameters:
    //   base    - Pointer to M0ClocklessData structure
    //   d       - Working register for dither calculation
    //   rled    - Offset to dither value in d[x] (0, 1, or 2)
    //   eoffset - Offset to error accumulator in e[x] (3, 4, or 5)
    //   scratch - Temporary register
    //
    // Purpose:
    //   Implement Floyd-Steinberg-style error diffusion dithering.
    //   The error from the current pixel affects the dither value for the
    //   next pixel.
    //
    // Algorithm:
    //   new_dither = error - old_dither
    //
    // Cycles: 7 (ldrb=2, ldrb=2, subs=1, strb=2)
    ///////////////////////////////////////////////////////////////////////////
    "  .macro adjdither7 base,d,rled,eoffset,scratch;"
    "    ldrb \\d, [\\base, \\rled];"                 // Load current dither value
    "    ldrb \\scratch,[\\base,\\eoffset];"          // Load error accumulator (e)
    "    .syntax unified;"
    "    subs \\d, \\scratch, \\d;"                   // Calculate: d = e - d
    "    .syntax divided;"
    "    strb \\d, [\\base, \\rled];"                 // Store new dither value
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // incleds3 MACRO - Advance LED pointer to next pixel (3 cycles)
    //
    // Parameters:
    //   leds    - Pointer to LED data array (will be incremented)
    //   base    - Pointer to M0ClocklessData structure
    //   scratch - Temporary register
    //
    // Purpose:
    //   Move to the next pixel in the LED array. The increment amount is
    //   stored in M0ClocklessData.adj (offset 6), typically 3 for RGB.
    //
    // Why variable increment?
    //   Allows the same code to work with different pixel formats:
    //     adj=3: RGB pixels
    //     adj=4: RGBW pixels (though current code only supports RGB)
    //
    // Cycles: 3 (ldrb=2, add=1)
    ///////////////////////////////////////////////////////////////////////////
    "  .macro incleds3   leds, base, scratch;"
    "    ldrb \\scratch, [\\base, #6];"               // Load increment from adj field
    "    add \\leds, \\leds, \\scratch;"              // Advance LED pointer
    "  .endm;"

    ///////////////////////////////////////////////////////////////////////////
    // cmploop5 MACRO - Decrement counter and loop (5 cycles)
    //
    // Parameters:
    //   counter - Pixel counter (decremented each iteration)
    //   label   - Label to jump to if more pixels remain
    //
    // Purpose:
    //   Loop control: continue processing pixels until counter reaches 0.
    //
    // Cycles: 5 (subs=1, beq=1-2, m0pad=0-1, b=2-3)
    ///////////////////////////////////////////////////////////////////////////
    "  .macro cmploop5 counter,label;"
    "    .syntax unified;"
    "    subs \\counter, #1;"                         // Decrement pixel counter
    "    .syntax divided;"
    "    beq done_\\@;"                                // If zero, exit loop
    "    m0pad;"
    "    b \\label;"                                   // Jump back to loop start
    "    done_\\@:;"
    "  .endm;"

    " .endif;"
  );

  /////////////////////////////////////////////////////////////////////////////
  // ASSEMBLY ARGUMENT MAPPING
  //
  // This maps C++ variables to assembly operands for inline assembly.
  //
  // Register Constraints:
  //   "l" = Low registers (r0-r7)   - required for most M0 instructions
  //   "h" = High registers (r8-r15) - limited instruction support
  //   "I" = Immediate constant       - compile-time constant value
  //   "+" = Read-write               - variable is both input and output
  //
  // Output operands (read-write):
  //   [leds]    - LED array pointer (high register OK for ldrb)
  //   [counter] - Remaining pixel count
  //   [scratch] - Temporary workspace
  //   [d]       - Dither working value
  //   [bn]      - Byte being processed (next byte)
  //   [b]       - Byte ready for bit-by-bit output
  //
  // Input operands (read-only):
  //   [port]    - GPIO port address
  //   [base]    - Pointer to M0ClocklessData
  //   [bitmask] - GPIO pin bitmask
  //   [hi_off]  - GPIO offset for setting pin HIGH
  //   [lo_off]  - GPIO offset for setting pin LOW
  //   [led0]    - Offset to LED byte 0 (accounts for RGB ordering via RO macro)
  //   [led1]    - Offset to LED byte 1
  //   [led2]    - Offset to LED byte 2
  //   [e0]      - Offset to error accumulator 0 (offset 3+RO(0))
  //   [e1]      - Offset to error accumulator 1
  //   [e2]      - Offset to error accumulator 2
  //   [scale0]  - Offset to scale factor 0 (32-bit, so 4*(2+RO(0)))
  //   [scale1]  - Offset to scale factor 1
  //   [scale2]  - Offset to scale factor 2
  //   [T1]      - Timing constant T1 (in CPU cycles)
  //   [T2]      - Timing constant T2
  //   [T3]      - Timing constant T3
  /////////////////////////////////////////////////////////////////////////////
#define M0_ASM_ARGS     :             \
      [leds] "+h" (leds),             \
      [counter] "+l" (counter),       \
      [scratch] "+l" (scratch),       \
      [d] "+l" (d),                   \
      [bn] "+l" (bn),                 \
      [b] "+l" (b)                    \
    :                                 \
      [port] "l" (port),              \
      [base] "l" (base),              \
      [bitmask] "l" (bitmask),        \
      [hi_off] "I" (HI_OFFSET),       \
      [lo_off] "I" (LO_OFFSET),       \
      [led0] "I" (RO(0)),             \
      [led1] "I" (RO(1)),             \
      [led2] "I" (RO(2)),             \
      [e0] "I" (3+RO(0)),             \
      [e1] "I" (3+RO(1)),             \
      [e2] "I" (3+RO(2)),             \
      [scale0] "I" (4*(2+RO(0))),         \
      [scale1] "I" (4*(2+RO(1))),         \
      [scale2] "I" (4*(2+RO(2))),         \
      [T1] "I" (T1),                  \
      [T2] "I" (T2),                  \
      [T3] "I" (T3)                   \
    :

  /////////////////////////////////////////////////////////////////////////////
  // CONVENIENCE MACROS - High-level operations built from assembly macros
  //
  // These macros compose the low-level assembly macros into readable
  // sequences that implement the WS2812 bit output protocol.
  //
  // Bit output sequence (repeated 8 times per byte):
  //   HI2              - Set pin HIGH (start of bit)
  //   _D1              - Wait T1 time (minus overhead)
  //   QLO4             - Check bit value, set LOW if bit=0
  //   <operation>      - Do useful work (load/dither/scale next byte)
  //   _D2(cycles)      - Wait remaining T2 time (minus operation cycles)
  //   LO2              - Set pin LOW (end of bit)
  //   _D3(cycles)      - Wait T3 time (minus overhead)
  //
  // Timeline for bit=1:
  //      ┌─── T1 ────┐┌─── T2 ────┐┌─── T3 ────┐
  // HIGH ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
  // LOW                           ▁▁▁▁▁▁▁▁▁▁▁▁▁▁
  //      ↑           ↑             ↑
  //     HI2         QLO4          LO2
  //                 (skip LOW)
  //
  // Timeline for bit=0:
  //      ┌─── T1 ────┐┌─── T2 ────┐┌─── T3 ────┐
  // HIGH ▔▔▔▔▔▔▔▔▔▔▔▔
  // LOW             ▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁
  //      ↑           ↑             ↑
  //     HI2         QLO4          LO2
  //                 (set LOW)
  /////////////////////////////////////////////////////////////////////////////
  // Loop control
#define LOOP            "  loop_%=:"                    // Define loop label
#define CMPLOOP5        "  cmploop5 %[counter], loop_%=;"   // Decrement counter and loop

  // Bit timing operations
#define HI2             "  qset2 %[bitmask], %[port], %[hi_off];"   // Set pin HIGH
#define LO2             "  qset2 %[bitmask], %[port], %[lo_off];"   // Set pin LOW
#define QLO4            "  qlo4 %[b],%[bitmask],%[port], %[lo_off];" // Conditionally set LOW based on bit
#define _D1             "  mod_delay %c[T1],2,0,%[scratch];"        // Delay T1 (minus HI2's 2 cycles)
#define _D2(ADJ)        "  mod_delay %c[T2],4," #ADJ ",%[scratch];" // Delay T2 (minus QLO4's 4 cycles + ADJ)
#define _D3(ADJ)        "  mod_delay %c[T3],2," #ADJ ",%[scratch];" // Delay T3 (minus LO2's 2 cycles + ADJ)

  // Pixel processing operations (occur during T2 delays between bits)
#define LOADLEDS3(X)    "  loadleds3 %[leds], %[bn], %[led" #X "] ,%[scratch];"     // Load byte X (3 cycles)
#define LOADDITHER7(X)  "  loaddither7 %[bn], %[d], %[base], %[led" #X "];"         // Load dither (7 cycles)
#define DITHER5         "  dither5 %[bn], %[d];"                                    // Apply dither (5 cycles)
#define SCALE4(X)       "  scale4 %[bn], %[base], %[scale" #X "], %[scratch];"      // Apply scaling (4 cycles)
#define ADJDITHER7(X)   "  adjdither7 %[base],%[d],%[led" #X "],%[e" #X "],%[scratch];" // Adjust dither (7 cycles)
#define SWAPBBN1        "  swapbbn1 %[b], %[bn];"                                   // Position byte for output (1 cycle)
#define INCLEDS3        "  incleds3 %[leds],%[base],%[scratch];"                    // Advance LED pointer (3 cycles)
#define NOTHING         ""                                                          // No operation (0 cycles)

  /////////////////////////////////////////////////////////////////////////////
  // THREE EXECUTION MODES
  //
  // The code has three modes depending on interrupt requirements:
  //
  // MODE 3 (SEI_CHK + ALLOW_INTERRUPTS): Hardware-assisted interrupt support
  //   - Interrupts enabled between pixels
  //   - Uses platform-specific macros (SEI_CHK, CLI_CHK) for timing checks
  //   - Best for platforms with hardware timer support
  //
  // MODE 2 (ALLOW_INTERRUPTS): Software interrupt support
  //   - Interrupts enabled between pixels
  //   - Uses SysTick timer to measure interrupt duration
  //   - Aborts if interrupt takes >45μs (would violate LED timing)
  //   - Small gaps between pixels (few μs)
  //
  // MODE 1 (neither): No interrupts - fastest, tightest timing
  //   - Entire LED string output in one uninterrupted assembly block
  //   - Zero gaps between pixels
  //   - Interrupts disabled entire time (can be >1ms for long strings)
  //   - Ideal for best visual quality when interrupts can be disabled
  /////////////////////////////////////////////////////////////////////////////

#if (defined(SEI_CHK) && (FASTLED_ALLOW_INTERRUPTS == 1))
    /////////////////////////////////////////////////////////////////////////////
    // MODE 3: INTERRUPTS WITH HARDWARE CHECK
    //
    // Pre-load first pixel's byte 0 outside the loop, then repeatedly:
    //   1. Output 3 bytes (1 complete RGB pixel) in assembly
    //   2. Use platform-specific macros to check/enable/disable interrupts
    //   3. Decrement counter
    //   4. Loop until all pixels done
    //
    // Characteristics:
    //   - Similar to Mode 2 but uses hardware timer support
    //   - Platform-specific timing checks (SEI_CHK, CLI_CHK)
    //   - Fastest of the interrupt-enabled modes
    /////////////////////////////////////////////////////////////////////////////
    // Pre-load byte 0 (outside main loop)
    // This prepares the first byte for output while still in C++ context
    asm __volatile__ (
      LOADLEDS3(0) LOADDITHER7(0) DITHER5 SCALE4(0) ADJDITHER7(0) SWAPBBN1
      M0_ASM_ARGS);

    do {
      asm __volatile__ (
      /////////////////////////////////////////////////////////////////////////
      // Write out byte 0 (8 bits), while preparing byte 1
      //
      // Each line outputs one bit using: HI2 _D1 QLO4 <work> _D2 LO2 _D3
      // During the T2 period of each bit, we perform work for next byte:
      //   Bit 7: Nothing (0 cycles)
      //   Bit 6: Load byte 1 from memory (3 cycles)
      //   Bit 5: Load dither value (7 cycles)
      //   Bit 4: Apply dithering (5 cycles)
      //   Bit 3: Apply color scaling (4 cycles)
      //   Bit 2: Adjust dither for next time (7 cycles)
      //   Bit 1: Nothing (0 cycles)
      //   Bit 0: Position byte 1 for output (1 cycle)
      /////////////////////////////////////////////////////////////////////////
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(1)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(1)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(1)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(1)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      /////////////////////////////////////////////////////////////////////////
      // Write out byte 1 (8 bits), while preparing byte 2
      /////////////////////////////////////////////////////////////////////////
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(2)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(2)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(2)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(2)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      /////////////////////////////////////////////////////////////////////////
      // Write out byte 2 (8 bits), while preparing byte 0 of NEXT pixel
      // After byte 2, we increment the LED pointer and prep the next pixel
      /////////////////////////////////////////////////////////////////////////
      HI2 _D1 QLO4 INCLEDS3        _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(0)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(0)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(0)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(0)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(5)

      M0_ASM_ARGS
      );
      // Re-enable interrupts between pixels (platform-specific macros)
      SEI_CHK; INNER_SEI; --counter; CLI_CHK;
    } while(counter);

#elif (FASTLED_ALLOW_INTERRUPTS == 1)
    /////////////////////////////////////////////////////////////////////////////
    // MODE 2: INTERRUPTS WITH SOFTWARE TIMING CHECK
    //
    // Similar to Mode 3, but uses SysTick timer to measure interrupt duration.
    // If an interrupt takes too long (>45μs), abort the frame to prevent
    // corrupting LED data with timing violations.
    //
    // Characteristics:
    //   - Interrupts enabled between pixels using sei()/cli()
    //   - SysTick timer tracks interrupt duration
    //   - Returns 0 (failure) if timing violation detected
    //   - Small gaps between pixels allowed
    /////////////////////////////////////////////////////////////////////////////
    // We're allowing interrupts - track the loop outside the asm code, and
    // re-enable interrupts in between each iteration.
    asm __volatile__ (
      // pre-load byte 0
      LOADLEDS3(0) LOADDITHER7(0) DITHER5 SCALE4(0) ADJDITHER7(0) SWAPBBN1
      M0_ASM_ARGS);

    do {
      asm __volatile__ (
      // Write out byte 0, prepping byte 1
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(1)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(1)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(1)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(1)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 1, prepping byte 2
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(2)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(2)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(2)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(2)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 INCLEDS3        _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 2, prepping byte 0
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(0)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(0)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(0)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(0)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(5)

      M0_ASM_ARGS
      );

      /////////////////////////////////////////////////////////////////////////
      // Interrupt timing check using SysTick timer
      //
      // SysTick is a down-counter that decrements on each CPU cycle.
      // We measure time elapsed during the interrupt by comparing SysTick->VAL
      // before and after enabling interrupts.
      //
      // IMPORTANT: SysTick counts DOWN, so elapsed = before - after
      // (or handle wraparound if timer restarted)
      //
      // Timing constraint:
      //   WS2812 LEDs require data within ~50μs or they latch/reset.
      //   We allow 45μs maximum interrupt duration to stay safe.
      /////////////////////////////////////////////////////////////////////////
      uint32_t ticksBeforeInterrupts = SysTick->VAL;
      sei();          // Enable interrupts
      --counter;
      cli();          // Disable interrupts

      // Calculate elapsed time and check if it exceeds 45μs
      // Note: This check isn't perfect - if >1ms elapses, SysTick wraps around
      // and we may not detect it correctly. But 1ms is way too long anyway.
      const uint32_t kTicksPerMs = VARIANT_MCK / 1000;
      const uint32_t kTicksPerUs = kTicksPerMs / 1000;
      const uint32_t kTicksIn45us = kTicksPerUs * 45;

      const uint32_t currentTicks = SysTick->VAL;

      if (ticksBeforeInterrupts < currentTicks) {
        // Timer wrapped around (started over during interrupt)
        if ((ticksBeforeInterrupts + (kTicksPerMs - currentTicks)) > kTicksIn45us) {
          return 0;  // Interrupt took too long - abort
        }
      } else {
        // Normal case: timer decremented (no wraparound)
        if ((ticksBeforeInterrupts - currentTicks) > kTicksIn45us) {
          return 0;  // Interrupt took too long - abort
        }
      }
    } while(counter);

#else
    /////////////////////////////////////////////////////////////////////////////
    // MODE 1: NO INTERRUPTS - TIGHTEST TIMING
    //
    // Output entire LED string in one uninterrupted assembly block.
    // This provides the best timing precision and zero inter-pixel gaps,
    // but keeps interrupts disabled for the entire duration.
    //
    // Duration: ~30μs per pixel × num_leds
    //   Example: 100 LEDs = ~3ms with interrupts disabled
    //
    // Characteristics:
    //   - Zero gaps between pixels
    //   - Perfect timing (no interrupt jitter)
    //   - Interrupts disabled for entire string (may affect responsiveness)
    //   - Best visual quality
    /////////////////////////////////////////////////////////////////////////////
    // Pre-load byte 0, then loop over ALL pixels in one assembly block
    asm __volatile__ (
      /////////////////////////////////////////////////////////////////////////
      // Pre-load byte 0 (prepare first byte before entering main loop)
      /////////////////////////////////////////////////////////////////////////
    LOADLEDS3(0) LOADDITHER7(0) DITHER5 SCALE4(0) ADJDITHER7(0) SWAPBBN1

    /////////////////////////////////////////////////////////////////////////////
    // Main loop: Write all pixels with zero interruptions
    //
    // For each pixel (3 bytes):
    //   - Write byte 0 (8 bits) while preparing byte 1
    //   - Write byte 1 (8 bits) while preparing byte 2
    //   - Write byte 2 (8 bits) while preparing byte 0 of next pixel
    //   - Loop via CMPLOOP5 until all pixels done
    //
    // This creates a continuous stream of precisely-timed bits with no gaps.
    /////////////////////////////////////////////////////////////////////////////
    LOOP
      // Write out byte 0, prepping byte 1
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(1)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(1)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(1)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(1)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 1, prepping byte 2
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(2)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(2)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(2)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(2)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 INCLEDS3        _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 2, prepping byte 0
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(0)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(0)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(0)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(0)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(5) CMPLOOP5

      M0_ASM_ARGS
    );
#endif

  /////////////////////////////////////////////////////////////////////////////
  // SUCCESS - All pixels output successfully
  // Return the number of LEDs written (or 0 if Mode 2 detected timing violation)
  /////////////////////////////////////////////////////////////////////////////
  return num_leds;
}

/******************************************************************************
 * END OF M0 CLOCKLESS LED DRIVER
 *
 * SUMMARY:
 * This highly optimized assembly code achieves:
 *
 * 1. Precise timing: Cycle-accurate bit timing for WS2812 protocol
 * 2. Zero overhead: Computation overlapped with I/O during T2 periods
 * 3. Flexible: Three modes for different interrupt requirements
 * 4. Portable: Works on both M0 and M0+ with timing adjustments
 * 5. Full-featured: Includes dithering, color scaling, and gamma correction
 *
 * The macro-based approach allows the same core logic to work for:
 * - Different RGB orderings (via RO() macro)
 * - Different CPU speeds (via T1/T2/T3 calculations)
 * - Different interrupt policies (via conditional compilation)
 *
 * LIMITATION: Hardcoded for 3 bytes per pixel (RGB only).
 * Adding RGBW support requires significant restructuring to output 4 bytes
 * per pixel and adjust all timing calculations.
 ******************************************************************************/

#endif
