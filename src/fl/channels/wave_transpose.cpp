/// @file wave_transpose.cpp
/// @brief Waveform generation and transposition implementation

#include "wave_transpose.h"

namespace fl {

// ============================================================================
// Simplified waveTranspose8_2 (fixed 8:1 expansion, LUT-based, no branching)
// ============================================================================

/// @brief Transpose two 64-byte pulse arrays into 128-byte bit-interleaved output
///
/// This helper function performs bit-level interleaving of two pulse arrays
/// for 2-lane parallel transmission to DMA/GPIO.
///
/// Each lane has 64 pulse bytes. For each pair of pulse bytes (one from each lane),
/// we extract all 8 bits and interleave them to produce 2 output bytes (16 bits total).
/// Pattern per output byte: [lane0_bit, lane1_bit, lane0_bit, lane1_bit, ...] = 0xAA when lanes differ
///
/// @param lane_waves Array of 2 lane waveforms (64 bytes each)
/// @param output Output buffer (128 bytes = 64 positions × 2 bytes each)
static FL_IRAM void transpose_wave_symbols8_2(
    const WavePulses8Symbol lane_waves[2],
    uint8_t output[2*sizeof(WavePulses8Symbol)]
) {
    // Cast to byte arrays for easier indexing (64 bytes per lane)
    const uint8_t* lane0 = reinterpret_cast<const uint8_t*>(&lane_waves[0]);
    const uint8_t* lane1 = reinterpret_cast<const uint8_t*>(&lane_waves[1]);

    // For each of 64 pulse byte positions, interleave bits from both lanes
    for (int i = 0; i < 64; i++) {
        uint8_t byte0 = lane0[i];
        uint8_t byte1 = lane1[i];

        // Interleave 8 bits from each lane → 16 bits → 2 output bytes
        // First output byte: high 4 bits from each lane
        uint8_t out0 = 0;
        out0 |= ((byte0 >> 7) & 1) << 7;  // lane0 bit 7
        out0 |= ((byte1 >> 7) & 1) << 6;  // lane1 bit 7
        out0 |= ((byte0 >> 6) & 1) << 5;  // lane0 bit 6
        out0 |= ((byte1 >> 6) & 1) << 4;  // lane1 bit 6
        out0 |= ((byte0 >> 5) & 1) << 3;  // lane0 bit 5
        out0 |= ((byte1 >> 5) & 1) << 2;  // lane1 bit 5
        out0 |= ((byte0 >> 4) & 1) << 1;  // lane0 bit 4
        out0 |= ((byte1 >> 4) & 1) << 0;  // lane1 bit 4

        // Second output byte: low 4 bits from each lane
        uint8_t out1 = 0;
        out1 |= ((byte0 >> 3) & 1) << 7;  // lane0 bit 3
        out1 |= ((byte1 >> 3) & 1) << 6;  // lane1 bit 3
        out1 |= ((byte0 >> 2) & 1) << 5;  // lane0 bit 2
        out1 |= ((byte1 >> 2) & 1) << 4;  // lane1 bit 2
        out1 |= ((byte0 >> 1) & 1) << 3;  // lane0 bit 1
        out1 |= ((byte1 >> 1) & 1) << 2;  // lane1 bit 1
        out1 |= ((byte0 >> 0) & 1) << 1;  // lane0 bit 0
        out1 |= ((byte1 >> 0) & 1) << 0;  // lane1 bit 0

        output[i * 2] = out0;
        output[i * 2 + 1] = out1;
    }
}

/// @brief Convert a byte to 8 WavePulses8 structures using nibble LUT
///
/// Expands a byte (8 bits) into 8 WavePulses8 structures (64 bytes total)
/// by looking up the high and low nibbles in the pre-computed LUT.
/// Each nibble lookup returns 4 WavePulses8 structures.
///
/// @param byte_value The byte to expand (0-255)
/// @param lut Pre-computed nibble expansion lookup table
/// @param output Output array for 8 WavePulses8 structures (64 bytes total)
static FL_IRAM void convertByteToWavePulses8Symbol(
    uint8_t byte_value,
    const Wave8BitExpansionLut& lut,
    WavePulses8Symbol* output
) {

    // Cache pointer to high nibble data to avoid repeated indexing
    const WavePulses8* high_nibble_data = lut.lut[(byte_value >> 4) & 0xF];
    for (int i = 0; i < 4; i++) {
        output->symbols[i] = high_nibble_data[i];
    }

    // Cache pointer to low nibble data to avoid repeated indexing
    const WavePulses8* low_nibble_data = lut.lut[byte_value & 0xF];
    for (int i = 0; i < 4; i++) {
        output->symbols[4 + i] = low_nibble_data[i];
    }
}

// GCC-specific optimization attribute (Clang ignores it)
FL_OPTIMIZE_FUNCTION_O3
FL_IRAM void waveTranspose8_2(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut& lut,
    uint8_t (&FL_RESTRICT_PARAM output)[2 * sizeof(WavePulses8Symbol)]
) {
    // Allocate waveform buffers on stack (16 WavePulses8 total: 8 per lane × 2 lanes)
    // Layout: [Lane0_bit7, Lane0_bit6, ..., Lane0_bit0, Lane1_bit7, Lane1_bit6, ..., Lane1_bit0]
    WavePulses8Symbol laneWaveformSymbols[2];

    // Convert each lane byte to wave pulse symbols
    convertByteToWavePulses8Symbol(lanes[0], lut, &laneWaveformSymbols[0]);
    convertByteToWavePulses8Symbol(lanes[1], lut, &laneWaveformSymbols[1]);

    // Transpose waveforms to DMA format (reinterpret as flat byte arrays)
    transpose_wave_symbols8_2(
        laneWaveformSymbols,
        output
    );
}



} // namespace fl
