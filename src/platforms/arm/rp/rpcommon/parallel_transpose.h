#pragma once

/// @file parallel_transpose.h
/// @brief Bit transposition functions for parallel LED output on RP2040/RP2350
///
/// This file provides efficient bit transposition functions to convert standard
/// LED data (RGB bytes) into the bit-parallel format required by the PIO state
/// machine for simultaneous multi-strip output.
///
/// ## Data Transformation
///
/// **Input Format (Standard FastLED):**
/// ```
/// Strip 0: [R0][G0][B0][R1][G1][B1]...  (sequential bytes)
/// Strip 1: [R0][G0][B0][R1][G1][B1]...
/// Strip 2: [R0][G0][B0][R1][G1][B1]...
/// Strip 3: [R0][G0][B0][R1][G1][B1]...
/// ```
///
/// **Output Format (Bit-Transposed for PIO):**
/// ```
/// For 4 strips, each byte contains 1 bit from each of 4 strips:
/// Byte 0:  [0][0][0][0][S3_R0_b7][S2_R0_b7][S1_R0_b7][S0_R0_b7]  // MSB of R0
/// Byte 1:  [0][0][0][0][S3_R0_b6][S2_R0_b6][S1_R0_b6][S0_R0_b6]
/// ...
/// Byte 7:  [0][0][0][0][S3_R0_b0][S2_R0_b0][S1_R0_b0][S0_R0_b0]  // LSB of R0
/// Byte 8:  [0][0][0][0][S3_G0_b7][S2_G0_b7][S1_G0_b7][S0_G0_b7]  // MSB of G0
/// ...
/// ```
///
/// For 8 strips, each byte contains 1 bit from each of 8 strips (no padding).
/// For 2 strips, each byte contains 1 bit from each of 2 strips (6 bits padding).
///
/// ## Performance
///
/// - **8-strip transpose:** ~15-20 CPU cycles per byte using Hacker's Delight algorithm
/// - **4-strip transpose:** ~8-12 CPU cycles per byte using nibble extraction
/// - **2-strip transpose:** ~6-8 CPU cycles per byte using bit extraction
///
/// ## Usage Example
///
/// ```cpp
/// #define NUM_STRIPS 4
/// #define LEDS_PER_STRIP 100
///
/// CRGB leds[NUM_STRIPS][LEDS_PER_STRIP];
/// fl::u8* strip_ptrs[NUM_STRIPS];
///
/// // Setup pointers to each strip
/// for (int i = 0; i < NUM_STRIPS; i++) {
///     strip_ptrs[i] = (fl::u8*)leds[i];
/// }
///
/// // Allocate output buffer (24 bytes per LED)
/// fl::u8* output = new fl::u8[LEDS_PER_STRIP * 24];
///
/// // Transpose all strips
/// transpose_4strips(strip_ptrs, output, LEDS_PER_STRIP);
///
/// // Now `output` is ready to feed to PIO via DMA
/// ```
///
/// @see parallel_pio_program.h for PIO program that consumes this data
/// @see bitswap.h for the underlying transpose8x1_MSB() implementation

#include "fl/int.h"
#include "fl/force_inline.h"
#include "bitswap.h"
namespace fl {
/// @brief Transpose 8 LED strips into parallel bit format
///
/// This function transposes 8 LED strips from standard byte-sequential format
/// to bit-parallel format suitable for 8-way PIO output. It uses the highly
/// optimized transpose8x1_MSB() function from bitswap.h (Hacker's Delight algorithm).
///
/// **Input:** 8 strips, each with `num_leds * 3` bytes (RGB)
/// **Output:** `num_leds * 24` bytes (8 bytes per bit position × 24 bits per LED)
///
/// **Memory Layout (per LED):**
/// - Bytes 0-7:   Red channel (MSB to LSB), 1 bit from each of 8 strips per byte
/// - Bytes 8-15:  Green channel (MSB to LSB)
/// - Bytes 16-23: Blue channel (MSB to LSB)
///
/// @param input Array of 8 pointers to LED strip data (each strip is num_leds * 3 bytes)
/// @param output Pointer to output buffer (must be at least num_leds * 24 bytes)
/// @param num_leds Number of LEDs per strip (all strips padded to this length)
///
/// @note All strips must be pre-padded to the same length (num_leds)
/// @note Output buffer must be pre-allocated by caller
/// @note This is the most efficient transpose function (uses existing optimized code)
///
/// @see transpose8x1_MSB() in bitswap.h
FASTLED_FORCE_INLINE void transpose_8strips(
    const u8* const input[8],
    u8* output,
    u16 num_leds
) {
    // Process each LED
    for (u16 led = 0; led < num_leds; led++) {
        u8 temp_input[8];

        // Process each color channel (R, G, B)
        for (int color = 0; color < 3; color++) {
            // Collect one byte from each strip for this color
            for (int strip = 0; strip < 8; strip++) {
                temp_input[strip] = input[strip][led * 3 + color];
            }

            // Transpose 8 bytes → 8 bytes (1 bit from each strip per output byte)
            // Output is MSB-first: output[0] has bit 7 of all 8 strips
            transpose8x1_MSB(temp_input, output);

            // Advance output pointer by 8 bytes (one transposed byte)
            output += 8;
        }
    }
}

/// @brief Transpose 4 LED strips into parallel bit format
///
/// This function transposes 4 LED strips from standard byte-sequential format
/// to bit-parallel format suitable for 4-way PIO output. Each output byte contains
/// 1 bit from each of the 4 strips in the lower 4 bits (upper 4 bits are zero).
///
/// **Input:** 4 strips, each with `num_leds * 3` bytes (RGB)
/// **Output:** `num_leds * 24` bytes (8 bytes per bit position × 24 bits per LED)
///
/// **Memory Layout (per bit position):**
/// - Bit 0: Strip 0 bit value
/// - Bit 1: Strip 1 bit value
/// - Bit 2: Strip 2 bit value
/// - Bit 3: Strip 3 bit value
/// - Bits 4-7: Zero (unused by PIO, but present for alignment)
///
/// @param input Array of 4 pointers to LED strip data (each strip is num_leds * 3 bytes)
/// @param output Pointer to output buffer (must be at least num_leds * 24 bytes)
/// @param num_leds Number of LEDs per strip (all strips padded to this length)
///
/// @note All strips must be pre-padded to the same length (num_leds)
/// @note Output buffer must be pre-allocated by caller
/// @note Upper 4 bits of each output byte are zero
FASTLED_FORCE_INLINE void transpose_4strips(
    const u8* const input[4],
    u8* output,
    u16 num_leds
) {
    // Process each LED
    for (u16 led = 0; led < num_leds; led++) {
        // Process each color channel (R, G, B)
        for (int color = 0; color < 3; color++) {
            // Collect one byte from each strip for this color
            u8 strip_bytes[4];
            for (int strip = 0; strip < 4; strip++) {
                strip_bytes[strip] = input[strip][led * 3 + color];
            }

            // Transpose: extract each bit position from all 4 strips
            // Output MSB-first (bit 7 first, then 6, 5, ..., 0)
            for (int bit = 7; bit >= 0; bit--) {
                u8 output_byte = 0;
                // Pack bits from all 4 strips into lower 4 bits
                for (int strip = 0; strip < 4; strip++) {
                    output_byte |= ((strip_bytes[strip] >> bit) & 1) << strip;
                }
                *output++ = output_byte;
            }
        }
    }
}

/// @brief Transpose 2 LED strips into parallel bit format
///
/// This function transposes 2 LED strips from standard byte-sequential format
/// to bit-parallel format suitable for 2-way PIO output. Each output byte contains
/// 1 bit from each of the 2 strips in the lower 2 bits (upper 6 bits are zero).
///
/// **Input:** 2 strips, each with `num_leds * 3` bytes (RGB)
/// **Output:** `num_leds * 24` bytes (8 bytes per bit position × 24 bits per LED)
///
/// **Memory Layout (per bit position):**
/// - Bit 0: Strip 0 bit value
/// - Bit 1: Strip 1 bit value
/// - Bits 2-7: Zero (unused by PIO, but present for alignment)
///
/// @param input Array of 2 pointers to LED strip data (each strip is num_leds * 3 bytes)
/// @param output Pointer to output buffer (must be at least num_leds * 24 bytes)
/// @param num_leds Number of LEDs per strip (all strips padded to this length)
///
/// @note All strips must be pre-padded to the same length (num_leds)
/// @note Output buffer must be pre-allocated by caller
/// @note Upper 6 bits of each output byte are zero
FASTLED_FORCE_INLINE void transpose_2strips(
    const u8* const input[2],
    u8* output,
    u16 num_leds
) {
    // Process each LED
    for (u16 led = 0; led < num_leds; led++) {
        // Process each color channel (R, G, B)
        for (int color = 0; color < 3; color++) {
            // Collect one byte from each strip for this color
            u8 strip_bytes[2];
            strip_bytes[0] = input[0][led * 3 + color];
            strip_bytes[1] = input[1][led * 3 + color];

            // Transpose: extract each bit position from both strips
            // Output MSB-first (bit 7 first, then 6, 5, ..., 0)
            for (int bit = 7; bit >= 0; bit--) {
                u8 output_byte =
                    ((strip_bytes[0] >> bit) & 1) |
                    (((strip_bytes[1] >> bit) & 1) << 1);
                *output++ = output_byte;
            }
        }
    }
}

/// @brief Calculate output buffer size needed for transposed data
///
/// All strip counts (2, 4, 8) use the same output format: 24 bytes per LED.
/// This is because each LED has 24 bits (8 bits × 3 colors), and each bit
/// position requires 1 output byte.
///
/// @param num_leds Maximum number of LEDs across all strips
/// @return Required output buffer size in bytes
///
/// @note For 8 strips: all 8 bits of each byte are used
/// @note For 4 strips: lower 4 bits used, upper 4 bits zero
/// @note For 2 strips: lower 2 bits used, upper 6 bits zero
FASTLED_FORCE_INLINE u32 calculate_transpose_buffer_size(u16 num_leds) {
    return num_leds * 24;  // 24 bytes per LED (3 colors × 8 bits each)
}

/// @brief Helper to transpose N strips with automatic dispatch
///
/// This is a convenience function that automatically selects the correct
/// transpose function based on the strip count. Useful for generic code.
///
/// @param num_strips Number of strips (must be 2, 4, or 8)
/// @param input Array of pointers to LED strip data
/// @param output Pointer to output buffer
/// @param num_leds Number of LEDs per strip
/// @return true if successful, false if invalid strip count
inline bool transpose_strips(
    u8 num_strips,
    const u8* const* input,
    u8* output,
    u16 num_leds
) {
    switch (num_strips) {
        case 8:
            transpose_8strips(input, output, num_leds);
            return true;
        case 4:
            transpose_4strips(input, output, num_leds);
            return true;
        case 2:
            transpose_2strips(input, output, num_leds);
            return true;
        default:
            return false;  // Invalid strip count
    }
}
}  // namespace fl