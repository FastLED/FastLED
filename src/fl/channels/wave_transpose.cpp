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
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O3")))
#endif
FL_IRAM void waveTranspose8_2(
    const uint8_t lanes[2],
    const Wave8BitExpansionLut& lut,
    uint8_t output[16]
) {
    // Constants for clarity
    constexpr size_t BYTES_PER_LANE = 64; // 8 bits × 8 pulses/bit

    // Allocate waveform buffers on stack (64 bytes per lane, 2 lanes)
    uint8_t laneWaveforms[2][BYTES_PER_LANE];

    // Expand both lanes using LUT (no branching!)
    for (size_t lane = 0; lane < 2; lane++) {
        uint8_t laneValue = lanes[lane];

        // Process high nibble (bits 7-4), then low nibble (bits 3-0)
        uint8_t highNibble = laneValue >> 4;
        uint8_t lowNibble = laneValue & 0x0F;

        // Copy high nibble waveform (32 bytes) to first half of lane buffer
        for (size_t i = 0; i < 32; i++) {
            laneWaveforms[lane][i] = lut.data[highNibble][i];
        }

        // Copy low nibble waveform (32 bytes) to second half of lane buffer
        for (size_t i = 0; i < 32; i++) {
            laneWaveforms[lane][32 + i] = lut.data[lowNibble][i];
        }
    }

    // Transpose waveforms to DMA format
    transpose_2lane_pulses(laneWaveforms[0], laneWaveforms[1], output);
}

// ============================================================================
// Simplified waveTranspose8_4 (fixed 8:1 expansion, LUT-based, no branching)
// ============================================================================

/// @brief Transpose four 64-byte pulse arrays into 128-byte bit-packed output
///
/// This helper function converts four arrays of pulse bytes (0xFF/0x00) into
/// a bit-packed DMA format for 4-lane parallel transmission.
///
/// **Input:** Four 64-byte arrays (lane0_pulses, lane1_pulses, lane2_pulses, lane3_pulses)
/// - Each byte is either 0xFF (HIGH pulse) or 0x00 (LOW pulse)
///
/// **Output:** 128 bytes of bit-packed data
/// - Each output byte packs 2 time ticks
/// - Each tick has 4 bits (one per lane)
/// - Bit packing: [t0_L0, t0_L1, t0_L2, t0_L3, t1_L0, t1_L1, t1_L2, t1_L3]
///
/// @param lane0_pulses Lane 0 pulse array (64 bytes)
/// @param lane1_pulses Lane 1 pulse array (64 bytes)
/// @param lane2_pulses Lane 2 pulse array (64 bytes)
/// @param lane3_pulses Lane 3 pulse array (64 bytes)
/// @param output Output buffer (128 bytes)
static FL_IRAM void transpose_4lane_pulses(
    const uint8_t lane0_pulses[64],
    const uint8_t lane1_pulses[64],
    const uint8_t lane2_pulses[64],
    const uint8_t lane3_pulses[64],
    uint8_t output[128]
) {
    // Constants for clarity
    constexpr size_t DATA_WIDTH = 4;         // 4 lanes
    constexpr size_t TICKS_PER_BYTE = 2;     // 8 bits / 4 lanes = 2 ticks
    constexpr size_t NUM_OUTPUT_BYTES = 128; // 64 pulses / 2 ticks = 32 bytes per 8 bits, 8 bits * 2 = 128

    // Transpose pulses into bit-packed format
    for (size_t byteIdx = 0; byteIdx < NUM_OUTPUT_BYTES; byteIdx++) {
        uint8_t outputByte = 0;

        // Pack TICKS_PER_BYTE (2) time ticks into this output byte
        for (size_t t = 0; t < TICKS_PER_BYTE; t++) {
            size_t tick = byteIdx * TICKS_PER_BYTE + t;

            // Extract pulses from all four lanes at this time tick
            uint8_t pulse0 = lane0_pulses[tick];
            uint8_t pulse1 = lane1_pulses[tick];
            uint8_t pulse2 = lane2_pulses[tick];
            uint8_t pulse3 = lane3_pulses[tick];

            // Convert 0xFF→1, 0x00→0
            uint8_t bit0 = (pulse0 != 0) ? 1 : 0;
            uint8_t bit1 = (pulse1 != 0) ? 1 : 0;
            uint8_t bit2 = (pulse2 != 0) ? 1 : 0;
            uint8_t bit3 = (pulse3 != 0) ? 1 : 0;

            // Bit position: temporal order (lane0, lane1, lane2, lane3, lane0, lane1, lane2, lane3)
            // PARLIO transmits LSB-first, so this gives correct temporal sequence
            size_t bitPos0 = t * DATA_WIDTH + 0; // lane0 bit position
            size_t bitPos1 = t * DATA_WIDTH + 1; // lane1 bit position
            size_t bitPos2 = t * DATA_WIDTH + 2; // lane2 bit position
            size_t bitPos3 = t * DATA_WIDTH + 3; // lane3 bit position

            outputByte |= (bit0 << bitPos0);
            outputByte |= (bit1 << bitPos1);
            outputByte |= (bit2 << bitPos2);
            outputByte |= (bit3 << bitPos3);
        }

        output[byteIdx] = outputByte;
    }
}

// GCC-specific optimization attribute (Clang ignores it)
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O3")))
#endif
FL_IRAM void waveTranspose8_4(
    const uint8_t lanes[4],
    const Wave8BitExpansionLut& lut,
    uint8_t output[128]
) {
    // Constants for clarity
    constexpr size_t BYTES_PER_LANE = 64; // 8 bits × 8 pulses/bit

    // Allocate waveform buffers on stack (64 bytes per lane, 4 lanes)
    uint8_t laneWaveforms[4][BYTES_PER_LANE];

    // Expand all 4 lanes using LUT (no branching!)
    for (size_t lane = 0; lane < 4; lane++) {
        uint8_t laneValue = lanes[lane];

        // Process high nibble (bits 7-4), then low nibble (bits 3-0)
        uint8_t highNibble = laneValue >> 4;
        uint8_t lowNibble = laneValue & 0x0F;

        // Copy high nibble waveform (32 bytes) to first half of lane buffer
        for (size_t i = 0; i < 32; i++) {
            laneWaveforms[lane][i] = lut.data[highNibble][i];
        }

        // Copy low nibble waveform (32 bytes) to second half of lane buffer
        for (size_t i = 0; i < 32; i++) {
            laneWaveforms[lane][32 + i] = lut.data[lowNibble][i];
        }
    }

    // Transpose waveforms to DMA format
    transpose_4lane_pulses(laneWaveforms[0], laneWaveforms[1], laneWaveforms[2], laneWaveforms[3], output);
}


// ============================================================================
// Simplified waveTranspose8_16 (fixed 8:1 expansion, LUT-based, no branching)
// ============================================================================

/// @brief Transpose sixteen 64-byte pulse arrays into 128-byte bit-packed output
///
/// This helper function converts sixteen arrays of pulse bytes (0xFF/0x00) into
/// a bit-packed DMA format for 16-lane parallel transmission.
///
/// **Input:** Sixteen 64-byte arrays (lane0_pulses, ..., lane15_pulses)
/// - Each byte is either 0xFF (HIGH pulse) or 0x00 (LOW pulse)
///
/// **Output:** 128 bytes of bit-packed data
/// - Each pair of output bytes packs 1 time tick across all 16 lanes
/// - Byte 0: lanes 0-7, Byte 1: lanes 8-15
/// - Bit packing: [t0_L0, t0_L1, ..., t0_L7] [t0_L8, ..., t0_L15]
///
/// @param lane_pulses Array of 16 lane pulse arrays (each 64 bytes)
/// @param output Output buffer (128 bytes)
static FL_IRAM void transpose_16lane_pulses(
    const uint8_t lane_pulses[16][64],
    uint8_t output[128]
) {
    // Constants for clarity
    constexpr size_t BYTES_PER_TICK = 2;     // 16 bits / 8 = 2 bytes
    constexpr size_t NUM_TICKS = 64;         // 64 pulses per lane

    // Transpose pulses into bit-packed format
    for (size_t tick = 0; tick < NUM_TICKS; tick++) {
        // Each tick produces 2 output bytes
        uint8_t outputByte0 = 0;  // lanes 0-7
        uint8_t outputByte1 = 0;  // lanes 8-15

        // Pack lanes 0-7 into first byte
        for (size_t lane = 0; lane < 8; lane++) {
            uint8_t pulse = lane_pulses[lane][tick];
            uint8_t bit = (pulse != 0) ? 1 : 0;
            outputByte0 |= (bit << lane);
        }

        // Pack lanes 8-15 into second byte
        for (size_t lane = 8; lane < 16; lane++) {
            uint8_t pulse = lane_pulses[lane][tick];
            uint8_t bit = (pulse != 0) ? 1 : 0;
            outputByte1 |= (bit << (lane - 8));
        }

        output[tick * BYTES_PER_TICK + 0] = outputByte0;
        output[tick * BYTES_PER_TICK + 1] = outputByte1;
    }
}

// GCC-specific optimization attribute (Clang ignores it)
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O3")))
#endif
FL_IRAM void waveTranspose8_16(
    const uint8_t lanes[16],
    const Wave8BitExpansionLut& lut,
    uint8_t output[128]
) {
    // Constants for clarity
    constexpr size_t BYTES_PER_LANE = 64; // 8 bits x 8 pulses/bit

    // Allocate waveform buffers on stack (64 bytes per lane, 16 lanes)
    uint8_t laneWaveforms[16][BYTES_PER_LANE];

    // Expand all 16 lanes using LUT (no branching!)
    for (size_t lane = 0; lane < 16; lane++) {
        uint8_t laneValue = lanes[lane];

        // Process high nibble (bits 7-4), then low nibble (bits 3-0)
        uint8_t highNibble = laneValue >> 4;
        uint8_t lowNibble = laneValue & 0x0F;

        // Copy high nibble waveform (32 bytes) to first half of lane buffer
        for (size_t i = 0; i < 32; i++) {
            laneWaveforms[lane][i] = lut.data[highNibble][i];
        }

        // Copy low nibble waveform (32 bytes) to second half of lane buffer
        for (size_t i = 0; i < 32; i++) {
            laneWaveforms[lane][32 + i] = lut.data[lowNibble][i];
        }
    }

    // Transpose waveforms to DMA format
    transpose_16lane_pulses(laneWaveforms, output);
}

// ============================================================================
// Simplified waveTranspose8_8 (fixed 8:1 expansion, LUT-based, no branching)
// ============================================================================

/// @brief Transpose eight 64-byte pulse arrays into 64-byte bit-packed output
///
/// This helper function converts eight arrays of pulse bytes (0xFF/0x00) into
/// a bit-packed DMA format for 8-lane parallel transmission.
///
/// **Input:** Eight 64-byte arrays (lane0_pulses through lane7_pulses)
/// - Each byte is either 0xFF (HIGH pulse) or 0x00 (LOW pulse)
///
/// **Output:** 64 bytes of bit-packed data
/// - Each output byte packs 1 time tick
/// - Each tick has 8 bits (one per lane)
/// - Bit packing: [t0_L0, t0_L1, t0_L2, t0_L3, t0_L4, t0_L5, t0_L6, t0_L7]
///
/// @param lane0_pulses Lane 0 pulse array (64 bytes)
/// @param lane1_pulses Lane 1 pulse array (64 bytes)
/// @param lane2_pulses Lane 2 pulse array (64 bytes)
/// @param lane3_pulses Lane 3 pulse array (64 bytes)
/// @param lane4_pulses Lane 4 pulse array (64 bytes)
/// @param lane5_pulses Lane 5 pulse array (64 bytes)
/// @param lane6_pulses Lane 6 pulse array (64 bytes)
/// @param lane7_pulses Lane 7 pulse array (64 bytes)
/// @param output Output buffer (64 bytes)
static FL_IRAM void transpose_8lane_pulses(
    const uint8_t lane0_pulses[64],
    const uint8_t lane1_pulses[64],
    const uint8_t lane2_pulses[64],
    const uint8_t lane3_pulses[64],
    const uint8_t lane4_pulses[64],
    const uint8_t lane5_pulses[64],
    const uint8_t lane6_pulses[64],
    const uint8_t lane7_pulses[64],
    uint8_t output[64]
) {
    // Constants for clarity
    constexpr size_t NUM_TICKS = 64;  // 64 pulses per lane = 64 output bytes (1 tick per byte)

    // Transpose pulses into bit-packed format
    // Special case: 8 lanes fills all 8 bits in one tick (1 tick per byte)
    for (size_t tick = 0; tick < NUM_TICKS; tick++) {
        // Extract pulses from all eight lanes at this time tick
        uint8_t pulse0 = lane0_pulses[tick];
        uint8_t pulse1 = lane1_pulses[tick];
        uint8_t pulse2 = lane2_pulses[tick];
        uint8_t pulse3 = lane3_pulses[tick];
        uint8_t pulse4 = lane4_pulses[tick];
        uint8_t pulse5 = lane5_pulses[tick];
        uint8_t pulse6 = lane6_pulses[tick];
        uint8_t pulse7 = lane7_pulses[tick];

        // Convert 0xFF→1, 0x00→0 and pack into output byte
        // Bit position: temporal order (lane0, lane1, lane2, ..., lane7)
        // PARLIO transmits LSB-first, so this gives correct temporal sequence
        uint8_t outputByte = 0;
        outputByte |= ((pulse0 != 0) ? 1 : 0) << 0;  // lane0 bit position
        outputByte |= ((pulse1 != 0) ? 1 : 0) << 1;  // lane1 bit position
        outputByte |= ((pulse2 != 0) ? 1 : 0) << 2;  // lane2 bit position
        outputByte |= ((pulse3 != 0) ? 1 : 0) << 3;  // lane3 bit position
        outputByte |= ((pulse4 != 0) ? 1 : 0) << 4;  // lane4 bit position
        outputByte |= ((pulse5 != 0) ? 1 : 0) << 5;  // lane5 bit position
        outputByte |= ((pulse6 != 0) ? 1 : 0) << 6;  // lane6 bit position
        outputByte |= ((pulse7 != 0) ? 1 : 0) << 7;  // lane7 bit position

        output[tick] = outputByte;
    }
}

// GCC-specific optimization attribute (Clang ignores it)
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O3")))
#endif
FL_IRAM void waveTranspose8_8(
    const uint8_t lanes[8],
    const Wave8BitExpansionLut& lut,
    uint8_t output[64]
) {
    // Constants for clarity
    constexpr size_t BYTES_PER_LANE = 64; // 8 bits × 8 pulses/bit

    // Allocate waveform buffers on stack (64 bytes per lane, 8 lanes)
    uint8_t laneWaveforms[8][BYTES_PER_LANE];

    // Expand all 8 lanes using LUT (no branching!)
    for (size_t lane = 0; lane < 8; lane++) {
        uint8_t laneValue = lanes[lane];

        // Process high nibble (bits 7-4), then low nibble (bits 3-0)
        uint8_t highNibble = laneValue >> 4;
        uint8_t lowNibble = laneValue & 0x0F;

        // Copy high nibble waveform (32 bytes) to first half of lane buffer
        for (size_t i = 0; i < 32; i++) {
            laneWaveforms[lane][i] = lut.data[highNibble][i];
        }

        // Copy low nibble waveform (32 bytes) to second half of lane buffer
        for (size_t i = 0; i < 32; i++) {
            laneWaveforms[lane][32 + i] = lut.data[lowNibble][i];
        }
    }

    // Transpose waveforms to DMA format
    transpose_8lane_pulses(
        laneWaveforms[0], laneWaveforms[1], laneWaveforms[2], laneWaveforms[3],
        laneWaveforms[4], laneWaveforms[5], laneWaveforms[6], laneWaveforms[7],
        output
    );
}

} // namespace fl
