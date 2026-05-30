/// @file wave8.cpp
/// @brief Waveform generation and transposition implementation
///
/// This file contains:
/// - Non-ISR LUT builder (buildWave8ExpansionLUT)
/// - Public transposition functions (wave8Transpose_2, wave8Transpose_4)
///
/// Note: Inline helper functions are in detail/wave8.hpp

#include "fl/channels/wave8.h"
#include "fl/channels/detail/wave8.hpp"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/isr/memcpy.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

namespace fl {

// ============================================================================
// Public Transposition Functions
// ============================================================================

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_2(const u8 (&FL_RESTRICT_PARAM lanes)[2],
                      const Wave8BitExpansionLut &lut,
                      u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]) {
    // Allocate waveform buffers on stack (16 Wave8Bit total: 8 packed bytes per lane × 2 lanes)
    // Each Wave8Byte is 8 bytes (8 Wave8Bit × 1 byte each)
    // Layout: [Lane0_bit7, Lane0_bit6, ..., Lane0_bit0, Lane1_bit7, Lane1_bit6, ..., Lane1_bit0]
    Wave8Byte laneWaveformSymbols[2];

    // Convert each lane byte to wave pulse symbols (8 packed bytes each)
    detail::wave8_convert_byte_to_wave8byte(lanes[0], lut, &laneWaveformSymbols[0]);
    detail::wave8_convert_byte_to_wave8byte(lanes[1], lut, &laneWaveformSymbols[1]);

    // Transpose waveforms to DMA format (interleave 8 packed bytes to 16 bytes)
    detail::wave8_transpose_2(laneWaveformSymbols, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_4(const u8 (&FL_RESTRICT_PARAM lanes)[4],
                      const Wave8BitExpansionLut &lut,
                      u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]) {
    // Allocate waveform buffers on stack (32 Wave8Bit total: 8 packed bytes per lane × 4 lanes)
    Wave8Byte laneWaveformSymbols[4];

    // Convert each lane byte to wave pulse symbols (8 packed bytes each)
    for (int lane = 0; lane < 4; lane++) {
        detail::wave8_convert_byte_to_wave8byte(lanes[lane], lut, &laneWaveformSymbols[lane]);
    }

    // Transpose waveforms to DMA format (interleave 32 packed bytes to 32 bytes)
    detail::wave8_transpose_4(laneWaveformSymbols, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_8(const u8 (&FL_RESTRICT_PARAM lanes)[8],
                      const Wave8BitExpansionLut &lut,
                      u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]) {
    // Allocate waveform buffers on stack (64 Wave8Bit total: 8 packed bytes per lane × 8 lanes)
    Wave8Byte laneWaveformSymbols[8];

    // Convert each lane byte to wave pulse symbols (8 packed bytes each)
    for (int lane = 0; lane < 8; lane++) {
        detail::wave8_convert_byte_to_wave8byte(lanes[lane], lut, &laneWaveformSymbols[lane]);
    }

    // Transpose waveforms to DMA format (interleave 64 packed bytes to 64 bytes)
    detail::wave8_transpose_8(laneWaveformSymbols, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_16(const u8 (&FL_RESTRICT_PARAM lanes)[16],
                       const Wave8BitExpansionLut &lut,
                       u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]) {
    // Allocate waveform buffers on stack (128 Wave8Bit total: 8 packed bytes per lane × 16 lanes)
    Wave8Byte laneWaveformSymbols[16];

    // Convert each lane byte to wave pulse symbols (8 packed bytes each)
    for (int lane = 0; lane < 16; lane++) {
        detail::wave8_convert_byte_to_wave8byte(lanes[lane], lut, &laneWaveformSymbols[lane]);
    }

    // Transpose waveforms to DMA format (interleave 128 packed bytes to 128 bytes)
    detail::wave8_transpose_16(laneWaveformSymbols, output);
}

// ============================================================================
// Byte-LUT overloads (#2526): cheaper expansion, same DMA output.
// ============================================================================

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_2(const u8 (&FL_RESTRICT_PARAM lanes)[2],
                      const Wave8ByteExpansionLut &lut,
                      u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]) {
    Wave8Byte laneWaveformSymbols[2];
    detail::wave8_expand_byte(lanes[0], lut, &laneWaveformSymbols[0]);
    detail::wave8_expand_byte(lanes[1], lut, &laneWaveformSymbols[1]);
    detail::wave8_transpose_2(laneWaveformSymbols, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_4(const u8 (&FL_RESTRICT_PARAM lanes)[4],
                      const Wave8ByteExpansionLut &lut,
                      u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]) {
    Wave8Byte laneWaveformSymbols[4];
    for (int lane = 0; lane < 4; lane++) {
        detail::wave8_expand_byte(lanes[lane], lut, &laneWaveformSymbols[lane]);
    }
    detail::wave8_transpose_4(laneWaveformSymbols, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_8(const u8 (&FL_RESTRICT_PARAM lanes)[8],
                      const Wave8ByteExpansionLut &lut,
                      u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]) {
    Wave8Byte laneWaveformSymbols[8];
    for (int lane = 0; lane < 8; lane++) {
        detail::wave8_expand_byte(lanes[lane], lut, &laneWaveformSymbols[lane]);
    }
    detail::wave8_transpose_8(laneWaveformSymbols, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_16(const u8 (&FL_RESTRICT_PARAM lanes)[16],
                       const Wave8ByteExpansionLut &lut,
                       u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]) {
    Wave8Byte laneWaveformSymbols[16];
    for (int lane = 0; lane < 16; lane++) {
        detail::wave8_expand_byte(lanes[lane], lut, &laneWaveformSymbols[lane]);
    }
    detail::wave8_transpose_16(laneWaveformSymbols, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_16x2_pipe2(const u8 (&FL_RESTRICT_PARAM lanes_a)[16],
                               const u8 (&FL_RESTRICT_PARAM lanes_b)[16],
                               const Wave8ByteExpansionLut &lut,
                               u8 (&FL_RESTRICT_PARAM output_a)[16 * sizeof(Wave8Byte)],
                               u8 (&FL_RESTRICT_PARAM output_b)[16 * sizeof(Wave8Byte)]) {
    // Expand both positions independently — compiler can interleave the two
    // loops freely because they share no data.
    Wave8Byte laneWaveformsA[16];
    Wave8Byte laneWaveformsB[16];
    for (int lane = 0; lane < 16; lane++) {
        detail::wave8_expand_byte(lanes_a[lane], lut, &laneWaveformsA[lane]);
    }
    for (int lane = 0; lane < 16; lane++) {
        detail::wave8_expand_byte(lanes_b[lane], lut, &laneWaveformsB[lane]);
    }
    // Symbol-major loop with both transposes inlined back-to-back: the two
    // OR-trees share no dependencies, so the compiler can interleave them and
    // fill the in-order pipeline bubbles. See #2548.
    detail::wave8_transpose_16x2_pipe2(laneWaveformsA, laneWaveformsB,
                                       output_a, output_b);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_16_bf1(const u8 (&FL_RESTRICT_PARAM lanes)[16],
                           const Wave8ByteExpansionLut &lut,
                           u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]) {
    // Extract W0/W1 chipset constants from the lut.
    // byte_lut[0x00].symbols[0] = waveform for input bit 7 == 0 = W0
    // byte_lut[0xFF].symbols[0] = waveform for input bit 7 == 1 = W1
    const u8 W0 = lut.lut[0x00].symbols[0].data;
    const u8 W1 = lut.lut[0xFF].symbols[0].data;
    detail::wave8_transpose_16_bf1(lanes, W0, W1, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_8_bf1(const u8 (&FL_RESTRICT_PARAM lanes)[8],
                          const Wave8ByteExpansionLut &lut,
                          u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]) {
    const u8 W0 = lut.lut[0x00].symbols[0].data;
    const u8 W1 = lut.lut[0xFF].symbols[0].data;
    detail::wave8_transpose_8_bf1(lanes, W0, W1, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_4_bf1(const u8 (&FL_RESTRICT_PARAM lanes)[4],
                          const Wave8ByteExpansionLut &lut,
                          u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]) {
    const u8 W0 = lut.lut[0x00].symbols[0].data;
    const u8 W1 = lut.lut[0xFF].symbols[0].data;
    detail::wave8_transpose_4_bf1(lanes, W0, W1, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_2_bf1(const u8 (&FL_RESTRICT_PARAM lanes)[2],
                          const Wave8ByteExpansionLut &lut,
                          u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]) {
    const u8 W0 = lut.lut[0x00].symbols[0].data;
    const u8 W1 = lut.lut[0xFF].symbols[0].data;
    detail::wave8_transpose_2_bf1(lanes, W0, W1, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_16x4_bf1_pipe4(const u8 (&FL_RESTRICT_PARAM lanes_a)[16],
                                   const u8 (&FL_RESTRICT_PARAM lanes_b)[16],
                                   const u8 (&FL_RESTRICT_PARAM lanes_c)[16],
                                   const u8 (&FL_RESTRICT_PARAM lanes_d)[16],
                                   const Wave8ByteExpansionLut &lut,
                                   u8 (&FL_RESTRICT_PARAM output_a)[16 * sizeof(Wave8Byte)],
                                   u8 (&FL_RESTRICT_PARAM output_b)[16 * sizeof(Wave8Byte)],
                                   u8 (&FL_RESTRICT_PARAM output_c)[16 * sizeof(Wave8Byte)],
                                   u8 (&FL_RESTRICT_PARAM output_d)[16 * sizeof(Wave8Byte)]) {
    const u8 W0 = lut.lut[0x00].symbols[0].data;
    const u8 W1 = lut.lut[0xFF].symbols[0].data;
    detail::wave8_transpose_16x4_bf1_pipe4(lanes_a, lanes_b, lanes_c, lanes_d,
                                            W0, W1,
                                            output_a, output_b, output_c, output_d);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_16x4_pipe4(const u8 (&FL_RESTRICT_PARAM lanes_a)[16],
                               const u8 (&FL_RESTRICT_PARAM lanes_b)[16],
                               const u8 (&FL_RESTRICT_PARAM lanes_c)[16],
                               const u8 (&FL_RESTRICT_PARAM lanes_d)[16],
                               const Wave8ByteExpansionLut &lut,
                               u8 (&FL_RESTRICT_PARAM output_a)[16 * sizeof(Wave8Byte)],
                               u8 (&FL_RESTRICT_PARAM output_b)[16 * sizeof(Wave8Byte)],
                               u8 (&FL_RESTRICT_PARAM output_c)[16 * sizeof(Wave8Byte)],
                               u8 (&FL_RESTRICT_PARAM output_d)[16 * sizeof(Wave8Byte)]) {
    Wave8Byte laneWaveformsA[16];
    Wave8Byte laneWaveformsB[16];
    Wave8Byte laneWaveformsC[16];
    Wave8Byte laneWaveformsD[16];
    for (int lane = 0; lane < 16; lane++) {
        detail::wave8_expand_byte(lanes_a[lane], lut, &laneWaveformsA[lane]);
    }
    for (int lane = 0; lane < 16; lane++) {
        detail::wave8_expand_byte(lanes_b[lane], lut, &laneWaveformsB[lane]);
    }
    for (int lane = 0; lane < 16; lane++) {
        detail::wave8_expand_byte(lanes_c[lane], lut, &laneWaveformsC[lane]);
    }
    for (int lane = 0; lane < 16; lane++) {
        detail::wave8_expand_byte(lanes_d[lane], lut, &laneWaveformsD[lane]);
    }
    detail::wave8_transpose_16x4_pipe4(laneWaveformsA, laneWaveformsB,
                                       laneWaveformsC, laneWaveformsD,
                                       output_a, output_b, output_c, output_d);
}

// ============================================================================
// LUT Builder from Timing Data
// Note: This is not designed to be called from ISR handlers.
// ============================================================================
FL_OPTIMIZE_FUNCTION
Wave8BitExpansionLut buildWave8ExpansionLUT(const ChipsetTiming &timing) {
    Wave8BitExpansionLut lut;

    // Step 1: Calculate absolute times from ChipsetTiming format
    // ChipsetTiming.T1 = T0H (high time for bit 0)
    // ChipsetTiming.T2 = T1H - T0H (ADDITIONAL high time for bit 1)
    // ChipsetTiming.T3 = T0L (low tail duration)
    const u32 t0h = timing.T1;                     // T0H: bit 0 goes LOW here
    const u32 t1h = timing.T1 + timing.T2;          // T1H: bit 1 goes LOW here
    const u32 period = timing.T1 + timing.T2 + timing.T3; // Total period

    // Step 2: Normalize absolute times
    const float t0h_norm = static_cast<float>(t0h) / period;
    const float t1h_norm = static_cast<float>(t1h) / period;

    // Step 3: Convert to pulse counts (fixed 8 pulses per bit)
    // pulses_bit0: number of HIGH pulses for bit 0 (before it goes LOW at t0h)
    // pulses_bit1: number of HIGH pulses for bit 1 (before it goes LOW at t1h)
    u32 pulses_bit0 =
        static_cast<u32>(t0h_norm * 8.0f + 0.5f); // round
    u32 pulses_bit1 =
        static_cast<u32>(t1h_norm * 8.0f + 0.5f); // round

    // Clamp to valid range [0, 8]
    if (pulses_bit0 > 8)
        pulses_bit0 = 8;
    if (pulses_bit1 > 8)
        pulses_bit1 = 8;

    // Step 4: Generate bit0 and bit1 waveforms (1 byte each, packed format)
    // Each bit represents one pulse (MSB = first pulse)
    u8 bit0_waveform = 0;
    u8 bit1_waveform = 0;

    // Bit 0: Set MSB bits for HIGH pulses
    for (u32 i = 0; i < pulses_bit0; i++) {
        bit0_waveform |= (0x80 >> i);  // Set bit from MSB
    }

    // Bit 1: Set MSB bits for HIGH pulses
    for (u32 i = 0; i < pulses_bit1; i++) {
        bit1_waveform |= (0x80 >> i);  // Set bit from MSB
    }

    // Step 5: Build LUT for all 16 nibbles
    for (u8 nibble = 0; nibble < 16; nibble++) {
        // For each nibble, generate 4 Wave8Bit (one per bit, MSB first)
        for (int bit_pos = 3; bit_pos >= 0; bit_pos--) {
            // Extract bit (MSB first: bit 3, 2, 1, 0)
            const bool bit_set = (nibble >> bit_pos) & 1;
            const u8 waveform = bit_set ? bit1_waveform : bit0_waveform;

            // Store packed waveform to LUT entry
            lut.lut[nibble][3 - bit_pos].data = waveform;
        }
    }

    return lut;
}

// Byte-indexed expansion LUT (#2526). Entry b = high-nibble expansion in
// symbols[0..3] + low-nibble expansion in symbols[4..7], matching
// wave8_convert_byte_to_wave8byte() exactly (so the byte path is bit-identical).
Wave8ByteExpansionLut buildWave8ByteExpansionLUT(const Wave8BitExpansionLut &nibble) {
    Wave8ByteExpansionLut out;
    for (int b = 0; b < 256; ++b) {
        const Wave8Bit *hi = nibble.lut[(b >> 4) & 0xF];
        const Wave8Bit *lo = nibble.lut[b & 0xF];
        for (int i = 0; i < 4; ++i) {
            out.lut[b].symbols[i] = hi[i];
            out.lut[b].symbols[i + 4] = lo[i];
        }
    }
    return out;
}

// ============================================================================
// Untranspose Functions (Testing Only - Not Optimized)
// ============================================================================

FL_OPTIMIZE_FUNCTION
void wave8Untranspose_2(const u8 (&FL_RESTRICT_PARAM transposed)[2 * sizeof(Wave8Byte)],
                        u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]) {
    // Reverse the 2-lane transposition
    // Input: 16 bytes of interleaved data (2 bytes per symbol, 8 symbols)
    // Output: 2 Wave8Byte structures (16 bytes total, de-interleaved)

    Wave8Byte lane_waves[2];

    // Process each of the 8 symbols
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        // Read the 2 interleaved bytes for this symbol
        u16 interleaved = ((u16)transposed[symbol_idx * 2] << 8) |
                               transposed[symbol_idx * 2 + 1];

        // De-interleave bits back to lanes
        u8 lane0_bits = 0;
        u8 lane1_bits = 0;

        // Extract bits: interleaved format has alternating bits [L0, L1, L0, L1, ...]
        // Bits are ordered: [L1_b7, L0_b7, L1_b6, L0_b6, L1_b5, L0_b5, L1_b4, L0_b4, ...]
        for (int bit = 0; bit < 8; bit++) {
            // Lane 0 bits are at odd positions (shifted left by 1)
            if (interleaved & (1 << (bit * 2 + 1))) {
                lane0_bits |= (1 << bit);
            }
            // Lane 1 bits are at even positions
            if (interleaved & (1 << (bit * 2))) {
                lane1_bits |= (1 << bit);
            }
        }

        lane_waves[0].symbols[symbol_idx].data = lane0_bits;
        lane_waves[1].symbols[symbol_idx].data = lane1_bits;
    }

    // Copy de-interleaved data to output
    fl::isr::memcpy(output, &lane_waves[0], sizeof(Wave8Byte));
    fl::isr::memcpy(output + sizeof(Wave8Byte), &lane_waves[1], sizeof(Wave8Byte));
}

FL_OPTIMIZE_FUNCTION
void wave8Untranspose_4(const u8 (&FL_RESTRICT_PARAM transposed)[4 * sizeof(Wave8Byte)],
                        u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]) {
    // Reverse the 4-lane transposition
    // Input: 32 bytes of interleaved data (4 bytes per symbol, 8 symbols)
    // Output: 4 Wave8Byte structures (32 bytes total, de-interleaved)

    Wave8Byte lane_waves[4];

    // Process each of the 8 symbols
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 lane_bytes[4] = {0, 0, 0, 0};

        // Process 4 input bytes (2 pulses per byte)
        for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
            u8 input_byte = transposed[symbol_idx * 4 + byte_idx];

            // Calculate which pulse bits these correspond to
            int pulse_bit_hi = 7 - (byte_idx * 2);
            int pulse_bit_lo = pulse_bit_hi - 1;

            // De-interleave 4 lanes from this byte
            // Bit layout: [L3_hi, L2_hi, L1_hi, L0_hi, L3_lo, L2_lo, L1_lo, L0_lo]
            for (int lane = 0; lane < 4; lane++) {
                // Extract bits for this lane
                u8 pulse_hi = (input_byte >> (4 + lane)) & 1;
                u8 pulse_lo = (input_byte >> lane) & 1;

                // Reconstruct lane byte
                lane_bytes[lane] |= (pulse_hi << pulse_bit_hi);
                lane_bytes[lane] |= (pulse_lo << pulse_bit_lo);
            }
        }

        // Store de-interleaved bytes
        for (int lane = 0; lane < 4; lane++) {
            lane_waves[lane].symbols[symbol_idx].data = lane_bytes[lane];
        }
    }

    // Copy de-interleaved data to output
    for (int lane = 0; lane < 4; lane++) {
        fl::isr::memcpy(output + lane * sizeof(Wave8Byte), &lane_waves[lane], sizeof(Wave8Byte));
    }
}

FL_OPTIMIZE_FUNCTION
void wave8Untranspose_8(const u8 (&FL_RESTRICT_PARAM transposed)[8 * sizeof(Wave8Byte)],
                        u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave8Byte)]) {
    // Reverse the 8-lane transposition
    // Input: 64 bytes of interleaved data (8 bytes per symbol, 8 symbols)
    // Output: 8 Wave8Byte structures (64 bytes total, de-interleaved)

    Wave8Byte lane_waves[8];

    // Process each of the 8 symbols
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 lane_bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        // Process 8 input bytes (1 pulse per byte)
        // After transpose+reversal: byte 0 = bit 7 (first pulse, MSB),
        // byte 7 = bit 0 (last pulse, LSB)
        for (int byte_idx = 0; byte_idx < 8; byte_idx++) {
            u8 input_byte = transposed[symbol_idx * 8 + byte_idx];

            // Calculate which pulse bit this corresponds to
            // byte 0 = bit 7, byte 7 = bit 0 (reversed order)
            int pulse_bit = 7 - byte_idx;

            // De-interleave 8 lanes from this byte
            // Bit layout: [L7, L6, L5, L4, L3, L2, L1, L0]
            for (int lane = 0; lane < 8; lane++) {
                // Extract bit for this lane (lane 0 = LSB, lane 7 = MSB)
                u8 pulse = (input_byte >> lane) & 1;

                // Reconstruct lane byte
                lane_bytes[lane] |= (pulse << pulse_bit);
            }
        }

        // Store de-interleaved bytes
        for (int lane = 0; lane < 8; lane++) {
            lane_waves[lane].symbols[symbol_idx].data = lane_bytes[lane];
        }
    }

    // Copy de-interleaved data to output
    for (int lane = 0; lane < 8; lane++) {
        fl::isr::memcpy(output + lane * sizeof(Wave8Byte), &lane_waves[lane], sizeof(Wave8Byte));
    }
}

FL_OPTIMIZE_FUNCTION
void wave8Untranspose_16(const u8 (&FL_RESTRICT_PARAM transposed)[16 * sizeof(Wave8Byte)],
                         u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave8Byte)]) {
    // Reverse the 16-lane transposition
    // Input: 128 bytes of interleaved data (16 bytes per symbol, 8 symbols)
    // Output: 16 Wave8Byte structures (128 bytes total, de-interleaved)

    Wave8Byte lane_waves[16];

    // Process each of the 8 symbols
    for (int symbol_idx = 0; symbol_idx < 8; symbol_idx++) {
        u8 lane_bytes[16] = {0};

        // Process 8 pulses (16 bytes total: 2 bytes per pulse)
        for (int pulse_idx = 0; pulse_idx < 8; pulse_idx++) {
            int pulse_bit = 7 - pulse_idx;

            // Read 16-bit word for this pulse
            int input_offset = symbol_idx * 16 + pulse_idx * 2;
            u16 input_word = (u16)transposed[input_offset] |
                                  ((u16)transposed[input_offset + 1] << 8);

            // De-interleave 16 lanes from this word
            // Bit layout: [L15, L14, L13, ..., L1, L0]
            for (int lane = 0; lane < 16; lane++) {
                // Extract bit for this lane (lane 0 = LSB, lane 15 = MSB)
                u8 pulse = (input_word >> lane) & 1;

                // Reconstruct lane byte
                lane_bytes[lane] |= (pulse << pulse_bit);
            }
        }

        // Store de-interleaved bytes
        for (int lane = 0; lane < 16; lane++) {
            lane_waves[lane].symbols[symbol_idx].data = lane_bytes[lane];
        }
    }

    // Copy de-interleaved data to output
    for (int lane = 0; lane < 16; lane++) {
        fl::isr::memcpy(output + lane * sizeof(Wave8Byte), &lane_waves[lane], sizeof(Wave8Byte));
    }
}

} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
