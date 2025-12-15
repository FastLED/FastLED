/// @file wave_transpose.cpp
/// @brief Waveform generation and transposition implementation

#include "wave_transpose.h"
#include "ftl/cstring.h"
#include "fl/channels/waveform_generator.h"

namespace fl {

// ============================================================================
// Simplified waveTranspose8_2 (fixed 8:1 expansion, LUT-based, no branching)
// ============================================================================

/// @brief Transpose two 64-byte pulse arrays into 16-byte bit-packed output
///
/// This helper function converts two arrays of pulse bytes (0xFF/0x00) into
/// a bit-packed DMA format for 2-lane parallel transmission.
///
/// **Input:** Two 64-byte arrays (lane0_pulses, lane1_pulses)
/// - Each byte is either 0xFF (HIGH pulse) or 0x00 (LOW pulse)
///
/// **Output:** 16 bytes of bit-packed data
/// - Each output byte packs 4 time ticks
/// - Each tick has 2 bits (one per lane)
/// - Bit packing: [t0_L0, t0_L1, t1_L0, t1_L1, t2_L0, t2_L1, t3_L0, t3_L1]
///
/// @param lane0_pulses Lane 0 pulse array (64 bytes)
/// @param lane1_pulses Lane 1 pulse array (64 bytes)
/// @param output Output buffer (16 bytes)
static FL_IRAM void transpose_2lane_pulses(
    const uint8_t lane0_pulses[64],
    const uint8_t lane1_pulses[64],
    uint8_t output[16]
) {
    // Constants for clarity
    constexpr size_t DATA_WIDTH = 2;        // 2 lanes
    constexpr size_t TICKS_PER_BYTE = 4;    // 8 bits / 2 lanes = 4 ticks
    constexpr size_t NUM_OUTPUT_BYTES = 16; // 64 pulses / 4 ticks = 16 bytes

    // Transpose pulses into bit-packed format
    for (size_t byteIdx = 0; byteIdx < NUM_OUTPUT_BYTES; byteIdx++) {
        uint8_t outputByte = 0;

        // Pack TICKS_PER_BYTE (4) time ticks into this output byte
        for (size_t t = 0; t < TICKS_PER_BYTE; t++) {
            size_t tick = byteIdx * TICKS_PER_BYTE + t;

            // Extract pulses from both lanes at this time tick
            uint8_t pulse0 = lane0_pulses[tick];
            uint8_t pulse1 = lane1_pulses[tick];

            // Convert 0xFF→1, 0x00→0
            uint8_t bit0 = (pulse0 != 0) ? 1 : 0;
            uint8_t bit1 = (pulse1 != 0) ? 1 : 0;

            // Bit position: temporal order (lane0, lane1, lane0, lane1, ...)
            // PARLIO transmits LSB-first, so this gives correct temporal sequence
            size_t bitPos0 = t * DATA_WIDTH + 0; // lane0 bit position
            size_t bitPos1 = t * DATA_WIDTH + 1; // lane1 bit position

            outputByte |= (bit0 << bitPos0);
            outputByte |= (bit1 << bitPos1);
        }

        output[byteIdx] = outputByte;
    }
}

// GCC-specific optimization attribute (Clang ignores it)
FL_OPTIMIZE_FUNCTION_O3
FL_IRAM void waveTranspose8_2(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut& lut,
    uint8_t (&FL_RESTRICT_PARAM output)[16]
) {
    // Constants for clarity
    constexpr size_t PULSES_PER_LANE = 8; // 8 WavePulses8 structures per byte (8 bits × 1 WavePulses8/bit)

    // Allocate waveform buffers on stack (8 WavePulses8 per lane, 2 lanes)
    WavePulses8 laneWaveforms[2][PULSES_PER_LANE];

    // Expand both lanes using LUT (no branching!)
    for (size_t lane = 0; lane < 2; lane++) {
        uint8_t laneValue = lanes[lane];

        // Process high nibble (bits 7-4), then low nibble (bits 3-0)
        uint8_t highNibble = laneValue >> 4;
        uint8_t lowNibble = laneValue & 0x0F;

        // Copy high nibble waveform (4 WavePulses8) to first half of lane buffer
        for (size_t i = 0; i < 4; i++) {
            laneWaveforms[lane][i] = lut.data[highNibble][i];
        }

        // Copy low nibble waveform (4 WavePulses8) to second half of lane buffer
        for (size_t i = 0; i < 4; i++) {
            laneWaveforms[lane][4 + i] = lut.data[lowNibble][i];
        }
    }

    // Transpose waveforms to DMA format (reinterpret as flat byte arrays)
    transpose_2lane_pulses(
        reinterpret_cast<const uint8_t*>(laneWaveforms[0]),
        reinterpret_cast<const uint8_t*>(laneWaveforms[1]),
        output
    );
}



} // namespace fl
