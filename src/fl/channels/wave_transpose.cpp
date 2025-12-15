/// @file wave_transpose.cpp
/// @brief Waveform generation and transposition implementation

#include "wave_transpose.h"

namespace fl {

// ============================================================================
// Simplified waveTranspose8_2 (fixed 8:1 expansion, LUT-based, no branching)
// ============================================================================

/// @brief Transpose two 64-byte pulse arrays into 16-byte bit-packed output
///
/// This helper function converts two arrays of pulse bytes (0xFF/0x00) into
/// a bit-packed DMA format for 2-lane parallel transmission.
///
/// Each lane has 64 bytes (8 WavePulses8 × 8 bytes each). Each pulse byte
/// is either 0xFF (bit=1) or 0x00 (bit=0). We extract these bits and
/// interleave them into 16 output bytes (128 bits total).
///
/// @param lane_waves Array of 2 lane waveforms (64 bytes each)
/// @param output Output buffer (16 bytes = 128 bits)
static FL_IRAM void transpose_2lane_pulses(
    const WavePulses8Symbol lane_waves[2],
    uint8_t output[2*sizeof(WavePulses8Symbol)]
) {
    // Cast to byte arrays for easier indexing (64 bytes per lane)
    const uint8_t* lane0 = reinterpret_cast<const uint8_t*>(&lane_waves[0]);
    const uint8_t* lane1 = reinterpret_cast<const uint8_t*>(&lane_waves[1]);

    // Each output byte contains 4 bit-pairs (8 bits total)
    // Format: [lane0_bit3, lane1_bit3, lane0_bit2, lane1_bit2, lane0_bit1, lane1_bit1, lane0_bit0, lane1_bit0]
    for (int out_byte = 0; out_byte < 16; out_byte++) {
        uint8_t result = 0;

        // Each output byte packs 4 pairs of bits from consecutive positions
        for (int pair = 0; pair < 4; pair++) {
            int src_index = out_byte * 4 + pair;

            // Extract bit from each lane (0xFF → 1, 0x00 → 0)
            uint8_t bit0 = (lane0[src_index] != 0) ? 1 : 0;
            uint8_t bit1 = (lane1[src_index] != 0) ? 1 : 0;

            // Pack bits in MSB-first order: lane0 in bit 7, lane1 in bit 6, etc.
            result |= (bit0 << (7 - pair * 2));
            result |= (bit1 << (6 - pair * 2));
        }

        output[out_byte] = result;
    }
}

/// @brief Convert a single bit to WavePulses8 using LUT
///
/// Looks up the pre-computed pulse pattern for a single bit value.
///
/// @param bit_value The bit to expand (0 or 1)
/// @param lut Pre-computed bit expansion lookup table
/// @return WavePulses8 structure containing the pulse pattern
static FL_IRAM WavePulses8 convertBitToWavePulse8(
    bool bit_value,
    const Wave8BitExpansionLut& lut
) {
    return lut.lut[bit_value ? 1 : 0];
}

static FL_IRAM void convertByteToWavePulses8Symbol(
    uint8_t byte_value,
    const Wave8BitExpansionLut& lut,
    WavePulses8Symbol* output
) {
    // Expand each bit (MSB first) to WavePulses8
    for (int i = 0; i < 8; i++) {
        bool bit = (byte_value >> (7 - i)) & 1;
        output->symbols[i] = convertBitToWavePulse8(bit, lut);
    }
}

// GCC-specific optimization attribute (Clang ignores it)
FL_OPTIMIZE_FUNCTION_O3
FL_IRAM void waveTranspose8_2(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut& lut,
    uint8_t (&FL_RESTRICT_PARAM output)[16]
) {
    // Allocate waveform buffers on stack (16 WavePulses8 total: 8 per lane × 2 lanes)
    // Layout: [Lane0_bit7, Lane0_bit6, ..., Lane0_bit0, Lane1_bit7, Lane1_bit6, ..., Lane1_bit0]
    WavePulses8Symbol laneWaveformSymbols[2];

    // Convert each lane byte to wave pulse symbols
    convertByteToWavePulses8Symbol(lanes[0], lut, &laneWaveformSymbols[0]);
    convertByteToWavePulses8Symbol(lanes[1], lut, &laneWaveformSymbols[1]);

    // Transpose waveforms to DMA format (reinterpret as flat byte arrays)
    transpose_2lane_pulses(
        laneWaveformSymbols,
        output
    );
}



} // namespace fl
