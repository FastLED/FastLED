/// @file wave_transpose.cpp
/// @brief Waveform generation and transposition implementation

#include "wave_transpose.h"

namespace fl {


constexpr uint8_t kTranspose4_16_LUT[16] = {
    0x00,0x01,0x04,0x05,
    0x10,0x11,0x14,0x15,
    0x40,0x41,0x44,0x45,
    0x50,0x51,0x54,0x55
};

#define SPREAD8_TO_16(lane_u8_0, lane_u8_1, out_16) do {                           \
    const uint8_t _a = (uint8_t)(lane_u8_0);                                       \
    const uint8_t _b = (uint8_t)(lane_u8_1);                                       \
    const uint16_t _even = (uint16_t)(                                             \
        (uint16_t)kTranspose4_16_LUT[_b & 0x0Fu] |                                    \
        ((uint16_t)kTranspose4_16_LUT[_b >> 4] << 8)                                  \
    );                                                                             \
    const uint16_t _odd  = (uint16_t)(                                             \
        ((uint16_t)kTranspose4_16_LUT[_a & 0x0Fu] |                                   \
         ((uint16_t)kTranspose4_16_LUT[_a >> 4] << 8)) << 1                           \
    );                                                                             \
    (out_16) |= (uint16_t)(_even | _odd);                                          \
} while (0)

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

    // For each of 64 pulse byte positions, interleave bits from both lanes using LUT-based spread
    for (int i = 0; i < 64; i++) {
        uint16_t interleaved = 0;
        SPREAD8_TO_16(lane0[i], lane1[i], interleaved);

        // Write interleaved 16-bit result as two bytes (little-endian)
        output[i * 2] = (uint8_t)(interleaved >> 8);
        output[i * 2 + 1] = (uint8_t)(interleaved & 0xFF);
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
