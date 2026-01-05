/// @file wave8.hpp
/// @brief Inline implementation details for wave8 transposition
///
/// This header contains force-inlined implementations of wave8 transposition
/// functions for optimal performance in ISR/DMA contexts.

#pragma once

#include "fl/channels/wave8.h"
#include "fl/compiler_control.h"
#include "fl/force_inline.h"
#include "fl/isr.h"
#include "fl/stl/bit_cast.h"

namespace fl {

namespace detail {

// ============================================================================
// Lookup Tables
// ============================================================================

// 2-lane LUT: Spreads 4 bits into 2-lane interleaved positions (nibble → byte)
constexpr uint8_t kTranspose4_16_LUT[16] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11,
                                            0x14, 0x15, 0x40, 0x41, 0x44, 0x45,
                                            0x50, 0x51, 0x54, 0x55};

// 4-lane LUT: Spreads 2 bits into 4-lane interleaved positions (2-bit → byte)
// Maps [0b00, 0b01, 0b10, 0b11] → bit patterns at lane positions
// For lane N: bits placed at positions (bit*4 + N)
constexpr uint8_t kTranspose2_4_LUT[4] = {
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
void wave8_convert_byte_to_wave8byte(uint8_t byte_value,
                                      const Wave8BitExpansionLut &lut,
                                      Wave8Byte *output) {
    // ISR-optimized copy: Copy high nibble (4 bytes = 1 x uint32_t)
    const Wave8Bit *high_nibble_data = lut.lut[(byte_value >> 4) & 0xF];
    isr::memcpy32(fl::bit_cast_ptr<uint32_t>(&output->symbols[0]),
                  fl::bit_cast_ptr<const uint32_t>(high_nibble_data),
                  1); // 4 bytes = 1 x uint32_t

    // ISR-optimized copy: Copy low nibble (4 bytes = 1 x uint32_t)
    const Wave8Bit *low_nibble_data = lut.lut[byte_value & 0xF];
    isr::memcpy32(fl::bit_cast_ptr<uint32_t>(&output->symbols[4]),
                  fl::bit_cast_ptr<const uint32_t>(low_nibble_data),
                  1); // 4 bytes = 1 x uint32_t
}

// ============================================================================
// 2-Lane Transposition Helper Macro
// ============================================================================

#define FL_WAVE8_SPREAD_TO_16(lane_u8_0, lane_u8_1, out_16)                            \
    do {                                                                       \
        const uint8_t _a = (uint8_t)(lane_u8_0);                               \
        const uint8_t _b = (uint8_t)(lane_u8_1);                               \
        const uint16_t _even =                                                 \
            (uint16_t)((uint16_t)::fl::detail::kTranspose4_16_LUT[_b & 0x0Fu] |        \
                       ((uint16_t)::fl::detail::kTranspose4_16_LUT[_b >> 4] << 8));    \
        const uint16_t _odd =                                                  \
            (uint16_t)(((uint16_t)::fl::detail::kTranspose4_16_LUT[_a & 0x0Fu] |       \
                        ((uint16_t)::fl::detail::kTranspose4_16_LUT[_a >> 4] << 8))    \
                       << 1);                                                  \
        (out_16) |= (uint16_t)(_even | _odd);                                  \
    } while (0)

// ============================================================================
// 2-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 2 lanes of Wave8Byte data into interleaved format
/// @param lane_waves Array of 2 Wave8Byte structures
/// @param output Output buffer (16 bytes)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_2(const Wave8Byte lane_waves[2],
                       uint8_t output[2 * sizeof(Wave8Byte)]) {
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        uint16_t interleaved = 0;
        FL_WAVE8_SPREAD_TO_16(lane_waves[0].symbols[symbol_idx].data,
                              lane_waves[1].symbols[symbol_idx].data,
                              interleaved);

        output[symbol_idx * 2] = (uint8_t)(interleaved >> 8);
        output[symbol_idx * 2 + 1] = (uint8_t)(interleaved & 0xFF);
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
                       uint8_t output[4 * sizeof(Wave8Byte)]) {
    // Each symbol (Wave8Bit) has 8 pulses
    // With 4 lanes, we produce 4 bytes per symbol (2 pulses per byte × 4 lanes)
    // Output format: [L3_P7, L2_P7, L1_P7, L0_P7, L3_P6, L2_P6, L1_P6, L0_P6, ...]

    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        uint8_t lane_bytes[4];
        for (int lane = 0; lane < 4; lane++) {
            lane_bytes[lane] = lane_waves[lane].symbols[symbol_idx].data;
        }

        // Process 4 output bytes (2 pulses per byte)
        for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
            // Extract 2 pulses starting from bit position (7 - byte_idx*2)
            int pulse_bit_hi = 7 - (byte_idx * 2);      // High pulse bit
            int pulse_bit_lo = pulse_bit_hi - 1;        // Low pulse bit

            uint8_t output_byte = 0;

            // Interleave 4 lanes for these 2 pulses
            // Bit layout: [L3_hi, L2_hi, L1_hi, L0_hi, L3_lo, L2_lo, L1_lo, L0_lo]
            for (int lane = 0; lane < 4; lane++) {
                // Extract 2-bit pattern for this lane
                uint8_t pulse_hi = (lane_bytes[lane] >> pulse_bit_hi) & 1;
                uint8_t pulse_lo = (lane_bytes[lane] >> pulse_bit_lo) & 1;
                uint8_t two_pulses = (pulse_hi << 1) | pulse_lo;

                // Use LUT to spread 2 bits, then shift to lane position
                uint8_t spread = kTranspose2_4_LUT[two_pulses];
                output_byte |= (spread << lane);
            }

            output[symbol_idx * 4 + byte_idx] = output_byte;
        }
    }
}

// ============================================================================
// 8-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 8 lanes of Wave8Byte data into interleaved format
/// @param lane_waves Array of 8 Wave8Byte structures
/// @param output Output buffer (64 bytes)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_8(const Wave8Byte lane_waves[8],
                       uint8_t output[8 * sizeof(Wave8Byte)]) {
    // Each symbol (Wave8Bit) has 8 pulses
    // With 8 lanes, we produce 8 bytes per symbol (1 pulse per byte × 8 lanes)
    // Output format: [L7_P7, L6_P7, ..., L0_P7, L7_P6, L6_P6, ..., L0_P6, ...]

    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        uint8_t lane_bytes[8];
        for (int lane = 0; lane < 8; lane++) {
            lane_bytes[lane] = lane_waves[lane].symbols[symbol_idx].data;
        }

        // Process 8 output bytes (1 pulse per byte)
        for (int byte_idx = 0; byte_idx < 8; byte_idx++) {
            // Extract 1 pulse from bit position (7 - byte_idx)
            int pulse_bit = 7 - byte_idx;

            uint8_t output_byte = 0;

            // Interleave 8 lanes for this pulse
            // Bit layout: [L7, L6, L5, L4, L3, L2, L1, L0]
            for (int lane = 0; lane < 8; lane++) {
                // Extract 1-bit pattern for this lane
                uint8_t pulse = (lane_bytes[lane] >> pulse_bit) & 1;

                // Place bit at lane position (lane 0 = LSB, lane 7 = MSB)
                output_byte |= (pulse << lane);
            }

            output[symbol_idx * 8 + byte_idx] = output_byte;
        }
    }
}

// ============================================================================
// 16-Lane Transposition (Force Inline)
// ============================================================================

/// @brief Transpose 16 lanes of Wave8Byte data into interleaved format
/// @param lane_waves Array of 16 Wave8Byte structures
/// @param output Output buffer (128 bytes)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8_transpose_16(const Wave8Byte lane_waves[16],
                        uint8_t output[16 * sizeof(Wave8Byte)]) {
    // Each symbol (Wave8Bit) has 8 pulses
    // With 16 lanes, we produce 16 bytes per symbol (1 pulse across 16 lanes = 2 bytes)
    // Output format: 16 lanes interleaved, 8 bits per lane, 2 bytes per pulse
    // [L15_P7, ..., L0_P7, L15_P6, ..., L0_P6, ...]

    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        uint8_t lane_bytes[16];
        for (int lane = 0; lane < 16; lane++) {
            lane_bytes[lane] = lane_waves[lane].symbols[symbol_idx].data;
        }

        // Process 16 output bytes (8 pulses × 2 bytes per pulse)
        for (int byte_idx = 0; byte_idx < 16; byte_idx++) {
            // Each pair of bytes represents one pulse across all 16 lanes
            int pulse_bit = 7 - (byte_idx / 2);

            uint8_t output_byte = 0;

            // Determine which 8 lanes to process (high byte = lanes 8-15, low byte = lanes 0-7)
            if (byte_idx % 2 == 0) {
                // High byte: lanes 8-15
                for (int lane = 8; lane < 16; lane++) {
                    uint8_t pulse = (lane_bytes[lane] >> pulse_bit) & 1;
                    output_byte |= (pulse << (lane - 8));
                }
            } else {
                // Low byte: lanes 0-7
                for (int lane = 0; lane < 8; lane++) {
                    uint8_t pulse = (lane_bytes[lane] >> pulse_bit) & 1;
                    output_byte |= (pulse << lane);
                }
            }

            output[symbol_idx * 16 + byte_idx] = output_byte;
        }
    }
}

} // namespace detail

// ============================================================================
// Public wave8() Function (Force Inline)
// ============================================================================

/// @brief Convert byte to 8 Wave8Bit structures using nibble LUT
/// @note Inline implementation for ISR performance (always_inline requires visible body)
FASTLED_FORCE_INLINE FL_IRAM FL_OPTIMIZE_FUNCTION
void wave8(uint8_t lane,
           const Wave8BitExpansionLut &lut,
           uint8_t (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]) {
    // Convert single lane byte to wave pulse symbols (8 bytes packed)
    // Use properly aligned local variable to avoid alignment issues
    Wave8Byte waveformSymbol;
    detail::wave8_convert_byte_to_wave8byte(lane, lut, &waveformSymbol);

    // ISR-optimized 32-bit copy: Copy 8 bytes as 2 x uint32_t words
    // Wave8Byte is 8-byte aligned (FL_ALIGNAS(8)), guaranteeing 4-byte alignment
    isr::memcpy32(fl::bit_cast_ptr<uint32_t>(output),
                  fl::bit_cast_ptr<const uint32_t>(&waveformSymbol.symbols[0].data),
                  2); // 8 bytes = 2 x uint32_t
}

} // namespace fl
