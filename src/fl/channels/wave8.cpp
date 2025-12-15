/// @file wave8.cpp
/// @brief Waveform generation and transposition implementation

#include "wave8.h"

namespace fl {

constexpr uint8_t kTranspose4_16_LUT[16] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11,
                                            0x14, 0x15, 0x40, 0x41, 0x44, 0x45,
                                            0x50, 0x51, 0x54, 0x55};

#define FL_WAVE8_SPREAD_TO_16(lane_u8_0, lane_u8_1, out_16)                            \
    do {                                                                       \
        const uint8_t _a = (uint8_t)(lane_u8_0);                               \
        const uint8_t _b = (uint8_t)(lane_u8_1);                               \
        const uint16_t _even =                                                 \
            (uint16_t)((uint16_t)kTranspose4_16_LUT[_b & 0x0Fu] |              \
                       ((uint16_t)kTranspose4_16_LUT[_b >> 4] << 8));          \
        const uint16_t _odd =                                                  \
            (uint16_t)(((uint16_t)kTranspose4_16_LUT[_a & 0x0Fu] |             \
                        ((uint16_t)kTranspose4_16_LUT[_a >> 4] << 8))          \
                       << 1);                                                  \
        (out_16) |= (uint16_t)(_even | _odd);                                  \
    } while (0)

// Special optimized transpose function for ISR/DMA contexts.
FL_OPTIMIZE_FUNCTION_O3 FL_IRAM
static void detail_transpose_2(const Wave8Byte lane_waves[2],
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

/// @brief Convert a byte to 8 Wave8Bit structures using nibble LUT
///
/// Expands a byte (8 bits) into 8 Wave8Bit structures (8 bytes total)
/// by looking up the high and low nibbles in the pre-computed LUT.
/// Each nibble lookup returns 4 Wave8Bit structures (4 bytes).
///
/// @param byte_value The byte to expand (0-255)
/// @param lut Pre-computed nibble expansion lookup table
/// @param output Output array for 8 Wave8Bit structures (8 bytes total)
FL_OPTIMIZE_FUNCTION_O3 FL_IRAM
static void convertByteToWave8Byte(uint8_t byte_value,
                                   const Wave8BitExpansionLut &lut,
                                   Wave8Byte *output) {

    // Cache pointer to high nibble data to avoid repeated indexing
    const Wave8Bit *high_nibble_data = lut.lut[(byte_value >> 4) & 0xF];
    for (int i = 0; i < 4; i++) {
        output->symbols[i] = high_nibble_data[i];
    }

    // Cache pointer to low nibble data to avoid repeated indexing
    const Wave8Bit *low_nibble_data = lut.lut[byte_value & 0xF];
    for (int i = 0; i < 4; i++) {
        output->symbols[4 + i] = low_nibble_data[i];
    }
}

// ============================================================================
// LUT Builder from Timing Data
// Doesn't this is not designed to be called from ISR handlers.
// ============================================================================
FL_OPTIMIZE_FUNCTION_O3
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

// GCC-specific optimization attribute (Clang ignores it)
FL_OPTIMIZE_FUNCTION_O3 FL_IRAM
void wave8(uint8_t lane,
           const Wave8BitExpansionLut &lut,
           uint8_t (&FL_RESTRICT_PARAM output)[sizeof(Wave8Byte)]) {
    // Convert single lane byte to wave pulse symbols (8 bytes packed)
    // Use properly aligned local variable to avoid alignment issues
    Wave8Byte waveformSymbol;
    convertByteToWave8Byte(lane, lut, &waveformSymbol);

    // Copy to output array byte-by-byte (sizeof(Wave8Byte) = 8)
    const uint8_t* src = &waveformSymbol.symbols[0].data;
    for (size_t i = 0; i < sizeof(Wave8Byte); i++) {
        output[i] = src[i];
    }
}

// GCC-specific optimization attribute (Clang ignores it)
FL_OPTIMIZE_FUNCTION_O3 FL_IRAM
void wave8Transpose_2(const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
                      const Wave8BitExpansionLut &lut,
                      uint8_t (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave8Byte)]) {
    // Allocate waveform buffers on stack (16 Wave8Bit total: 8 packed bytes per lane × 2 lanes)
    // Each Wave8Byte is 8 bytes (8 Wave8Bit × 1 byte each)
    // Layout: [Lane0_bit7, Lane0_bit6, ..., Lane0_bit0, Lane1_bit7, Lane1_bit6, ..., Lane1_bit0]
    Wave8Byte laneWaveformSymbols[2];

    // Convert each lane byte to wave pulse symbols (8 packed bytes each)
    convertByteToWave8Byte(lanes[0], lut, &laneWaveformSymbols[0]);
    convertByteToWave8Byte(lanes[1], lut, &laneWaveformSymbols[1]);

    // Transpose waveforms to DMA format (interleave 8 packed bytes to 16 bytes)
    detail_transpose_2(laneWaveformSymbols, output);
}



} // namespace fl
