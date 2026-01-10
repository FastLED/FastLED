#pragma once

/// @file transposition.h
/// @brief Unified bit transposition functions for FastLED
///
/// This file consolidates all bit transposition and bit-interleaving functions used
/// throughout FastLED for various parallel LED output methods:
///
/// ## Core 8x1 Bit Transpose (from bitswap.h)
/// - Basic 8-byte by 8-bit rotation functions
/// - Based on Hacker's Delight algorithms
/// - Used by parallel output drivers on ARM, ESP8266, ESP32
///
/// ## SPI Multi-Lane Transposer
/// - Bit-interleaving for 2/4/8/16-way SPI parallel transmission
/// - Unified stateless functional design
/// - Used by APA102, SK9822, LPD8806, WS2801, P9813 controllers
///
/// ## Parallel Strip Transposer (RP2040/RP2350)
/// - Transpose N LED strips into bit-parallel format for PIO
/// - Optimized for 2/4/8 strip configurations
/// - Used by RP2040/RP2350 PIO-based parallel output

#include "fl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/int.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/optional.h"
#include "fl/stl/cstring.h"

namespace fl {

// ============================================================================
// Core 8x1 Bit Transpose Functions
// ============================================================================

// Note: These transpose functions are used across multiple platforms,
// so they are defined for all targets

/// Structure representing 8 bits of access
typedef union {
    fl::u8 raw;   ///< the entire byte
    struct {
        fl::u32 a0:1;  ///< bit 0 (0x01)
        fl::u32 a1:1;  ///< bit 1 (0x02)
        fl::u32 a2:1;  ///< bit 2 (0x04)
        fl::u32 a3:1;  ///< bit 3 (0x08)
        fl::u32 a4:1;  ///< bit 4 (0x10)
        fl::u32 a5:1;  ///< bit 5 (0x20)
        fl::u32 a6:1;  ///< bit 6 (0x40)
        fl::u32 a7:1;  ///< bit 7 (0x80)
    };
} just8bits;

/// Structure representing 32 bits of access
typedef struct {
    fl::u32 a0:1;  ///< byte 'a', bit 0
    fl::u32 a1:1;  ///< byte 'a', bit 1
    fl::u32 a2:1;  ///< byte 'a', bit 2
    fl::u32 a3:1;  ///< byte 'a', bit 3
    fl::u32 a4:1;  ///< byte 'a', bit 4
    fl::u32 a5:1;  ///< byte 'a', bit 5
    fl::u32 a6:1;  ///< byte 'a', bit 6
    fl::u32 a7:1;  ///< byte 'a', bit 7
    fl::u32 b0:1;  ///< byte 'b', bit 0
    fl::u32 b1:1;  ///< byte 'b', bit 1
    fl::u32 b2:1;  ///< byte 'b', bit 2
    fl::u32 b3:1;  ///< byte 'b', bit 3
    fl::u32 b4:1;  ///< byte 'b', bit 4
    fl::u32 b5:1;  ///< byte 'b', bit 5
    fl::u32 b6:1;  ///< byte 'b', bit 6
    fl::u32 b7:1;  ///< byte 'b', bit 7
    fl::u32 c0:1;  ///< byte 'c', bit 0
    fl::u32 c1:1;  ///< byte 'c', bit 1
    fl::u32 c2:1;  ///< byte 'c', bit 2
    fl::u32 c3:1;  ///< byte 'c', bit 3
    fl::u32 c4:1;  ///< byte 'c', bit 4
    fl::u32 c5:1;  ///< byte 'c', bit 5
    fl::u32 c6:1;  ///< byte 'c', bit 6
    fl::u32 c7:1;  ///< byte 'c', bit 7
    fl::u32 d0:1;  ///< byte 'd', bit 0
    fl::u32 d1:1;  ///< byte 'd', bit 1
    fl::u32 d2:1;  ///< byte 'd', bit 2
    fl::u32 d3:1;  ///< byte 'd', bit 3
    fl::u32 d4:1;  ///< byte 'd', bit 4
    fl::u32 d5:1;  ///< byte 'd', bit 5
    fl::u32 d6:1;  ///< byte 'd', bit 6
    fl::u32 d7:1;  ///< byte 'd', bit 7
} sub4;

/// Union containing a full 8 bytes to swap the bit orientation on
typedef union {
    fl::u32 word[2];  ///< two 32-bit values to load for swapping
    fl::u8 bytes[8];  ///< eight 8-bit values to load for swapping
    struct {
        sub4 a;  ///< 32-bit access struct for bit swapping, upper four bytes
        sub4 b;  ///< 32-bit access struct for bit swapping, lower four bytes
    };
} bitswap_type;

/// Simplified 8x1 bit transpose (non-inline version)
///
/// This rotates data into LSB for faster write (code can walk array backwards).
/// Based on: https://web.archive.org/web/20190108225554/http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
///
/// @param A Input array (8 bytes)
/// @param B Output array (8 bytes, transposed)
void transpose8x1_noinline(unsigned char *A, unsigned char *B);

/// Simplified 8x1 bit transpose (inline version)
///
/// @param A Input array (8 bytes)
/// @param B Output array (8 bytes, transposed)
FASTLED_FORCE_INLINE void transpose8x1(unsigned char *A, unsigned char *B) {
    fl::u32 x, y, t;

    // Load the array and pack it into x and y.
    y = *(unsigned int*)(A);
    x = *(unsigned int*)(A+4);

    // pre-transform x
    t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
    t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

    // pre-transform y
    t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
    t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

    // final transform
    t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
    y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
    x = t;

    *((uint32_t*)B) = y;
    *((uint32_t*)(B+4)) = x;
}

/// Simplified 8x1 bit transpose with MSB-first output
///
/// Based on: https://web.archive.org/web/20190108225554/http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
///
/// @param A Input array (8 bytes)
/// @param B Output array (8 bytes, transposed MSB-first)
FASTLED_FORCE_INLINE void transpose8x1_MSB(unsigned char *A, unsigned char *B) {
    fl::u32 x, y, t;

    // Load the array and pack it into x and y.
    y = *(unsigned int*)(A);
    x = *(unsigned int*)(A+4);

    // pre-transform x
    t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
    t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

    // pre-transform y
    t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
    t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

    // final transform
    t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
    y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
    x = t;

    B[7] = y; y >>= 8;
    B[6] = y; y >>= 8;
    B[5] = y; y >>= 8;
    B[4] = y;

    B[3] = x; x >>= 8;
    B[2] = x; x >>= 8;
    B[1] = x; x >>= 8;
    B[0] = x;
}

/// Templated 8x8 bit transpose with custom stride
///
/// Based on: https://web.archive.org/web/20190108225554/http://www.hackersdelight.org/hdcodetxt/transpose8.c.txt
///
/// @tparam m Input stride (1 for sequential bytes)
/// @tparam n Output stride (1 for sequential bytes)
/// @param A Input array (8 bytes at stride m)
/// @param B Output array (8 bytes at stride n, transposed)
template<int m, int n>
FASTLED_FORCE_INLINE void transpose8(unsigned char *A, unsigned char *B) {
    fl::u32 x, y, t;

    // Load the array and pack it into x and y.
    if(m == 1) {
        y = *(unsigned int*)(A);
        x = *(unsigned int*)(A+4);
    } else {
        x = (fl::u32(A[0])<<24)   | (fl::u32(A[m])<<16)   | (fl::u32(A[2*m])<<8) | A[3*m];
        y = (fl::u32(A[4*m])<<24) | (fl::u32(A[5*m])<<16) | (fl::u32(A[6*m])<<8) | A[7*m];
    }

    // pre-transform x
    t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
    t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

    // pre-transform y
    t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
    t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

    // final transform
    t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
    y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
    x = t;

    B[7*n] = y; y >>= 8;
    B[6*n] = y; y >>= 8;
    B[5*n] = y; y >>= 8;
    B[4*n] = y;

    B[3*n] = x; x >>= 8;
    B[2*n] = x; x >>= 8;
    B[n] = x; x >>= 8;
    B[0] = x;
}

// ============================================================================
// Low-Level ISR-Safe Transposition Primitives
// ============================================================================

/// @brief Low-level bit-interleaving primitive for 2 lanes (ISR-safe)
///
/// Transposes 2 input bytes into 2-way interleaved format with direct bit extraction.
/// This function is ISR-safe: no allocations, no exceptions, minimal overhead.
/// Inline functions are automatically placed where needed - no IRAM_ATTR required.
///
/// @param lane0_byte Pointer to lane 0 byte
/// @param lane1_byte Pointer to lane 1 byte
/// @param output Output buffer (must have space for 2 bytes)
/// @param num_bytes Number of bytes to transpose per lane
///
/// @note Inline function - inlined at call site (including ISR contexts)
/// @note Output size is num_bytes * 2
inline void transpose_2lane_inline(
    const uint8_t* lane0_byte,
    const uint8_t* lane1_byte,
    uint8_t* output,
    size_t num_bytes
);

/// @brief Low-level bit-interleaving primitive for 4 lanes (ISR-safe)
///
/// Transposes 4 input bytes into 4-way interleaved format with direct bit extraction.
/// This function is ISR-safe: no allocations, no exceptions, minimal overhead.
/// Inline functions are automatically placed where needed - no IRAM_ATTR required.
///
/// @param lanes Array of 4 lane byte pointers
/// @param output Output buffer (must have space for num_bytes * 4 bytes)
/// @param num_bytes Number of bytes to transpose per lane
///
/// @note Inline function - inlined at call site (including ISR contexts)
/// @note Output size is num_bytes * 4
inline void transpose_4lane_inline(
    const uint8_t* const lanes[4],
    uint8_t* output,
    size_t num_bytes
);

/// @brief Low-level bit-interleaving primitive for 8 lanes (ISR-safe)
///
/// Transposes 8 input bytes into 8-way interleaved format with optimized bit extraction.
/// This function is ISR-safe: no allocations, no exceptions, minimal overhead.
/// Inline functions are automatically placed where needed - no IRAM_ATTR required.
///
/// @param lanes Array of 8 lane byte pointers
/// @param output Output buffer (must have space for num_bytes * 8 bytes)
/// @param num_bytes Number of bytes to transpose per lane
///
/// @note Inline function - inlined at call site (including ISR contexts)
/// @note Output size is num_bytes * 8
inline void transpose_8lane_inline(
    const uint8_t* const lanes[8],
    uint8_t* output,
    size_t num_bytes
);

/// @brief Low-level bit-interleaving primitive for 16 lanes (ISR-safe)
///
/// Transposes 16 input bytes into 16-way interleaved format.
/// This function is ISR-safe: no allocations, no exceptions, minimal overhead.
/// Inline functions are automatically placed where needed - no IRAM_ATTR required.
///
/// @param lanes Array of 16 lane byte pointers
/// @param output Output buffer (must have space for num_bytes * 16 bytes)
/// @param num_bytes Number of bytes to transpose per lane
///
/// @note Inline function - inlined at call site (including ISR contexts)
/// @note Output size is num_bytes * 16
inline void transpose_16lane_inline(
    const uint8_t* const lanes[16],
    uint8_t* output,
    size_t num_bytes
);

/// @brief Generic bit-interleaving primitive for N lanes with M-bit source data (ISR-safe)
///
/// This is a generalized transposition function that can handle:
/// - Variable number of lanes (1-16)
/// - Variable source data width (8, 16, or 32 bits)
///
/// **Use cases:**
/// - ESP32 PARLIO: 8 lanes × 32-bit waveforms → 32 output bytes per source position
/// - Standard SPI: 8 lanes × 8-bit bytes → 8 output bytes per source position
///
/// @tparam TSource Source data type (uint8_t, uint16_t, or uint32_t)
/// @param lanes Array of lane data pointers (cast to TSource*)
/// @param num_lanes Number of lanes to transpose (1-16)
/// @param output Output buffer
/// @param num_items Number of source items to transpose per lane
///
/// @note Inline function - inlined at call site (including ISR contexts)
/// @note Output size is num_items * (sizeof(TSource) * 8) bytes
/// @note Handles lane counts < 8 by padding upper bits with zeros
template<typename TSource>
inline void transpose_generic_inline(
    const TSource* const lanes[],
    size_t num_lanes,
    uint8_t* output,
    size_t num_items
);

// Implementation of inline ISR-safe primitives

inline void transpose_2lane_inline(
    const uint8_t* lane0_byte,
    const uint8_t* lane1_byte,
    uint8_t* output,
    size_t num_bytes
) {
    for (size_t byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
        uint8_t a = lane0_byte[byte_idx];
        uint8_t b = lane1_byte[byte_idx];

        // dest[0] contains bit pairs for positions 7,6,5,4 (MSB first)
        output[byte_idx * 2 + 0] =
            ((a >> 7) & 0x01) << 0 | ((b >> 7) & 0x01) << 1 |
            ((a >> 6) & 0x01) << 2 | ((b >> 6) & 0x01) << 3 |
            ((a >> 5) & 0x01) << 4 | ((b >> 5) & 0x01) << 5 |
            ((a >> 4) & 0x01) << 6 | ((b >> 4) & 0x01) << 7;

        // dest[1] contains bit pairs for positions 3,2,1,0 (LSB)
        output[byte_idx * 2 + 1] =
            ((a >> 3) & 0x01) << 0 | ((b >> 3) & 0x01) << 1 |
            ((a >> 2) & 0x01) << 2 | ((b >> 2) & 0x01) << 3 |
            ((a >> 1) & 0x01) << 4 | ((b >> 1) & 0x01) << 5 |
            ((a >> 0) & 0x01) << 6 | ((b >> 0) & 0x01) << 7;
    }
}

inline void transpose_4lane_inline(
    const uint8_t* const lanes[4],
    uint8_t* output,
    size_t num_bytes
) {
    for (size_t byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
        uint8_t a = lanes[0][byte_idx];
        uint8_t b = lanes[1][byte_idx];
        uint8_t c = lanes[2][byte_idx];
        uint8_t d = lanes[3][byte_idx];

        uint8_t* dest = &output[byte_idx * 4];

        dest[0] = ((a >> 7) & 0x01) << 0 | ((b >> 7) & 0x01) << 1 | ((c >> 7) & 0x01) << 2 | ((d >> 7) & 0x01) << 3 |
                  ((a >> 6) & 0x01) << 4 | ((b >> 6) & 0x01) << 5 | ((c >> 6) & 0x01) << 6 | ((d >> 6) & 0x01) << 7;

        dest[1] = ((a >> 5) & 0x01) << 0 | ((b >> 5) & 0x01) << 1 | ((c >> 5) & 0x01) << 2 | ((d >> 5) & 0x01) << 3 |
                  ((a >> 4) & 0x01) << 4 | ((b >> 4) & 0x01) << 5 | ((c >> 4) & 0x01) << 6 | ((d >> 4) & 0x01) << 7;

        dest[2] = ((a >> 3) & 0x01) << 0 | ((b >> 3) & 0x01) << 1 | ((c >> 3) & 0x01) << 2 | ((d >> 3) & 0x01) << 3 |
                  ((a >> 2) & 0x01) << 4 | ((b >> 2) & 0x01) << 5 | ((c >> 2) & 0x01) << 6 | ((d >> 2) & 0x01) << 7;

        dest[3] = ((a >> 1) & 0x01) << 0 | ((b >> 1) & 0x01) << 1 | ((c >> 1) & 0x01) << 2 | ((d >> 1) & 0x01) << 3 |
                  ((a >> 0) & 0x01) << 4 | ((b >> 0) & 0x01) << 5 | ((c >> 0) & 0x01) << 6 | ((d >> 0) & 0x01) << 7;
    }
}

inline void transpose_8lane_inline(
    const uint8_t* const lanes[8],
    uint8_t* output,
    size_t num_bytes
) {
    for (size_t byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
        // Pack 8 bytes into a single 64-bit register
        // This reduces register pressure and enables parallel bit extraction
        uint64_t packed =
            ((uint64_t)lanes[0][byte_idx] << 0)  |
            ((uint64_t)lanes[1][byte_idx] << 8)  |
            ((uint64_t)lanes[2][byte_idx] << 16) |
            ((uint64_t)lanes[3][byte_idx] << 24) |
            ((uint64_t)lanes[4][byte_idx] << 32) |
            ((uint64_t)lanes[5][byte_idx] << 40) |
            ((uint64_t)lanes[6][byte_idx] << 48) |
            ((uint64_t)lanes[7][byte_idx] << 56);

        uint8_t* dest = &output[byte_idx * 8];

        // Extract bits in parallel (compiler can optimize independent shifts)
        for (int bit = 7; bit >= 0; bit--) {
            dest[7 - bit] =
                ((packed >> (bit + 0))  & 0x01) << 0 |
                ((packed >> (bit + 8))  & 0x01) << 1 |
                ((packed >> (bit + 16)) & 0x01) << 2 |
                ((packed >> (bit + 24)) & 0x01) << 3 |
                ((packed >> (bit + 32)) & 0x01) << 4 |
                ((packed >> (bit + 40)) & 0x01) << 5 |
                ((packed >> (bit + 48)) & 0x01) << 6 |
                ((packed >> (bit + 56)) & 0x01) << 7;
        }
    }
}

inline void transpose_16lane_inline(
    const uint8_t* const lanes[16],
    uint8_t* output,
    size_t num_bytes
) {
    for (size_t byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
        // Pack lanes 0-7 into first 64-bit register
        uint64_t packed_lo =
            ((uint64_t)lanes[0][byte_idx] << 0)  |
            ((uint64_t)lanes[1][byte_idx] << 8)  |
            ((uint64_t)lanes[2][byte_idx] << 16) |
            ((uint64_t)lanes[3][byte_idx] << 24) |
            ((uint64_t)lanes[4][byte_idx] << 32) |
            ((uint64_t)lanes[5][byte_idx] << 40) |
            ((uint64_t)lanes[6][byte_idx] << 48) |
            ((uint64_t)lanes[7][byte_idx] << 56);

        // Pack lanes 8-15 into second 64-bit register
        uint64_t packed_hi =
            ((uint64_t)lanes[8][byte_idx]  << 0)  |
            ((uint64_t)lanes[9][byte_idx]  << 8)  |
            ((uint64_t)lanes[10][byte_idx] << 16) |
            ((uint64_t)lanes[11][byte_idx] << 24) |
            ((uint64_t)lanes[12][byte_idx] << 32) |
            ((uint64_t)lanes[13][byte_idx] << 40) |
            ((uint64_t)lanes[14][byte_idx] << 48) |
            ((uint64_t)lanes[15][byte_idx] << 56);

        uint8_t* dest = &output[byte_idx * 16];

        // Extract bits in parallel from both packed registers
        for (int bit = 7; bit >= 0; bit--) {
            dest[7 - bit] =
                ((packed_lo >> (bit + 0))  & 0x01) << 0 |
                ((packed_lo >> (bit + 8))  & 0x01) << 1 |
                ((packed_lo >> (bit + 16)) & 0x01) << 2 |
                ((packed_lo >> (bit + 24)) & 0x01) << 3 |
                ((packed_lo >> (bit + 32)) & 0x01) << 4 |
                ((packed_lo >> (bit + 40)) & 0x01) << 5 |
                ((packed_lo >> (bit + 48)) & 0x01) << 6 |
                ((packed_lo >> (bit + 56)) & 0x01) << 7;

            dest[15 - bit] =
                ((packed_hi >> (bit + 0))  & 0x01) << 0 |
                ((packed_hi >> (bit + 8))  & 0x01) << 1 |
                ((packed_hi >> (bit + 16)) & 0x01) << 2 |
                ((packed_hi >> (bit + 24)) & 0x01) << 3 |
                ((packed_hi >> (bit + 32)) & 0x01) << 4 |
                ((packed_hi >> (bit + 40)) & 0x01) << 5 |
                ((packed_hi >> (bit + 48)) & 0x01) << 6 |
                ((packed_hi >> (bit + 56)) & 0x01) << 7;
        }
    }
}

template<typename TSource>
inline void transpose_generic_inline(
    const TSource* const lanes[],
    size_t num_lanes,
    uint8_t* output,
    size_t num_items
) {
    constexpr size_t bits_per_item = sizeof(TSource) * 8;

    for (size_t item_idx = 0; item_idx < num_items; item_idx++) {
        uint8_t* dest = &output[item_idx * bits_per_item];

        // Process each bit position in the source data (MSB to LSB)
        for (size_t bit_pos = 0; bit_pos < bits_per_item; bit_pos++) {
            size_t src_bit = (bits_per_item - 1) - bit_pos;
            uint8_t output_byte = 0;

            // Extract bit from each lane (up to 8 lanes per output byte)
            for (size_t lane = 0; lane < num_lanes && lane < 8; lane++) {
                TSource src_value = lanes[lane][item_idx];
                uint8_t bit = (src_value >> src_bit) & 0x01;
                output_byte |= (bit << (7 - lane));
            }

            dest[bit_pos] = output_byte;
        }
    }
}

// ============================================================================
// SPI Multi-Lane Transposer
// ============================================================================

/// Unified stateless bit-interleaving transposer for multi-lane SPI parallel LED transmission
///
/// This provides a unified functional approach to bit-interleaving for multi-lane SPI transmission.
/// All state is managed by the caller - the transposer only performs the conversion.
///
/// # Supported Widths
///
/// - **2-way SPI**: `transpose2()` - 2 parallel data lanes
/// - **4-way SPI**: `transpose4()` - 4 parallel data lanes
/// - **8-way SPI**: `transpose8()` - 8 parallel data lanes
/// - **16-way SPI**: `transpose16()` - 16 parallel data lanes
///
/// # How Bit-Interleaving Works
///
/// Traditional SPI sends one byte at a time on a single data line (MOSI).
/// Multi-lane SPI uses N data lines (D0-DN) to send N bits in parallel per clock cycle.
///
/// The transposer converts per-lane data into interleaved format:
///
/// **Example: 2-way SPI**
/// ```
/// Input (2 separate lanes):
///   Lane 0: [0xAB, ...] → Strip 1 (D0 pin)
///   Lane 1: [0x12, ...] → Strip 2 (D1 pin)
///
/// Output (interleaved): Each input byte becomes 2 output bytes
///   Input: Lane0=0xAB (10101011), Lane1=0x12 (00010010)
///   Out[0] = 0x2A (bits 7:4 from each lane, 4 bits per lane)
///   Out[1] = 0x2B (bits 3:0 from each lane, 4 bits per lane)
/// ```
///
/// # Synchronized Latching with Black LED Padding
///
/// LED strips often have different lengths. To ensure all strips latch simultaneously
/// (updating LEDs at the same time), shorter strips are padded with black LED frames
/// at the BEGINNING of the data stream.
///
/// Common padding patterns:
/// - **APA102/SK9822**: {0xE0, 0x00, 0x00, 0x00} (brightness=0, RGB=0)
/// - **LPD8806**: {0x80, 0x80, 0x80} (7-bit GRB with MSB=1, all colors 0)
/// - **WS2801**: {0x00, 0x00, 0x00} (RGB all zero)
/// - **P9813**: {0xFF, 0x00, 0x00, 0x00} (flag byte + BGR all zero)
///
/// These invisible black LEDs are prepended so all strips finish transmitting
/// simultaneously, providing synchronized visual updates across all parallel strips.
///
/// # Usage Example
///
/// @code
/// using namespace fl;
///
/// // Prepare lane data
/// vector<uint8_t> lane0_data = {0xAB, 0xCD, ...};
/// vector<uint8_t> lane1_data = {0x12, 0x34, ...};
/// span<const uint8_t> apa102_padding = APA102Controller<...>::getPaddingLEDFrame();
///
/// // Setup lanes (optional for unused lanes)
/// optional<SPITransposer::LaneData> lane0 = SPITransposer::LaneData{lane0_data, apa102_padding};
/// optional<SPITransposer::LaneData> lane1 = SPITransposer::LaneData{lane1_data, apa102_padding};
///
/// // Allocate output buffer (caller manages memory)
/// size_t max_size = fl::fl_max(lane0_data.size(), lane1_data.size());
/// vector<uint8_t> output(max_size * 2);  // 2× for 2-way, 4× for 4-way, 8× for 8-way
///
/// // Perform transpose
/// const char* error = nullptr;
/// bool success = SPITransposer::transpose2(lane0, lane1, output, &error);
/// if (!success) {
///     FL_WARN("Transpose failed: " << error);
/// }
/// @endcode
///
/// # Performance
///
/// - **CPU overhead**: Minimal - just the transpose operation (runs once per frame)
/// - **Transpose time**: ~25-100µs depending on lane count and data size
/// - **Transmission time**: Hardware DMA, zero CPU usage during transfer
/// - **Optimization**: Direct bit extraction provides optimal performance
class SPITransposer {
public:
    /// Lane data structure: payload + padding frame
    struct LaneData {
        fl::span<const uint8_t> payload;        ///< Actual LED data for this lane
        fl::span<const uint8_t> padding_frame;  ///< Black LED frame for padding (repeating pattern)
    };

    /// Transpose 2 lanes of data into interleaved dual-SPI format
    ///
    /// @param lane0 Lane 0 data (use fl::nullopt for unused lane)
    /// @param lane1 Lane 1 data (use fl::nullopt for unused lane)
    /// @param output Output buffer to write interleaved data (size must be divisible by 2)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 2
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    static bool transpose2(const fl::optional<LaneData>& lane0,
                          const fl::optional<LaneData>& lane1,
                          fl::span<uint8_t> output,
                          const char** error = nullptr);

    /// Transpose 4 lanes of data into interleaved quad-SPI format
    ///
    /// @param lane0 Lane 0 data (use fl::nullopt for unused lane)
    /// @param lane1 Lane 1 data (use fl::nullopt for unused lane)
    /// @param lane2 Lane 2 data (use fl::nullopt for unused lane)
    /// @param lane3 Lane 3 data (use fl::nullopt for unused lane)
    /// @param output Output buffer to write interleaved data (size must be divisible by 4)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 4
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    static bool transpose4(const fl::optional<LaneData>& lane0,
                          const fl::optional<LaneData>& lane1,
                          const fl::optional<LaneData>& lane2,
                          const fl::optional<LaneData>& lane3,
                          fl::span<uint8_t> output,
                          const char** error = nullptr);

    /// Transpose 8 lanes of data into interleaved octal-SPI format
    ///
    /// @param lanes Array of 8 lane data (use fl::nullopt for unused lanes)
    /// @param output Output buffer to write interleaved data (size must be divisible by 8)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 8
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    static bool transpose8(const fl::optional<LaneData> lanes[8],
                          fl::span<uint8_t> output,
                          const char** error = nullptr);

    /// Transpose 16 lanes of data into interleaved hex-SPI format
    ///
    /// @param lanes Array of 16 lane data (use fl::nullopt for unused lanes)
    /// @param output Output buffer to write interleaved data (size must be divisible by 16)
    /// @param error Optional pointer to receive error message (set to nullptr if unused)
    /// @return true on success, false if output buffer size is invalid
    ///
    /// @note Output buffer size determines max lane size: max_size = output.size() / 16
    /// @note Shorter lanes are padded at the beginning with repeating padding_frame pattern
    /// @note Empty lanes (nullopt) are filled with zeros or first lane's padding
    static bool transpose16(const fl::optional<LaneData> lanes[16],
                           fl::span<uint8_t> output,
                           const char** error = nullptr);

private:
    /// Get byte from lane at given index, handling padding automatically
    static uint8_t getLaneByte(const LaneData& lane, size_t byte_idx, size_t max_size);
};

// ============================================================================
// Parallel Strip Transposer (RP2040/RP2350 PIO)
// ============================================================================

/// @brief Transpose 8 LED strips into parallel bit format
///
/// This function transposes 8 LED strips from standard byte-sequential format
/// to bit-parallel format suitable for 8-way PIO output. It uses the highly
/// optimized transpose8x1_MSB() function (Hacker's Delight algorithm).
///
/// **Input:** 8 strips, each with `num_leds * bytes_per_led` bytes
/// **Output:** `num_leds * bytes_per_led * 8` bytes
///
/// @param input Array of 8 pointers to LED strip data
/// @param output Pointer to output buffer
/// @param num_leds Number of LEDs per strip (all strips padded to this length)
/// @param bytes_per_led Number of bytes per LED (3 for RGB, 4 for RGBW, etc.)
///
/// @note All strips must be pre-padded to the same length
/// @note Output buffer must be pre-allocated by caller
FASTLED_FORCE_INLINE void transpose_8strips(
    const u8* const input[8],
    u8* output,
    u16 num_leds,
    u8 bytes_per_led
) {
    // Process each LED
    for (u16 led = 0; led < num_leds; led++) {
        u8 temp_input[8];

        // Process each byte in the LED
        for (u8 byte_idx = 0; byte_idx < bytes_per_led; byte_idx++) {
            // Collect one byte from each strip for this byte position
            for (int strip = 0; strip < 8; strip++) {
                temp_input[strip] = input[strip][led * bytes_per_led + byte_idx];
            }

            // Transpose 8 bytes → 8 bytes (1 bit from each strip per output byte)
            transpose8x1_MSB(temp_input, output);

            // Advance output pointer by 8 bytes
            output += 8;
        }
    }
}

/// @brief Transpose 4 LED strips into parallel bit format
///
/// This function transposes 4 LED strips from standard byte-sequential format
/// to bit-parallel format suitable for 4-way PIO output.
///
/// **Input:** 4 strips, each with `num_leds * bytes_per_led` bytes
/// **Output:** `num_leds * bytes_per_led * 8` bytes
///
/// @param input Array of 4 pointers to LED strip data
/// @param output Pointer to output buffer
/// @param num_leds Number of LEDs per strip (all strips padded to this length)
/// @param bytes_per_led Number of bytes per LED (3 for RGB, 4 for RGBW, etc.)
///
/// @note All strips must be pre-padded to the same length
/// @note Output buffer must be pre-allocated by caller
/// @note Upper 4 bits of each output byte are zero
FASTLED_FORCE_INLINE void transpose_4strips(
    const u8* const input[4],
    u8* output,
    u16 num_leds,
    u8 bytes_per_led
) {
    // Process each LED
    for (u16 led = 0; led < num_leds; led++) {
        // Process each byte in the LED
        for (u8 byte_idx = 0; byte_idx < bytes_per_led; byte_idx++) {
            // Collect one byte from each strip for this byte position
            u8 strip_bytes[4];
            for (int strip = 0; strip < 4; strip++) {
                strip_bytes[strip] = input[strip][led * bytes_per_led + byte_idx];
            }

            // Transpose: extract each bit position from all 4 strips
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
/// to bit-parallel format suitable for 2-way PIO output.
///
/// **Input:** 2 strips, each with `num_leds * bytes_per_led` bytes
/// **Output:** `num_leds * bytes_per_led * 8` bytes
///
/// @param input Array of 2 pointers to LED strip data
/// @param output Pointer to output buffer
/// @param num_leds Number of LEDs per strip (all strips padded to this length)
/// @param bytes_per_led Number of bytes per LED (3 for RGB, 4 for RGBW, etc.)
///
/// @note All strips must be pre-padded to the same length
/// @note Output buffer must be pre-allocated by caller
/// @note Upper 6 bits of each output byte are zero
FASTLED_FORCE_INLINE void transpose_2strips(
    const u8* const input[2],
    u8* output,
    u16 num_leds,
    u8 bytes_per_led
) {
    // Process each LED
    for (u16 led = 0; led < num_leds; led++) {
        // Process each byte in the LED
        for (u8 byte_idx = 0; byte_idx < bytes_per_led; byte_idx++) {
            // Collect one byte from each strip for this byte position
            u8 strip_bytes[2];
            strip_bytes[0] = input[0][led * bytes_per_led + byte_idx];
            strip_bytes[1] = input[1][led * bytes_per_led + byte_idx];

            // Transpose: extract each bit position from both strips
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
/// All strip counts (2, 4, 8) use the same output format: bytes_per_led * 8 bytes per LED.
///
/// @param num_leds Maximum number of LEDs across all strips
/// @param bytes_per_led Number of bytes per LED (3 for RGB, 4 for RGBW, etc.)
/// @return Required output buffer size in bytes
FASTLED_FORCE_INLINE u32 calculate_transpose_buffer_size(u16 num_leds, u8 bytes_per_led) {
    return num_leds * bytes_per_led * 8;
}

/// @brief Helper to transpose N strips with automatic dispatch
///
/// @param num_strips Number of strips (must be 2, 4, or 8)
/// @param input Array of pointers to LED strip data
/// @param output Pointer to output buffer
/// @param num_leds Number of LEDs per strip
/// @param bytes_per_led Number of bytes per LED (3 for RGB, 4 for RGBW, etc.)
/// @return true if successful, false if invalid strip count
inline bool transpose_strips(
    u8 num_strips,
    const u8* const* input,
    u8* output,
    u16 num_leds,
    u8 bytes_per_led
) {
    switch (num_strips) {
        case 8:
            transpose_8strips(input, output, num_leds, bytes_per_led);
            return true;
        case 4:
            transpose_4strips(input, output, num_leds, bytes_per_led);
            return true;
        case 2:
            transpose_2strips(input, output, num_leds, bytes_per_led);
            return true;
        default:
            return false;  // Invalid strip count
    }
}

// ============================================================================
// PARLIO Wave8 Transposer (ESP32-S3 Parallel I/O)
// ============================================================================

/// @brief Template specialization of transpose for compile-time data_width (optimization)
///
/// This template version eliminates runtime branching by specializing for each data width.
/// The compiler generates optimized code for each DATA_WIDTH value at compile time.
///
/// @tparam DATA_WIDTH Number of parallel lanes (1, 2, 4, 8, or 16) - compile-time constant
template<size_t DATA_WIDTH>
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION size_t transpose_wave8byte_parlio_template(
    const uint8_t* FL_RESTRICT_PARAM laneWaveforms,
    uint8_t* FL_RESTRICT_PARAM outputBuffer
) {
    constexpr size_t bytes_per_lane = 8;   // sizeof(Wave8Byte)
    constexpr size_t pulsesPerByte = 64;   // 8 bits × 8 pulses per bit
    size_t outputIdx = 0;

    // Note: Using regular if statements (C++11 compatible)
    // Compiler optimizes away dead branches for constant template parameters
    if (DATA_WIDTH == 8) {
        // Special optimized case for 8 lanes with bit packing
        // Optimized: Hoist packing outside inner loop to reduce redundant operations
        for (size_t bit_pos = 0; bit_pos < 8; bit_pos++) {
            // Pack 8 wave8_byte values into a single 64-bit register for parallel extraction
            // This packing is done once per bit_pos (8 times) instead of 64 times
            uint64_t packed =
                ((uint64_t)laneWaveforms[0 * bytes_per_lane + bit_pos] << 0)  |
                ((uint64_t)laneWaveforms[1 * bytes_per_lane + bit_pos] << 8)  |
                ((uint64_t)laneWaveforms[2 * bytes_per_lane + bit_pos] << 16) |
                ((uint64_t)laneWaveforms[3 * bytes_per_lane + bit_pos] << 24) |
                ((uint64_t)laneWaveforms[4 * bytes_per_lane + bit_pos] << 32) |
                ((uint64_t)laneWaveforms[5 * bytes_per_lane + bit_pos] << 40) |
                ((uint64_t)laneWaveforms[6 * bytes_per_lane + bit_pos] << 48) |
                ((uint64_t)laneWaveforms[7 * bytes_per_lane + bit_pos] << 56);

            // Inner loop: extract 8 pulses from the packed data
            for (size_t pulse_bit = 0; pulse_bit < 8; pulse_bit++) {
                // Extract pulse bits in parallel (compiler can optimize independent shifts)
                outputBuffer[outputIdx++] =
                    ((packed >> (7 - pulse_bit + 0))  & 0x01) << 0 |
                    ((packed >> (7 - pulse_bit + 8))  & 0x01) << 1 |
                    ((packed >> (7 - pulse_bit + 16)) & 0x01) << 2 |
                    ((packed >> (7 - pulse_bit + 24)) & 0x01) << 3 |
                    ((packed >> (7 - pulse_bit + 32)) & 0x01) << 4 |
                    ((packed >> (7 - pulse_bit + 40)) & 0x01) << 5 |
                    ((packed >> (7 - pulse_bit + 48)) & 0x01) << 6 |
                    ((packed >> (7 - pulse_bit + 56)) & 0x01) << 7;
            }
        }
    } else if (DATA_WIDTH <= 8) {
        // Pack into single bytes (compile-time branch elimination via template instantiation)
        const size_t ticksPerByte = 8 / DATA_WIDTH;
        const size_t numOutputBytes = (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;

        for (size_t outputByteIdx = 0; outputByteIdx < numOutputBytes; outputByteIdx++) {
            uint8_t outputByte = 0;

            FL_UNROLL(8)
            for (size_t t = 0; t < ticksPerByte; t++) {
                size_t pulse_idx = outputByteIdx * ticksPerByte + t;
                if (pulse_idx >= pulsesPerByte)
                    break;

                size_t bit_pos = pulse_idx / 8;
                size_t pulse_bit = pulse_idx % 8;

                FL_UNROLL(8)
                for (size_t lane = 0; lane < DATA_WIDTH; lane++) {
                    const uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);
                    uint8_t wave8_byte = laneWaveform[bit_pos];
                    uint8_t pulse = (wave8_byte >> (7 - pulse_bit)) & 1;

                    size_t bitPos = t * DATA_WIDTH + lane;
                    outputByte |= (pulse << bitPos);
                }
            }

            outputBuffer[outputIdx++] = outputByte;
        }
    } else if (DATA_WIDTH == 16) {
        // Pack into 16-bit words (compile-time branch)
        // Optimized: Software pipelining + output buffering
        // Process 2 bit positions in parallel for better ILP, and batch writes for better cache efficiency

        // Output buffer: accumulate 16 words (32 bytes) before writing
        // This aligns with typical 32-byte cache lines and reduces memory write overhead
        uint8_t writeBuffer[32];
        size_t writeIdx = 0;

        for (size_t bit_pos = 0; bit_pos < 8; bit_pos += 2) {
            // Pack 16 wave8_byte values for TWO bit positions simultaneously
            // This enables instruction-level parallelism and better register utilization
            uint64_t packed_lo_0 =
                ((uint64_t)laneWaveforms[0 * bytes_per_lane + bit_pos + 0] << 0)  |
                ((uint64_t)laneWaveforms[1 * bytes_per_lane + bit_pos + 0] << 8)  |
                ((uint64_t)laneWaveforms[2 * bytes_per_lane + bit_pos + 0] << 16) |
                ((uint64_t)laneWaveforms[3 * bytes_per_lane + bit_pos + 0] << 24) |
                ((uint64_t)laneWaveforms[4 * bytes_per_lane + bit_pos + 0] << 32) |
                ((uint64_t)laneWaveforms[5 * bytes_per_lane + bit_pos + 0] << 40) |
                ((uint64_t)laneWaveforms[6 * bytes_per_lane + bit_pos + 0] << 48) |
                ((uint64_t)laneWaveforms[7 * bytes_per_lane + bit_pos + 0] << 56);

            uint64_t packed_hi_0 =
                ((uint64_t)laneWaveforms[8  * bytes_per_lane + bit_pos + 0] << 0)  |
                ((uint64_t)laneWaveforms[9  * bytes_per_lane + bit_pos + 0] << 8)  |
                ((uint64_t)laneWaveforms[10 * bytes_per_lane + bit_pos + 0] << 16) |
                ((uint64_t)laneWaveforms[11 * bytes_per_lane + bit_pos + 0] << 24) |
                ((uint64_t)laneWaveforms[12 * bytes_per_lane + bit_pos + 0] << 32) |
                ((uint64_t)laneWaveforms[13 * bytes_per_lane + bit_pos + 0] << 40) |
                ((uint64_t)laneWaveforms[14 * bytes_per_lane + bit_pos + 0] << 48) |
                ((uint64_t)laneWaveforms[15 * bytes_per_lane + bit_pos + 0] << 56);

            uint64_t packed_lo_1 =
                ((uint64_t)laneWaveforms[0 * bytes_per_lane + bit_pos + 1] << 0)  |
                ((uint64_t)laneWaveforms[1 * bytes_per_lane + bit_pos + 1] << 8)  |
                ((uint64_t)laneWaveforms[2 * bytes_per_lane + bit_pos + 1] << 16) |
                ((uint64_t)laneWaveforms[3 * bytes_per_lane + bit_pos + 1] << 24) |
                ((uint64_t)laneWaveforms[4 * bytes_per_lane + bit_pos + 1] << 32) |
                ((uint64_t)laneWaveforms[5 * bytes_per_lane + bit_pos + 1] << 40) |
                ((uint64_t)laneWaveforms[6 * bytes_per_lane + bit_pos + 1] << 48) |
                ((uint64_t)laneWaveforms[7 * bytes_per_lane + bit_pos + 1] << 56);

            uint64_t packed_hi_1 =
                ((uint64_t)laneWaveforms[8  * bytes_per_lane + bit_pos + 1] << 0)  |
                ((uint64_t)laneWaveforms[9  * bytes_per_lane + bit_pos + 1] << 8)  |
                ((uint64_t)laneWaveforms[10 * bytes_per_lane + bit_pos + 1] << 16) |
                ((uint64_t)laneWaveforms[11 * bytes_per_lane + bit_pos + 1] << 24) |
                ((uint64_t)laneWaveforms[12 * bytes_per_lane + bit_pos + 1] << 32) |
                ((uint64_t)laneWaveforms[13 * bytes_per_lane + bit_pos + 1] << 40) |
                ((uint64_t)laneWaveforms[14 * bytes_per_lane + bit_pos + 1] << 48) |
                ((uint64_t)laneWaveforms[15 * bytes_per_lane + bit_pos + 1] << 56);

            // Inner loop: interleave extraction from both bit positions
            // This allows CPU to execute independent operations in parallel
            for (size_t pulse_bit = 0; pulse_bit < 8; pulse_bit++) {
                // Extract pulse bits for first bit position
                uint16_t outputWord_0 =
                    ((packed_lo_0 >> (7 - pulse_bit + 0))  & 0x01) << 0  |
                    ((packed_lo_0 >> (7 - pulse_bit + 8))  & 0x01) << 1  |
                    ((packed_lo_0 >> (7 - pulse_bit + 16)) & 0x01) << 2  |
                    ((packed_lo_0 >> (7 - pulse_bit + 24)) & 0x01) << 3  |
                    ((packed_lo_0 >> (7 - pulse_bit + 32)) & 0x01) << 4  |
                    ((packed_lo_0 >> (7 - pulse_bit + 40)) & 0x01) << 5  |
                    ((packed_lo_0 >> (7 - pulse_bit + 48)) & 0x01) << 6  |
                    ((packed_lo_0 >> (7 - pulse_bit + 56)) & 0x01) << 7  |
                    ((packed_hi_0 >> (7 - pulse_bit + 0))  & 0x01) << 8  |
                    ((packed_hi_0 >> (7 - pulse_bit + 8))  & 0x01) << 9  |
                    ((packed_hi_0 >> (7 - pulse_bit + 16)) & 0x01) << 10 |
                    ((packed_hi_0 >> (7 - pulse_bit + 24)) & 0x01) << 11 |
                    ((packed_hi_0 >> (7 - pulse_bit + 32)) & 0x01) << 12 |
                    ((packed_hi_0 >> (7 - pulse_bit + 40)) & 0x01) << 13 |
                    ((packed_hi_0 >> (7 - pulse_bit + 48)) & 0x01) << 14 |
                    ((packed_hi_0 >> (7 - pulse_bit + 56)) & 0x01) << 15;

                // Extract pulse bits for second bit position
                uint16_t outputWord_1 =
                    ((packed_lo_1 >> (7 - pulse_bit + 0))  & 0x01) << 0  |
                    ((packed_lo_1 >> (7 - pulse_bit + 8))  & 0x01) << 1  |
                    ((packed_lo_1 >> (7 - pulse_bit + 16)) & 0x01) << 2  |
                    ((packed_lo_1 >> (7 - pulse_bit + 24)) & 0x01) << 3  |
                    ((packed_lo_1 >> (7 - pulse_bit + 32)) & 0x01) << 4  |
                    ((packed_lo_1 >> (7 - pulse_bit + 40)) & 0x01) << 5  |
                    ((packed_lo_1 >> (7 - pulse_bit + 48)) & 0x01) << 6  |
                    ((packed_lo_1 >> (7 - pulse_bit + 56)) & 0x01) << 7  |
                    ((packed_hi_1 >> (7 - pulse_bit + 0))  & 0x01) << 8  |
                    ((packed_hi_1 >> (7 - pulse_bit + 8))  & 0x01) << 9  |
                    ((packed_hi_1 >> (7 - pulse_bit + 16)) & 0x01) << 10 |
                    ((packed_hi_1 >> (7 - pulse_bit + 24)) & 0x01) << 11 |
                    ((packed_hi_1 >> (7 - pulse_bit + 32)) & 0x01) << 12 |
                    ((packed_hi_1 >> (7 - pulse_bit + 40)) & 0x01) << 13 |
                    ((packed_hi_1 >> (7 - pulse_bit + 48)) & 0x01) << 14 |
                    ((packed_hi_1 >> (7 - pulse_bit + 56)) & 0x01) << 15;

                // Write to buffer instead of directly to output
                writeBuffer[writeIdx++] = outputWord_0 & 0xFF;
                writeBuffer[writeIdx++] = (outputWord_0 >> 8) & 0xFF;
                writeBuffer[writeIdx++] = outputWord_1 & 0xFF;
                writeBuffer[writeIdx++] = (outputWord_1 >> 8) & 0xFF;
            }

            // Flush buffer when full (16 words = 32 bytes)
            // This triggers efficient burst writes that align with cache lines
            if (writeIdx == 32) {
                fl::memcpy(&outputBuffer[outputIdx], writeBuffer, 32);
                outputIdx += 32;
                writeIdx = 0;
            }
        }
    } else {
        // Invalid DATA_WIDTH (compile-time error if template instantiated with wrong value)
        return 0;
    }

    return outputIdx;
}

/// @brief Transpose Wave8Byte waveforms into PARLIO bit-parallel format (ISR-safe)
///
/// This function transposes Wave8Byte waveform data (8 bytes per lane representing
/// 64 pulses) into PARLIO's bit-packed parallel format for multi-lane transmission.
///
/// **Input Format:** Wave8Byte structures
/// - Each lane: 8 bytes (Wave8Bit symbols[8])
/// - Each byte: 8 pulses packed MSB-first
/// - Total: 64 pulses per input byte (8 bits × 8 pulses per bit)
///
/// **Output Format:** PARLIO bit-parallel format
/// - data_width ≤ 8: Bit-packed bytes (multiple pulses per byte)
/// - data_width == 16: 16-bit words (one pulse per word, 2 bytes)
///
/// **Supported Data Widths:** 1, 2, 4, 8, 16 lanes
///
/// @param laneWaveforms Array of Wave8Byte data (data_width lanes × 8 bytes each)
/// @param data_width Number of parallel lanes (1, 2, 4, 8, or 16)
/// @param outputBuffer Output buffer for bit-packed data
/// @return Number of bytes written to output buffer (8 for ≤8 lanes, 16 for 16 lanes)
///
/// @note ISR-safe: inline function, no allocations
/// @note Output size: 8 bytes (data_width ≤ 8) or 16 bytes (data_width = 16)
/// @note Returns 0 if data_width is invalid (not 1, 2, 4, 8, or 16)
///
/// @example
/// ```cpp
/// uint8_t laneWaveforms[16 * 8];  // 16 lanes × 8 bytes
/// uint8_t output[16];              // Max output size
/// size_t written = transpose_wave8byte_parlio(laneWaveforms, 8, output);
/// ```
FASTLED_FORCE_INLINE FL_IRAM size_t transpose_wave8byte_parlio(
    const uint8_t* FL_RESTRICT_PARAM laneWaveforms,
    size_t data_width,
    uint8_t* FL_RESTRICT_PARAM outputBuffer
) {
    // Dispatch to template specialization based on runtime data_width
    // Compiler generates optimized code for each specialization (no runtime branching)
    switch (data_width) {
        case 1:
            return transpose_wave8byte_parlio_template<1>(laneWaveforms, outputBuffer);
        case 2:
            return transpose_wave8byte_parlio_template<2>(laneWaveforms, outputBuffer);
        case 4:
            return transpose_wave8byte_parlio_template<4>(laneWaveforms, outputBuffer);
        case 8:
            return transpose_wave8byte_parlio_template<8>(laneWaveforms, outputBuffer);
        case 16:
            return transpose_wave8byte_parlio_template<16>(laneWaveforms, outputBuffer);
        default:
            // Invalid data_width
            return 0;
    }
}

}  // namespace fl
