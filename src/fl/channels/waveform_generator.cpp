/// @file waveform_generator.cpp
/// @brief Implementation of generic waveform generator for clockless LED protocols

#include "waveform_generator.h"
#include "fl/math_macros.h"
#include "ftl/cstring.h"

namespace fl {

// ============================================================================
// Waveform Generator Functions
// ============================================================================

size_t generateBit0Waveform(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    uint8_t* b0_waveform,
    size_t b0_waveform_size_max
) {
    // Validate inputs
    if (hz == 0 || t1_ns == 0 || t2_ns == 0 || t3_ns == 0) {
        return 0;
    }

    // Calculate resolution: nanoseconds per pulse
    // resolution_ns = 1,000,000,000 / hz
    uint32_t resolution_ns = 1000000000UL / hz;
    if (resolution_ns == 0) {
        return 0;  // Frequency too high
    }

    // Calculate pulses using ceiling division
    uint32_t pulses_t1 = (t1_ns + resolution_ns - 1) / resolution_ns;
    uint32_t pulses_t2 = (t2_ns + resolution_ns - 1) / resolution_ns;
    uint32_t pulses_t3 = (t3_ns + resolution_ns - 1) / resolution_ns;

    uint32_t total_pulses = pulses_t1 + pulses_t2 + pulses_t3;

    if (b0_waveform_size_max < total_pulses) {
        return 0;  // Buffer too small
    }

    // Bit 0: HIGH for t1, LOW for t2+t3
    size_t idx = 0;
    for (uint32_t i = 0; i < pulses_t1; i++) {
        b0_waveform[idx++] = 0xFF;  // HIGH
    }
    for (uint32_t i = 0; i < pulses_t2 + pulses_t3; i++) {
        b0_waveform[idx++] = 0x00;  // LOW
    }

    return total_pulses;
}

size_t generateBit1Waveform(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    uint8_t* b1_waveform,
    size_t b1_waveform_size_max
) {
    // Validate inputs
    if (hz == 0 || t1_ns == 0 || t2_ns == 0 || t3_ns == 0) {
        return 0;
    }

    // Calculate resolution: nanoseconds per pulse
    uint32_t resolution_ns = 1000000000UL / hz;
    if (resolution_ns == 0) {
        return 0;  // Frequency too high
    }

    // Calculate pulses using ceiling division
    uint32_t pulses_t1 = (t1_ns + resolution_ns - 1) / resolution_ns;
    uint32_t pulses_t2 = (t2_ns + resolution_ns - 1) / resolution_ns;
    uint32_t pulses_t3 = (t3_ns + resolution_ns - 1) / resolution_ns;

    uint32_t total_pulses = pulses_t1 + pulses_t2 + pulses_t3;

    if (b1_waveform_size_max < total_pulses) {
        return 0;  // Buffer too small
    }

    // Bit 1: HIGH for t1+t2, LOW for t3
    size_t idx = 0;
    for (uint32_t i = 0; i < pulses_t1 + pulses_t2; i++) {
        b1_waveform[idx++] = 0xFF;  // HIGH
    }
    for (uint32_t i = 0; i < pulses_t3; i++) {
        b1_waveform[idx++] = 0x00;  // LOW
    }

    return total_pulses;
}

FL_IRAM size_t expandByteToWaveforms(
    uint8_t dataByte,
    const uint8_t* b0_waveform,
    size_t b0_waveform_size_max,
    const uint8_t* b1_waveform,
    size_t b1_waveform_size_max,
    uint8_t* output,
    size_t output_size
) {
    // Validate inputs
    if (!b0_waveform || !b1_waveform || !output) {
        return 0;  // Null pointer check
    }
    if (b0_waveform_size_max != b1_waveform_size_max || b0_waveform_size_max == 0) {
        return 0;
    }

    const size_t pulsesPerBit = b0_waveform_size_max;
    const size_t totalSize = 8 * pulsesPerBit;

    if (output_size < totalSize) {
        return 0;
    }

    // Expand each bit (MSB first) to its waveform
    size_t outIdx = 0;
    for (int bitPos = 7; bitPos >= 0; bitPos--) {
        bool bitValue = (dataByte >> bitPos) & 0x01;
        const uint8_t* waveform = bitValue ? b1_waveform : b0_waveform;

        // Copy waveform for this bit
        for (size_t i = 0; i < pulsesPerBit; i++) {
            output[outIdx++] = waveform[i];
        }
    }

    return totalSize;
}

}  // namespace fl
