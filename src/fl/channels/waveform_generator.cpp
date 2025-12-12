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

    uint32_t total_time_ns = t1_ns + t2_ns + t3_ns;
    uint32_t total_pulses = (total_time_ns + resolution_ns / 2) / resolution_ns;

    if (b0_waveform_size_max < total_pulses) {
        return 0;  // Buffer too small
    }

    // Allocate pulses proportionally (with rounding to nearest)
    // This ensures timing accuracy while maintaining constant total pulse count
    uint32_t pulses_t1 = (t1_ns * total_pulses + total_time_ns / 2) / total_time_ns;
    uint32_t pulses_t3 = (t3_ns * total_pulses + total_time_ns / 2) / total_time_ns;
    uint32_t pulses_t2 = total_pulses - pulses_t1 - pulses_t3;  // Remainder

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

    uint32_t total_time_ns = t1_ns + t2_ns + t3_ns;
    uint32_t total_pulses = (total_time_ns + resolution_ns / 2) / resolution_ns;

    if (b1_waveform_size_max < total_pulses) {
        return 0;  // Buffer too small
    }

    uint32_t pulses_t1 = (t1_ns * total_pulses + total_time_ns / 2) / total_time_ns;
    uint32_t pulses_t3 = (t3_ns * total_pulses + total_time_ns / 2) / total_time_ns;
    uint32_t pulses_t2 = total_pulses - pulses_t1 - pulses_t3;  // Remainder

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

// ============================================================================
// Nibble Lookup Table Optimization
// ============================================================================

bool generateNibbleLookupTable(
    const uint8_t* b0_waveform,
    const uint8_t* b1_waveform,
    size_t pulsesPerBit,
    NibbleLookupTable& table
) {
    // Validate inputs
    if (!b0_waveform || !b1_waveform || pulsesPerBit == 0) {
        return false;
    }

    // Check if nibble waveform fits in table
    const size_t nibbleSize = 4 * pulsesPerBit;
    if (nibbleSize > MAX_NIBBLE_WAVEFORM_SIZE) {
        return false;  // Waveform too large
    }

    table.nibble_size = nibbleSize;

    // Generate waveform for each nibble value (0x0 to 0xF)
    for (uint8_t nibble = 0; nibble < 16; nibble++) {
        uint8_t* nibbleWaveform = table.data[nibble];
        size_t outIdx = 0;

        // Expand 4 bits (MSB first) to waveforms
        for (int bitPos = 3; bitPos >= 0; bitPos--) {
            bool bitValue = (nibble >> bitPos) & 0x01;
            const uint8_t* waveform = bitValue ? b1_waveform : b0_waveform;

            // Copy waveform for this bit
            for (size_t i = 0; i < pulsesPerBit; i++) {
                nibbleWaveform[outIdx++] = waveform[i];
            }
        }
    }

    return true;
}

FL_IRAM size_t expandByteToWaveformsOptimized(
    uint8_t dataByte,
    NibbleLookupTable table,
    uint8_t* output,
    size_t output_size
) {
    // Validate inputs
    if (!output || table.nibble_size == 0) {
        return 0;
    }

    const size_t totalSize = 2 * table.nibble_size;  // 2 nibbles per byte
    if (output_size < totalSize) {
        return 0;
    }

    // Extract high nibble (bits 7-4) and low nibble (bits 3-0)
    const uint8_t highNibble = (dataByte >> 4) & 0x0F;
    const uint8_t lowNibble = dataByte & 0x0F;

    // Copy high nibble waveform
    const uint8_t* highWaveform = table.data[highNibble];
    for (size_t i = 0; i < table.nibble_size; i++) {
        output[i] = highWaveform[i];
    }

    // Copy low nibble waveform
    const uint8_t* lowWaveform = table.data[lowNibble];
    for (size_t i = 0; i < table.nibble_size; i++) {
        output[table.nibble_size + i] = lowWaveform[i];
    }

    return totalSize;
}

}  // namespace fl
