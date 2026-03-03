/// @file wave3.cpp.hpp
/// @brief Wave3 waveform generation and transposition implementation
///
/// This file contains:
/// - Non-ISR LUT builder (buildWave3ExpansionLUT)
/// - canUseWave3() eligibility check
/// - wave3ClockFrequencyHz() clock calculation
/// - Public transposition functions (wave3Transpose_2/4/8/16)
/// - Untranspose functions (for testing)

#include "wave3.h"
#include "fl/channels/detail/wave3.hpp"
#include "fl/chipsets/led_timing.h"
#include "fl/isr.h"

namespace fl {

// ============================================================================
// Wave3 Eligibility Check
// ============================================================================

FL_OPTIMIZE_FUNCTION
bool canUseWave3(const ChipsetTiming& timing) {
    const u32 period = timing.T1 + timing.T2 + timing.T3;
    if (period == 0) {
        return false;
    }

    // T0H = T1, T1H = T1 + T2
    const float t0h_norm = static_cast<float>(timing.T1) / period;
    const float t1h_norm = static_cast<float>(timing.T1 + timing.T2) / period;

    // Round to nearest integer tick count out of 3
    u32 ticks_bit0 = static_cast<u32>(t0h_norm * 3.0f + 0.5f);
    u32 ticks_bit1 = static_cast<u32>(t1h_norm * 3.0f + 0.5f);

    // Both must be in [1, 2] (need at least 1 high tick and 1 low tick)
    if (ticks_bit0 < 1 || ticks_bit0 > 2) return false;
    if (ticks_bit1 < 1 || ticks_bit1 > 2) return false;

    // Must be different (one is 1-tick, other is 2-tick)
    if (ticks_bit0 == ticks_bit1) return false;

    return true;
}

// ============================================================================
// Wave3 Clock Frequency
// ============================================================================

FL_OPTIMIZE_FUNCTION
u32 wave3ClockFrequencyHz(const ChipsetTiming& timing) {
    const u32 period = timing.T1 + timing.T2 + timing.T3;
    if (period == 0) {
        return 0;
    }
    // clock_hz = 3,000,000,000 / total_period_ns
    // Use u64 to avoid overflow
    return static_cast<u32>(3000000000ULL / period);
}

// ============================================================================
// LUT Builder from Timing Data
// ============================================================================

FL_OPTIMIZE_FUNCTION
Wave3BitExpansionLut buildWave3ExpansionLUT(const ChipsetTiming& timing) {
    Wave3BitExpansionLut lut;

    const u32 period = timing.T1 + timing.T2 + timing.T3;

    // T0H = T1, T1H = T1 + T2
    const float t0h_norm = static_cast<float>(timing.T1) / period;
    const float t1h_norm = static_cast<float>(timing.T1 + timing.T2) / period;

    // Round to nearest integer tick count out of 3
    u32 ticks_bit0 = static_cast<u32>(t0h_norm * 3.0f + 0.5f);
    u32 ticks_bit1 = static_cast<u32>(t1h_norm * 3.0f + 0.5f);

    // Clamp to valid range [0, 3]
    if (ticks_bit0 > 3) ticks_bit0 = 3;
    if (ticks_bit1 > 3) ticks_bit1 = 3;

    // Generate 3-bit patterns for bit 0 and bit 1
    // Pattern has ticks_bitN high bits followed by (3 - ticks_bitN) low bits
    // MSB first within the 3-bit field
    u8 pattern_bit0 = 0;
    u8 pattern_bit1 = 0;

    for (u32 i = 0; i < ticks_bit0; i++) {
        pattern_bit0 |= (0x4 >> i); // Set bits from MSB: 0x4=100, 0x6=110, 0x7=111
    }
    for (u32 i = 0; i < ticks_bit1; i++) {
        pattern_bit1 |= (0x4 >> i);
    }

    // Build LUT for all 16 nibbles
    // Each nibble maps to 12 bits (4 LED bits × 3 ticks each)
    for (u8 nibble = 0; nibble < 16; nibble++) {
        u16 pattern = 0;
        // Process bits MSB first (bit 3, 2, 1, 0)
        for (int bit_pos = 3; bit_pos >= 0; bit_pos--) {
            bool bit_set = (nibble >> bit_pos) & 1;
            u8 tick_pattern = bit_set ? pattern_bit1 : pattern_bit0;
            pattern = (pattern << 3) | (tick_pattern & 0x7);
        }
        lut.lut[nibble] = pattern;
    }

    return lut;
}

// ============================================================================
// Public Transposition Functions
// ============================================================================

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave3Transpose_2(const u8 (&FL_RESTRICT_PARAM lanes)[2],
                      const Wave3BitExpansionLut& lut,
                      u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave3Byte)]) {
    Wave3Byte laneWaveforms[2];
    detail::wave3_convert_byte_to_wave3byte(lanes[0], lut, &laneWaveforms[0]);
    detail::wave3_convert_byte_to_wave3byte(lanes[1], lut, &laneWaveforms[1]);
    detail::wave3_transpose_2(laneWaveforms, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave3Transpose_4(const u8 (&FL_RESTRICT_PARAM lanes)[4],
                      const Wave3BitExpansionLut& lut,
                      u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave3Byte)]) {
    Wave3Byte laneWaveforms[4];
    for (int lane = 0; lane < 4; lane++) {
        detail::wave3_convert_byte_to_wave3byte(lanes[lane], lut, &laneWaveforms[lane]);
    }
    detail::wave3_transpose_4(laneWaveforms, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave3Transpose_8(const u8 (&FL_RESTRICT_PARAM lanes)[8],
                      const Wave3BitExpansionLut& lut,
                      u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave3Byte)]) {
    Wave3Byte laneWaveforms[8];
    for (int lane = 0; lane < 8; lane++) {
        detail::wave3_convert_byte_to_wave3byte(lanes[lane], lut, &laneWaveforms[lane]);
    }
    detail::wave3_transpose_8(laneWaveforms, output);
}

FL_OPTIMIZE_FUNCTION FL_IRAM
void wave3Transpose_16(const u8 (&FL_RESTRICT_PARAM lanes)[16],
                       const Wave3BitExpansionLut& lut,
                       u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave3Byte)]) {
    Wave3Byte laneWaveforms[16];
    for (int lane = 0; lane < 16; lane++) {
        detail::wave3_convert_byte_to_wave3byte(lanes[lane], lut, &laneWaveforms[lane]);
    }
    detail::wave3_transpose_16(laneWaveforms, output);
}

// ============================================================================
// Untranspose Functions (Testing Only - Not Optimized)
// ============================================================================

FL_OPTIMIZE_FUNCTION
void wave3Untranspose_2(const u8 (&FL_RESTRICT_PARAM transposed)[2 * sizeof(Wave3Byte)],
                        u8 (&FL_RESTRICT_PARAM output)[2 * sizeof(Wave3Byte)]) {
    Wave3Byte lane_waves[2];

    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u16 interleaved = ((u16)transposed[symbol_idx * 2] << 8) |
                               transposed[symbol_idx * 2 + 1];

        u8 lane0_bits = 0;
        u8 lane1_bits = 0;

        for (int bit = 0; bit < 8; bit++) {
            if (interleaved & (1 << (bit * 2 + 1))) {
                lane0_bits |= (1 << bit);
            }
            if (interleaved & (1 << (bit * 2))) {
                lane1_bits |= (1 << bit);
            }
        }

        lane_waves[0].data[symbol_idx] = lane0_bits;
        lane_waves[1].data[symbol_idx] = lane1_bits;
    }

    fl::isr::memcpy(output, &lane_waves[0], sizeof(Wave3Byte));
    fl::isr::memcpy(output + sizeof(Wave3Byte), &lane_waves[1], sizeof(Wave3Byte));
}

FL_OPTIMIZE_FUNCTION
void wave3Untranspose_4(const u8 (&FL_RESTRICT_PARAM transposed)[4 * sizeof(Wave3Byte)],
                        u8 (&FL_RESTRICT_PARAM output)[4 * sizeof(Wave3Byte)]) {
    Wave3Byte lane_waves[4];

    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 lane_bytes[4] = {0, 0, 0, 0};

        for (int byte_idx = 0; byte_idx < 4; byte_idx++) {
            u8 input_byte = transposed[symbol_idx * 4 + byte_idx];

            int pulse_bit_hi = 7 - (byte_idx * 2);
            int pulse_bit_lo = pulse_bit_hi - 1;

            for (int lane = 0; lane < 4; lane++) {
                u8 pulse_hi = (input_byte >> (4 + lane)) & 1;
                u8 pulse_lo = (input_byte >> lane) & 1;
                lane_bytes[lane] |= (pulse_hi << pulse_bit_hi);
                lane_bytes[lane] |= (pulse_lo << pulse_bit_lo);
            }
        }

        for (int lane = 0; lane < 4; lane++) {
            lane_waves[lane].data[symbol_idx] = lane_bytes[lane];
        }
    }

    for (int lane = 0; lane < 4; lane++) {
        fl::isr::memcpy(output + lane * sizeof(Wave3Byte), &lane_waves[lane], sizeof(Wave3Byte));
    }
}

FL_OPTIMIZE_FUNCTION
void wave3Untranspose_8(const u8 (&FL_RESTRICT_PARAM transposed)[8 * sizeof(Wave3Byte)],
                        u8 (&FL_RESTRICT_PARAM output)[8 * sizeof(Wave3Byte)]) {
    Wave3Byte lane_waves[8];

    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 lane_bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        // Hacker's Delight 8x8 transpose outputs columns in LSB-first order:
        // byte 0 = column 0 (bit 0 of each lane), byte 7 = column 7 (bit 7)
        for (int byte_idx = 0; byte_idx < 8; byte_idx++) {
            u8 input_byte = transposed[symbol_idx * 8 + byte_idx];
            int pulse_bit = byte_idx;

            for (int lane = 0; lane < 8; lane++) {
                u8 pulse = (input_byte >> lane) & 1;
                lane_bytes[lane] |= (pulse << pulse_bit);
            }
        }

        for (int lane = 0; lane < 8; lane++) {
            lane_waves[lane].data[symbol_idx] = lane_bytes[lane];
        }
    }

    for (int lane = 0; lane < 8; lane++) {
        fl::isr::memcpy(output + lane * sizeof(Wave3Byte), &lane_waves[lane], sizeof(Wave3Byte));
    }
}

FL_OPTIMIZE_FUNCTION
void wave3Untranspose_16(const u8 (&FL_RESTRICT_PARAM transposed)[16 * sizeof(Wave3Byte)],
                         u8 (&FL_RESTRICT_PARAM output)[16 * sizeof(Wave3Byte)]) {
    Wave3Byte lane_waves[16];

    for (int symbol_idx = 0; symbol_idx < 3; symbol_idx++) {
        u8 lane_bytes[16] = {0};

        for (int pulse_idx = 0; pulse_idx < 8; pulse_idx++) {
            int pulse_bit = 7 - pulse_idx;
            int input_offset = symbol_idx * 16 + pulse_idx * 2;
            u16 input_word = (u16)transposed[input_offset] |
                                  ((u16)transposed[input_offset + 1] << 8);

            for (int lane = 0; lane < 16; lane++) {
                u8 pulse = (input_word >> lane) & 1;
                lane_bytes[lane] |= (pulse << pulse_bit);
            }
        }

        for (int lane = 0; lane < 16; lane++) {
            lane_waves[lane].data[symbol_idx] = lane_bytes[lane];
        }
    }

    for (int lane = 0; lane < 16; lane++) {
        fl::isr::memcpy(output + lane * sizeof(Wave3Byte), &lane_waves[lane], sizeof(Wave3Byte));
    }
}

} // namespace fl
