/// @file wave8.hpp
/// @brief Inline implementation details for wave8 transposition
///
/// This header contains force-inlined implementations of wave8 transposition
/// functions for optimal performance in ISR/DMA contexts.

#pragma once

#include "fl/channels/wave8.h"
#include "fl/channels/detail/bit_spread_lut.hpp"
#include "fl/stl/compiler_control.h"
#include "fl/stl/isr/memcpy.h"
#include "fl/stl/bit_cast.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

namespace detail {

// ============================================================================
// Lookup Tables
// ============================================================================

// 2-lane LUT: Spreads 4 bits into 2-lane interleaved positions (nibble → byte)
constexpr u8 kTranspose4_16_LUT[16] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11,
                                            0x14, 0x15, 0x40, 0x41, 0x44, 0x45,
                                            0x50, 0x51, 0x54, 0x55};

// 4-lane LUT: Spreads 2 bits into 4-lane interleaved positions (2-bit → byte)
// Maps [0b00, 0b01, 0b10, 0b11] → bit patterns at lane positions
// For lane N: bits placed at positions (bit*4 + N)
constexpr u8 kTranspose2_4_LUT[4] = {
    0x00,  // 0b00 → no bits set
    0x01,  // 0b01 → bit at position 0 (pulse 1)
    0x10,  // 0b10 → bit at position 4 (pulse 0)
    0x11   // 0b11 → bits at positions 0 and 4 (both pulses)
};

// ============================================================================
// Byte to Wave8Byte Conversion (Force Inline Helper)
// ============================================================================

/// @brief Helper: Convert byte to Wave8Byte using nibble LUT (internal use only)
/// @note Inline implementation for ISR performance
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_convert_byte_to_wave8byte(u8 byte_value,
                                      const Wave8BitExpansionLut &lut,
                                      Wave8Byte *output) {
    // ISR-optimized copy: Copy high nibble (4 bytes = 1 x uint32_t)
    const Wave8Bit *high_nibble_data = lut.lut[(byte_value >> 4) & 0xF];
    isr::memcpy_32(fl::bit_cast_ptr<u32>(&output->symbols[0]),
                  fl::bit_cast_ptr<const u32>(high_nibble_data),
                  1); // 4 bytes = 1 x uint32_t

    // ISR-optimized copy: Copy low nibble (4 bytes = 1 x uint32_t)
    const Wave8Bit *low_nibble_data = lut.lut[byte_value & 0xF];
    isr::memcpy_32(fl::bit_cast_ptr<u32>(&output->symbols[4]),
                  fl::bit_cast_ptr<const u32>(low_nibble_data),
                  1); // 4 bytes = 1 x uint32_t
}

/// @brief Byte-indexed expansion (#2526): one indexed 8-byte copy.
///
/// Single lookup (no >>4/mask, no second index) into the 256×8 byte LUT;
/// bit-identical to wave8_convert_byte_to_wave8byte(). ~half the index/issue
/// work for the same memory traffic — the win on the in-order RV32 core.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_expand_byte(u8 byte_value,
                       const Wave8ByteExpansionLut &lut,
                       Wave8Byte *output) {
    isr::memcpy_32(fl::bit_cast_ptr<u32>(output),
                  fl::bit_cast_ptr<const u32>(&lut.lut[byte_value]),
                  2); // 8 bytes = 2 x uint32_t
}

// ============================================================================
// 2-Lane Transposition Helper Macro
// ============================================================================

#define FL_WAVE8_SPREAD_TO_16(lane_u8_0, lane_u8_1, out_16)                            \
    do {                                                                       \
        const u8 _a = (u8)(lane_u8_0);                               \
        const u8 _b = (u8)(lane_u8_1);                               \
        const u16 _even =                                                 \
            (u16)((u16)::fl::detail::kTranspose4_16_LUT[_b & 0x0Fu] |        \
                       ((u16)::fl::detail::kTranspose4_16_LUT[_b >> 4] << 8));    \
        const u16 _odd =                                                  \
            (u16)(((u16)::fl::detail::kTranspose4_16_LUT[_a & 0x0Fu] |       \
                        ((u16)::fl::detail::kTranspose4_16_LUT[_a >> 4] << 8))    \
                       << 1);                                                  \
        (out_16) |= (u16)(_even | _odd);                                  \
    } while (0)

// ============================================================================
// 2-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 2 lanes of Wave8Byte data into interleaved format
/// @param lane_waves Array of 2 Wave8Byte structures (lane[0]=even bits, lane[1]=odd bits)
/// @param output Output buffer (16 bytes)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_2(const Wave8Byte lane_waves[2],
                       u8 output[2 * sizeof(Wave8Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u16 interleaved = 0;
        // NOTE: FL_WAVE8_SPREAD_TO_16 macro treats first param as ODD bits, second as EVEN bits
        // This matches wave8Untranspose_2 expectations: lane[0]→odd, lane[1]→even
        FL_WAVE8_SPREAD_TO_16(lane_waves[0].symbols[symbol_idx].data,
                              lane_waves[1].symbols[symbol_idx].data,
                              interleaved);

        output[symbol_idx * 2] = (u8)(interleaved >> 8);         // High byte first (MSB pulses 4-7)
        output[symbol_idx * 2 + 1] = (u8)(interleaved & 0xFF);  // Low byte second (LSB pulses 0-3)
    }
}

// ============================================================================
// 4-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 4 lanes of Wave8Byte data into interleaved format
/// @param lane_waves Array of 4 Wave8Byte structures
/// @param output Output buffer (32 bytes)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_4(const Wave8Byte lane_waves[4],
                       u8 output[4 * sizeof(Wave8Byte)]) {
    // Each symbol (Wave8Bit) has 8 pulses
    // With 4 lanes, we produce 4 bytes per symbol (2 pulses per byte × 4 lanes)
    // Output format: [L3_P7, L2_P7, L1_P7, L0_P7, L3_P6, L2_P6, L1_P6, L0_P6, ...]
    //
    // OPTIMIZED VERSION: Fully unrolled direct extraction (4.0x speedup vs baseline)
    // Based on successful 16-lane pattern that achieved 8x speedup
    // Eliminates triple-nested loops by explicitly extracting and packing bits

    // Process each symbol (8 iterations)
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        // Pre-load all 4 lane bytes into registers
        u8 l0 = lane_waves[0].symbols[symbol_idx].data;
        u8 l1 = lane_waves[1].symbols[symbol_idx].data;
        u8 l2 = lane_waves[2].symbols[symbol_idx].data;
        u8 l3 = lane_waves[3].symbols[symbol_idx].data;

        // Explicitly construct all 4 output bytes
        // Each output byte contains 2 pulses from all 4 lanes
        // Bit layout: [L3_hi, L2_hi, L1_hi, L0_hi, L3_lo, L2_lo, L1_lo, L0_lo]

        // Byte 0: pulses 7 (hi) and 6 (lo)
        output[symbol_idx * 4 + 0] =
            ((l3 >> 7) & 1) << 7 |
            ((l2 >> 7) & 1) << 6 |
            ((l1 >> 7) & 1) << 5 |
            ((l0 >> 7) & 1) << 4 |
            ((l3 >> 6) & 1) << 3 |
            ((l2 >> 6) & 1) << 2 |
            ((l1 >> 6) & 1) << 1 |
            ((l0 >> 6) & 1);

        // Byte 1: pulses 5 (hi) and 4 (lo)
        output[symbol_idx * 4 + 1] =
            ((l3 >> 5) & 1) << 7 |
            ((l2 >> 5) & 1) << 6 |
            ((l1 >> 5) & 1) << 5 |
            ((l0 >> 5) & 1) << 4 |
            ((l3 >> 4) & 1) << 3 |
            ((l2 >> 4) & 1) << 2 |
            ((l1 >> 4) & 1) << 1 |
            ((l0 >> 4) & 1);

        // Byte 2: pulses 3 (hi) and 2 (lo)
        output[symbol_idx * 4 + 2] =
            ((l3 >> 3) & 1) << 7 |
            ((l2 >> 3) & 1) << 6 |
            ((l1 >> 3) & 1) << 5 |
            ((l0 >> 3) & 1) << 4 |
            ((l3 >> 2) & 1) << 3 |
            ((l2 >> 2) & 1) << 2 |
            ((l1 >> 2) & 1) << 1 |
            ((l0 >> 2) & 1);

        // Byte 3: pulses 1 (hi) and 0 (lo)
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

/// @brief Transpose 8 lanes of Wave8Byte data into interleaved format
/// @param lane_waves Array of 8 Wave8Byte structures
/// @param output Output buffer (64 bytes)
///
/// Spread-LUT transpose (#2533): ~1.90× faster than the unrolled naive on the
/// ESP32-P4 (RV32) — 3356→1764 us/frame, bit-exact. See bit_spread_lut.hpp.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_8(const Wave8Byte lane_waves[8],
                       u8 output[8 * sizeof(Wave8Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 l[8];
        for (int lane = 0; lane < 8; lane++) {
            l[lane] = lane_waves[lane].symbols[symbol_idx].data;
        }
        spread_transpose8_symbol(l, output + symbol_idx * 8);
    }
}

// ============================================================================
// 16-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 16 lanes of Wave8Byte data into interleaved format
/// @param lane_waves Array of 16 Wave8Byte structures
/// @param output Output buffer (128 bytes)
///
/// Spread-LUT transpose (#2533): ~1.98× faster than the unrolled naive on the
/// ESP32-P4 (RV32) — 6649→3353 us/frame, bit-exact. Two-pass (two 8x8 via u32),
/// u64 SWAR, and Hacker's-Delight all lost to this on the in-order core; the
/// winning shape is independent table-lookup + shift + OR-reduce (native u32,
/// no dependency chain). See bit_spread_lut.hpp. Each 16-lane sample is 2 output
/// bytes: low = lanes 0-7, high = lanes 8-15.
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_16(const Wave8Byte lane_waves[16],
                        u8 output[16 * sizeof(Wave8Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 l[16];
        for (int lane = 0; lane < 16; lane++) {
            l[lane] = lane_waves[lane].symbols[symbol_idx].data;
        }
        spread_transpose16_symbol(l, output + symbol_idx * 16);
    }
}

/// @brief Pipe2: transpose 16-lane × 2-byte-positions in one fused call.
///        Result is bit-identical to two sequential `wave8_transpose_16` calls;
///        the win comes from interleaving the two independent OR-trees inside
///        the symbol loop so the in-order RV32 P4 can fill load-use stall
///        cycles from position A with ALU ops from position B (and vice versa).
///        Measured +26% / frame vs sequential calls on P4 v1.3 (#2548).
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_16x2_pipe2(const Wave8Byte lane_waves_a[16],
                                const Wave8Byte lane_waves_b[16],
                                u8 output_a[16 * sizeof(Wave8Byte)],
                                u8 output_b[16 * sizeof(Wave8Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 la[16];
        u8 lb[16];
        for (int lane = 0; lane < 16; lane++) {
            la[lane] = lane_waves_a[lane].symbols[symbol_idx].data;
            lb[lane] = lane_waves_b[lane].symbols[symbol_idx].data;
        }
        spread_transpose16_symbol(la, output_a + symbol_idx * 16);
        spread_transpose16_symbol(lb, output_b + symbol_idx * 16);
    }
}

} // namespace detail

// ============================================================================
// Public wave8() Function (Force Inline)
// ============================================================================

/// @brief Convert byte to 8 Wave8Bit structures using nibble LUT
/// @note Inline implementation for ISR performance (always_inline requires visible body)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8(u8 lane,
           const Wave8BitExpansionLut &lut,
           u8 (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]) {
    // Convert single lane byte to wave pulse symbols (8 bytes packed)
    // Use properly aligned local variable to avoid alignment issues
    Wave8Byte waveformSymbol;
    detail::wave8_convert_byte_to_wave8byte(lane, lut, &waveformSymbol);

    // ISR-optimized 32-bit copy: Copy 8 bytes as 2 x uint32_t words
    // Wave8Byte is 8-byte aligned (FL_ALIGNAS(8)), guaranteeing 4-byte alignment
    isr::memcpy_32(fl::bit_cast_ptr<u32>(output),
                  fl::bit_cast_ptr<const u32>(&waveformSymbol.symbols[0].data),
                  2); // 8 bytes = 2 x uint32_t
}

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
