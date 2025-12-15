

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "fl/compiler_control.h"

namespace fl {

// ============================================================================
// Wave Pulse Types
// ============================================================================

/// @brief Type-safe container for 8-byte wave pulse pattern
///
/// Represents the pulse expansion of a single bit. Each bit in the LED protocol
/// expands to 8 pulse bytes (e.g., WS2812 bit timing).
///
/// The struct is 8-byte aligned for optimized memory access in ISR/DMA contexts.
struct alignas(8) WavePulses8 {
    uint8_t data[8];
};

struct alignas(8) WavePulses8Symbol {
    WavePulses8 symbols[8];
};



// ============================================================================
// Nibble Lookup Table (LUT) Types and Generator
// ============================================================================

/// @brief Lookup table for nibble-to-waveform expansion
///
/// Maps each 4-bit nibble (0x0 to 0xF) to 4 WavePulses8 structures.
/// This reduces byte conversion from 8 lookups (bit-level) to 2 lookups (nibble-level).
struct alignas(64) Wave8BitExpansionLut {
     WavePulses8 lut[16][4];  // nibble -> 4 WavePulses8 (half symbol)
};


FL_IRAM void waveTranspose8_2(
    const uint8_t (&FL_RESTRICT_PARAM lanes)[2],
    const Wave8BitExpansionLut& lut,
    uint8_t (&FL_RESTRICT_PARAM output)[2 * sizeof(WavePulses8Symbol)]
);


} // namespace fl
