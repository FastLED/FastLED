/// @file wave3.hpp
/// @brief Inline implementation details for wave3 transposition
///
/// This header contains force-inlined implementations of wave3 transposition
/// functions for optimal performance in ISR/DMA contexts.

#pragma once

#include "fl/channels/wave3.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/isr/memcpy.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

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
///
/// Fully-unrolled naive bit-by-bit transpose — benchmarked (#2524) as the
/// fastest variant on the ESP32-P4 (RV32), beating a u32 Hacker's-Delight 8x8.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_8(const Wave3Byte lane_waves[8],
                       u8 output[8 * sizeof(Wave3Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        const u8 l0 = lane_waves[0].data[symbol_idx];
        const u8 l1 = lane_waves[1].data[symbol_idx];
        const u8 l2 = lane_waves[2].data[symbol_idx];
        const u8 l3 = lane_waves[3].data[symbol_idx];
        const u8 l4 = lane_waves[4].data[symbol_idx];
        const u8 l5 = lane_waves[5].data[symbol_idx];
        const u8 l6 = lane_waves[6].data[symbol_idx];
        const u8 l7 = lane_waves[7].data[symbol_idx];
        u8* out = &output[symbol_idx * 8];
#define FL_W3_PULSE(b) static_cast<u8>(((l7 >> (b)) & 1) << 7 | ((l6 >> (b)) & 1) << 6 | \
    ((l5 >> (b)) & 1) << 5 | ((l4 >> (b)) & 1) << 4 | ((l3 >> (b)) & 1) << 3 | \
    ((l2 >> (b)) & 1) << 2 | ((l1 >> (b)) & 1) << 1 | ((l0 >> (b)) & 1))
        out[0] = FL_W3_PULSE(7);
        out[1] = FL_W3_PULSE(6);
        out[2] = FL_W3_PULSE(5);
        out[3] = FL_W3_PULSE(4);
        out[4] = FL_W3_PULSE(3);
        out[5] = FL_W3_PULSE(2);
        out[6] = FL_W3_PULSE(1);
        out[7] = FL_W3_PULSE(0);
#undef FL_W3_PULSE
    }
}

// ============================================================================
// 16-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 16 lanes of Wave3Byte data into interleaved format
/// @param lane_waves Array of 16 Wave3Byte structures
/// @param output Output buffer (48 bytes = 16 * 3)
///
/// Fully-unrolled naive bit-by-bit transpose — benchmarked (#2524) as the
/// fastest on the ESP32-P4 (RV32), beating two-pass/SWAR. Each 16-lane sample
/// is 2 output bytes: low = lanes 0-7, high = lanes 8-15.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave3_transpose_16(const Wave3Byte lane_waves[16],
                        u8 output[16 * sizeof(Wave3Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        const u8 l0 = lane_waves[0].data[symbol_idx];
        const u8 l1 = lane_waves[1].data[symbol_idx];
        const u8 l2 = lane_waves[2].data[symbol_idx];
        const u8 l3 = lane_waves[3].data[symbol_idx];
        const u8 l4 = lane_waves[4].data[symbol_idx];
        const u8 l5 = lane_waves[5].data[symbol_idx];
        const u8 l6 = lane_waves[6].data[symbol_idx];
        const u8 l7 = lane_waves[7].data[symbol_idx];
        const u8 l8 = lane_waves[8].data[symbol_idx];
        const u8 l9 = lane_waves[9].data[symbol_idx];
        const u8 l10 = lane_waves[10].data[symbol_idx];
        const u8 l11 = lane_waves[11].data[symbol_idx];
        const u8 l12 = lane_waves[12].data[symbol_idx];
        const u8 l13 = lane_waves[13].data[symbol_idx];
        const u8 l14 = lane_waves[14].data[symbol_idx];
        const u8 l15 = lane_waves[15].data[symbol_idx];
        u8* out = &output[symbol_idx * 16];
#define FL_W3_LO(b) static_cast<u8>(((l7 >> (b)) & 1) << 7 | ((l6 >> (b)) & 1) << 6 | \
    ((l5 >> (b)) & 1) << 5 | ((l4 >> (b)) & 1) << 4 | ((l3 >> (b)) & 1) << 3 | \
    ((l2 >> (b)) & 1) << 2 | ((l1 >> (b)) & 1) << 1 | ((l0 >> (b)) & 1))
#define FL_W3_HI(b) static_cast<u8>(((l15 >> (b)) & 1) << 7 | ((l14 >> (b)) & 1) << 6 | \
    ((l13 >> (b)) & 1) << 5 | ((l12 >> (b)) & 1) << 4 | ((l11 >> (b)) & 1) << 3 | \
    ((l10 >> (b)) & 1) << 2 | ((l9 >> (b)) & 1) << 1 | ((l8 >> (b)) & 1))
        out[0] = FL_W3_LO(7);  out[1] = FL_W3_HI(7);
        out[2] = FL_W3_LO(6);  out[3] = FL_W3_HI(6);
        out[4] = FL_W3_LO(5);  out[5] = FL_W3_HI(5);
        out[6] = FL_W3_LO(4);  out[7] = FL_W3_HI(4);
        out[8] = FL_W3_LO(3);  out[9] = FL_W3_HI(3);
        out[10] = FL_W3_LO(2); out[11] = FL_W3_HI(2);
        out[12] = FL_W3_LO(1); out[13] = FL_W3_HI(1);
        out[14] = FL_W3_LO(0); out[15] = FL_W3_HI(0);
#undef FL_W3_LO
#undef FL_W3_HI
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

FL_OPTIMIZATION_LEVEL_O3_END
