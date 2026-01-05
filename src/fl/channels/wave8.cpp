/// @file wave8.cpp
/// @brief Waveform generation and transposition implementation
///
/// This file contains:
/// - Non-ISR LUT builder (buildWave8ExpansionLUT)
/// - Public transposition functions (wave8Transpose_2, wave8Transpose_4)
///
/// Note: Inline helper functions are in detail/wave8.hpp

#include "wave8.h"

namespace fl {

// ============================================================================
// Public Transposition Functions
// ============================================================================

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave8Transpose_2(const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
                      const Wave8BitExpansionLut &lut,
                      uint8_t (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]) {
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
void wave8Transpose_4(const uint8_t (&FL_RESTRICT_PARAM lanes)[4],
                      const Wave8BitExpansionLut &lut,
                      uint8_t (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave8Byte)]) {
    // Allocate waveform buffers on stack (32 Wave8Bit total: 8 packed bytes per lane × 4 lanes)
    Wave8Byte laneWaveformSymbols[4];

    // Convert each lane byte to wave pulse symbols (8 packed bytes each)
    for (int lane = 0; lane < 4; lane++) {
        detail::wave8_convert_byte_to_wave8byte(lanes[lane], lut, &laneWaveformSymbols[lane]);
    }

    // Transpose waveforms to DMA format (interleave 32 packed bytes to 32 bytes)
    detail::wave8_transpose_4(laneWaveformSymbols, output);
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
    const uint32_t t0h = timing.T1;                     // T0H: bit 0 goes LOW here
    const uint32_t t1h = timing.T1 + timing.T2;          // T1H: bit 1 goes LOW here
    const uint32_t period = timing.T1 + timing.T2 + timing.T3; // Total period

    // Step 2: Normalize absolute times
    const float t0h_norm = static_cast<float>(t0h) / period;
    const float t1h_norm = static_cast<float>(t1h) / period;

    // Step 3: Convert to pulse counts (fixed 8 pulses per bit)
    // pulses_bit0: number of HIGH pulses for bit 0 (before it goes LOW at t0h)
    // pulses_bit1: number of HIGH pulses for bit 1 (before it goes LOW at t1h)
    uint32_t pulses_bit0 =
        static_cast<uint32_t>(t0h_norm * 8.0f + 0.5f); // round
    uint32_t pulses_bit1 =
        static_cast<uint32_t>(t1h_norm * 8.0f + 0.5f); // round

    // Clamp to valid range [0, 8]
    if (pulses_bit0 > 8)
        pulses_bit0 = 8;
    if (pulses_bit1 > 8)
        pulses_bit1 = 8;

    // Step 4: Generate bit0 and bit1 waveforms (1 byte each, packed format)
    // Each bit represents one pulse (MSB = first pulse)
    uint8_t bit0_waveform = 0;
    uint8_t bit1_waveform = 0;

    // Bit 0: Set MSB bits for HIGH pulses
    for (uint32_t i = 0; i < pulses_bit0; i++) {
        bit0_waveform |= (0x80 >> i);  // Set bit from MSB
    }

    // Bit 1: Set MSB bits for HIGH pulses
    for (uint32_t i = 0; i < pulses_bit1; i++) {
        bit1_waveform |= (0x80 >> i);  // Set bit from MSB
    }

    // Step 5: Build LUT for all 16 nibbles
    for (uint8_t nibble = 0; nibble < 16; nibble++) {
        // For each nibble, generate 4 Wave8Bit (one per bit, MSB first)
        for (int bit_pos = 3; bit_pos >= 0; bit_pos--) {
            // Extract bit (MSB first: bit 3, 2, 1, 0)
            const bool bit_set = (nibble >> bit_pos) & 1;
            const uint8_t waveform = bit_set ? bit1_waveform : bit0_waveform;

            // Store packed waveform to LUT entry
            lut.lut[nibble][3 - bit_pos].data = waveform;
        }
    }

    return lut;
}

} // namespace fl
