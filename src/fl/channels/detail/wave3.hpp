/// @file wave3.hpp
/// @brief Inline implementation details for wave3 transposition
///
/// This header contains force-inlined implementations of wave3 transposition
/// functions for optimal performance in ISR/DMA contexts.

#pragma once

#include "fl/channels/wave3.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/isr/memcpy.h"

namespace fl {

namespace detail {

// ============================================================================
// Byte to Wave3Byte Conversion (Force Inline Helper)
// ============================================================================

/// @brief Helper: Convert byte to Wave3Byte using nibble LUT (internal use only)
/// @note Inline implementation for ISR performance
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_convert_byte_to_wave3byte(u8 byte_value,
                                      const Wave3BitExpansionLut& lut,
                                      Wave3Byte* output) {
    // High nibble → 12 bits, low nibble → 12 bits, total 24 bits = 3 bytes
    u16 high_pattern = lut.lut[(byte_value >> 4) & 0xF];
    u16 low_pattern = lut.lut[byte_value & 0xF];

    // Pack 24 bits into 3 bytes (MSB first):
    // Byte 0: bits [23..16] = high_pattern[11..4]
    // Byte 1: bits [15..8]  = high_pattern[3..0] | low_pattern[11..8]
    // Byte 2: bits [7..0]   = low_pattern[7..0]
    output->data[0] = (u8)(high_pattern >> 4);
    output->data[1] = (u8)(((high_pattern & 0xF) << 4) | ((low_pattern >> 8) & 0xF));
    output->data[2] = (u8)(low_pattern & 0xFF);
}

// ============================================================================
// 2-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 2 lanes of Wave3Byte data into interleaved format
/// @param lane_waves Array of 2 Wave3Byte structures
/// @param output Output buffer (6 bytes = 2 * 3)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_2(const Wave3Byte lane_waves[2],
                       u8 output[2 * sizeof(Wave3Byte)]) {
    // Wave3 produces 3 symbols per LED byte (one byte per symbol for each lane).
    // For 2 lanes: interleave bits from both lanes into 2 bytes per symbol.
    // Each symbol byte has 8 bits. With 2 lanes we produce 2 bytes per symbol
    // using the same bit-spreading as wave8_transpose_2.

    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 l0 = lane_waves[0].data[symbol_idx];
        u8 l1 = lane_waves[1].data[symbol_idx];

        // Interleave bits: for each bit position, lane0 goes to odd bits, lane1 to even bits
        u16 interleaved = 0;
        for (int bit = 0; bit < 8; bit++) {
            u16 b0 = (l0 >> bit) & 1;
            u16 b1 = (l1 >> bit) & 1;
            interleaved |= (b1 << (bit * 2));       // even positions
            interleaved |= (b0 << (bit * 2 + 1));   // odd positions
        }

        output[symbol_idx * 2] = (u8)(interleaved >> 8);
        output[symbol_idx * 2 + 1] = (u8)(interleaved & 0xFF);
    }
}

// ============================================================================
// 4-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 4 lanes of Wave3Byte data into interleaved format
/// @param lane_waves Array of 4 Wave3Byte structures
/// @param output Output buffer (12 bytes = 4 * 3)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_4(const Wave3Byte lane_waves[4],
                       u8 output[4 * sizeof(Wave3Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 l0 = lane_waves[0].data[symbol_idx];
        u8 l1 = lane_waves[1].data[symbol_idx];
        u8 l2 = lane_waves[2].data[symbol_idx];
        u8 l3 = lane_waves[3].data[symbol_idx];

        // 4 output bytes per symbol (2 pulses per byte × 4 lanes)
        // Same bit layout as wave8_transpose_4
        output[symbol_idx * 4 + 0] =
            ((l3 >> 7) & 1) << 7 |
            ((l2 >> 7) & 1) << 6 |
            ((l1 >> 7) & 1) << 5 |
            ((l0 >> 7) & 1) << 4 |
            ((l3 >> 6) & 1) << 3 |
            ((l2 >> 6) & 1) << 2 |
            ((l1 >> 6) & 1) << 1 |
            ((l0 >> 6) & 1);

        output[symbol_idx * 4 + 1] =
            ((l3 >> 5) & 1) << 7 |
            ((l2 >> 5) & 1) << 6 |
            ((l1 >> 5) & 1) << 5 |
            ((l0 >> 5) & 1) << 4 |
            ((l3 >> 4) & 1) << 3 |
            ((l2 >> 4) & 1) << 2 |
            ((l1 >> 4) & 1) << 1 |
            ((l0 >> 4) & 1);

        output[symbol_idx * 4 + 2] =
            ((l3 >> 3) & 1) << 7 |
            ((l2 >> 3) & 1) << 6 |
            ((l1 >> 3) & 1) << 5 |
            ((l0 >> 3) & 1) << 4 |
            ((l3 >> 2) & 1) << 3 |
            ((l2 >> 2) & 1) << 2 |
            ((l1 >> 2) & 1) << 1 |
            ((l0 >> 2) & 1);

        output[symbol_idx * 4 + 3] =
            ((l3 >> 1) & 1) << 7 |
            ((l2 >> 1) & 1) << 6 |
            ((l1 >> 1) & 1) << 5 |
            ((l0 >> 1) & 1) << 4 |
            ((l3 >> 0) & 1) << 3 |
            ((l2 >> 0) & 1) << 2 |
            ((l1 >> 0) & 1) << 1 |
            ((l0 >> 0) & 1);
    }
}

// ============================================================================
// 8-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 8 lanes of Wave3Byte data into interleaved format
/// @param lane_waves Array of 8 Wave3Byte structures
/// @param output Output buffer (24 bytes = 8 * 3)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_8(const Wave3Byte lane_waves[8],
                       u8 output[8 * sizeof(Wave3Byte)]) {
    // For 8 lanes, each symbol produces 8 bytes (1 byte per pulse, 8 pulses per symbol)
    // Use Hacker's Delight 8x8 transpose like wave8
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 lane_bytes[8];
        for (int lane = 0; lane < 8; lane++) {
            lane_bytes[lane] = lane_waves[lane].data[symbol_idx];
        }

        u32 x, y, t;
        isr::memcpy_32(&y, reinterpret_cast<const u32*>(lane_bytes), 1);       // ok reinterpret cast - lanes 0-3
        isr::memcpy_32(&x, reinterpret_cast<const u32*>(lane_bytes + 4), 1);   // ok reinterpret cast - lanes 4-7

        t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
        t = (x ^ (x >> 14)) & 0x0000CCCC;  x = x ^ t ^ (t << 14);
        t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
        t = (y ^ (y >> 14)) & 0x0000CCCC;  y = y ^ t ^ (t << 14);
        t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
        y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
        x = t;

        isr::memcpy_32(reinterpret_cast<u32*>(output + symbol_idx * 8), &y, 1);     // ok reinterpret cast - output byte array to u32
        isr::memcpy_32(reinterpret_cast<u32*>(output + symbol_idx * 8 + 4), &x, 1); // ok reinterpret cast - output byte array to u32
    }
}

// ============================================================================
// 16-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 16 lanes of Wave3Byte data into interleaved format
/// @param lane_waves Array of 16 Wave3Byte structures
/// @param output Output buffer (48 bytes = 16 * 3)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_16(const Wave3Byte lane_waves[16],
                        u8 output[16 * sizeof(Wave3Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        const u8 l0  = lane_waves[0].data[symbol_idx];
        const u8 l1  = lane_waves[1].data[symbol_idx];
        const u8 l2  = lane_waves[2].data[symbol_idx];
        const u8 l3  = lane_waves[3].data[symbol_idx];
        const u8 l4  = lane_waves[4].data[symbol_idx];
        const u8 l5  = lane_waves[5].data[symbol_idx];
        const u8 l6  = lane_waves[6].data[symbol_idx];
        const u8 l7  = lane_waves[7].data[symbol_idx];
        const u8 l8  = lane_waves[8].data[symbol_idx];
        const u8 l9  = lane_waves[9].data[symbol_idx];
        const u8 l10 = lane_waves[10].data[symbol_idx];
        const u8 l11 = lane_waves[11].data[symbol_idx];
        const u8 l12 = lane_waves[12].data[symbol_idx];
        const u8 l13 = lane_waves[13].data[symbol_idx];
        const u8 l14 = lane_waves[14].data[symbol_idx];
        const u8 l15 = lane_waves[15].data[symbol_idx];

        u8* out = &output[symbol_idx * 16];

        // 8 pulses per symbol, 2 bytes per pulse (low byte = lanes 0-7, high byte = lanes 8-15)
        out[0] = ((l7  >> 7) & 1) << 7 | ((l6  >> 7) & 1) << 6 | ((l5  >> 7) & 1) << 5 | ((l4  >> 7) & 1) << 4 |
                 ((l3  >> 7) & 1) << 3 | ((l2  >> 7) & 1) << 2 | ((l1  >> 7) & 1) << 1 | ((l0  >> 7) & 1);
        out[1] = ((l15 >> 7) & 1) << 7 | ((l14 >> 7) & 1) << 6 | ((l13 >> 7) & 1) << 5 | ((l12 >> 7) & 1) << 4 |
                 ((l11 >> 7) & 1) << 3 | ((l10 >> 7) & 1) << 2 | ((l9  >> 7) & 1) << 1 | ((l8  >> 7) & 1);

        out[2] = ((l7  >> 6) & 1) << 7 | ((l6  >> 6) & 1) << 6 | ((l5  >> 6) & 1) << 5 | ((l4  >> 6) & 1) << 4 |
                 ((l3  >> 6) & 1) << 3 | ((l2  >> 6) & 1) << 2 | ((l1  >> 6) & 1) << 1 | ((l0  >> 6) & 1);
        out[3] = ((l15 >> 6) & 1) << 7 | ((l14 >> 6) & 1) << 6 | ((l13 >> 6) & 1) << 5 | ((l12 >> 6) & 1) << 4 |
                 ((l11 >> 6) & 1) << 3 | ((l10 >> 6) & 1) << 2 | ((l9  >> 6) & 1) << 1 | ((l8  >> 6) & 1);

        out[4] = ((l7  >> 5) & 1) << 7 | ((l6  >> 5) & 1) << 6 | ((l5  >> 5) & 1) << 5 | ((l4  >> 5) & 1) << 4 |
                 ((l3  >> 5) & 1) << 3 | ((l2  >> 5) & 1) << 2 | ((l1  >> 5) & 1) << 1 | ((l0  >> 5) & 1);
        out[5] = ((l15 >> 5) & 1) << 7 | ((l14 >> 5) & 1) << 6 | ((l13 >> 5) & 1) << 5 | ((l12 >> 5) & 1) << 4 |
                 ((l11 >> 5) & 1) << 3 | ((l10 >> 5) & 1) << 2 | ((l9  >> 5) & 1) << 1 | ((l8  >> 5) & 1);

        out[6] = ((l7  >> 4) & 1) << 7 | ((l6  >> 4) & 1) << 6 | ((l5  >> 4) & 1) << 5 | ((l4  >> 4) & 1) << 4 |
                 ((l3  >> 4) & 1) << 3 | ((l2  >> 4) & 1) << 2 | ((l1  >> 4) & 1) << 1 | ((l0  >> 4) & 1);
        out[7] = ((l15 >> 4) & 1) << 7 | ((l14 >> 4) & 1) << 6 | ((l13 >> 4) & 1) << 5 | ((l12 >> 4) & 1) << 4 |
                 ((l11 >> 4) & 1) << 3 | ((l10 >> 4) & 1) << 2 | ((l9  >> 4) & 1) << 1 | ((l8  >> 4) & 1);

        out[8] = ((l7  >> 3) & 1) << 7 | ((l6  >> 3) & 1) << 6 | ((l5  >> 3) & 1) << 5 | ((l4  >> 3) & 1) << 4 |
                 ((l3  >> 3) & 1) << 3 | ((l2  >> 3) & 1) << 2 | ((l1  >> 3) & 1) << 1 | ((l0  >> 3) & 1);
        out[9] = ((l15 >> 3) & 1) << 7 | ((l14 >> 3) & 1) << 6 | ((l13 >> 3) & 1) << 5 | ((l12 >> 3) & 1) << 4 |
                 ((l11 >> 3) & 1) << 3 | ((l10 >> 3) & 1) << 2 | ((l9  >> 3) & 1) << 1 | ((l8  >> 3) & 1);

        out[10] = ((l7  >> 2) & 1) << 7 | ((l6  >> 2) & 1) << 6 | ((l5  >> 2) & 1) << 5 | ((l4  >> 2) & 1) << 4 |
                  ((l3  >> 2) & 1) << 3 | ((l2  >> 2) & 1) << 2 | ((l1  >> 2) & 1) << 1 | ((l0  >> 2) & 1);
        out[11] = ((l15 >> 2) & 1) << 7 | ((l14 >> 2) & 1) << 6 | ((l13 >> 2) & 1) << 5 | ((l12 >> 2) & 1) << 4 |
                  ((l11 >> 2) & 1) << 3 | ((l10 >> 2) & 1) << 2 | ((l9  >> 2) & 1) << 1 | ((l8  >> 2) & 1);

        out[12] = ((l7  >> 1) & 1) << 7 | ((l6  >> 1) & 1) << 6 | ((l5  >> 1) & 1) << 5 | ((l4  >> 1) & 1) << 4 |
                  ((l3  >> 1) & 1) << 3 | ((l2  >> 1) & 1) << 2 | ((l1  >> 1) & 1) << 1 | ((l0  >> 1) & 1);
        out[13] = ((l15 >> 1) & 1) << 7 | ((l14 >> 1) & 1) << 6 | ((l13 >> 1) & 1) << 5 | ((l12 >> 1) & 1) << 4 |
                  ((l11 >> 1) & 1) << 3 | ((l10 >> 1) & 1) << 2 | ((l9  >> 1) & 1) << 1 | ((l8  >> 1) & 1);

        out[14] = ((l7  >> 0) & 1) << 7 | ((l6  >> 0) & 1) << 6 | ((l5  >> 0) & 1) << 5 | ((l4  >> 0) & 1) << 4 |
                  ((l3  >> 0) & 1) << 3 | ((l2  >> 0) & 1) << 2 | ((l1  >> 0) & 1) << 1 | ((l0  >> 0) & 1);
        out[15] = ((l15 >> 0) & 1) << 7 | ((l14 >> 0) & 1) << 6 | ((l13 >> 0) & 1) << 5 | ((l12 >> 0) & 1) << 4 |
                  ((l11 >> 0) & 1) << 3 | ((l10 >> 0) & 1) << 2 | ((l9  >> 0) & 1) << 1 | ((l8  >> 0) & 1);
    }
}

} // namespace detail

// ============================================================================
// Public wave3() Function (Force Inline)
// ============================================================================

/// @brief Convert byte to 3 wave3 bytes using nibble LUT
/// @note Inline implementation for ISR performance
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3(u8 lane,
           const Wave3BitExpansionLut& lut,
           u8 (&FL_RESTRICT_PARAM output)[sizeof(Wave3Byte)]) {
    Wave3Byte wave3_output;
    detail::wave3_convert_byte_to_wave3byte(lane, lut, &wave3_output);
    output[0] = wave3_output.data[0];
    output[1] = wave3_output.data[1];
    output[2] = wave3_output.data[2];
}

} // namespace fl
