/// @file wave8_encoder_spi.h
/// @brief Wave8 encoding for ESP32 SPI channel engine
///
/// This encoder converts LED byte data into SPI waveforms using wave8 expansion,
/// similar to the PARLIO driver but optimized for SPI hardware characteristics.
///
/// Key differences from PARLIO:
/// - Single-lane mode (ESP32-C3): No transposition needed (direct wave8 encoding)
/// - Multi-lane mode (dual/quad): Requires transposition for parallel transmission
/// - SPI-specific timing: Uses SPI clock divider and bit patterns
///
/// Architecture:
/// 1. Build Wave8BitExpansionLut from ChipsetTiming (one-time setup)
/// 2. Single-lane: wave8_convert_byte_to_wave8byte() -> output (8 bytes per LED byte)
/// 3. Multi-lane: wave8_convert + wave8Transpose_N() -> interleaved output
///
/// Performance:
/// - Single-lane: 1 LUT lookup per byte (2 nibbles), 8 bytes output
/// - Multi-lane: N LUT lookups + transpose (16-128 bytes output depending on lane count)

#pragma once

#include "fl/channels/wave8.h"
#include "fl/channels/detail/wave8.hpp"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/warn.h"

// ESP32-specific: check feature flag for clockless SPI support
#if defined(ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

namespace fl {

/// @brief Encode single-lane LED data using wave8 expansion (no transposition)
///
/// Each LED byte expands to 8 bytes (1 Wave8Byte) without interleaving.
///
/// @param input LED byte data (typically RGB values)
/// @param output Output buffer for wave8-encoded SPI bits
/// @param lut Wave8 expansion lookup table (built from timing)
/// @return Number of bytes written to output (input.size() * 8)
inline size_t wave8EncodeSingleLane(
    fl::span<const uint8_t> input,
    fl::span<uint8_t> output,
    const Wave8BitExpansionLut& lut) {

    const size_t required_size = input.size() * 8;
    if (output.size() < required_size) {
        FL_WARN("wave8EncodeSingleLane: Output buffer too small (need "
                << required_size << " bytes, have " << output.size() << " bytes)");
        return 0;
    }

    size_t output_idx = 0;
    for (size_t i = 0; i < input.size(); i++) {
        Wave8Byte wave8_output;
        detail::wave8_convert_byte_to_wave8byte(input[i], lut, &wave8_output);
        fl::memcpy(output.data() + output_idx, &wave8_output, sizeof(Wave8Byte));
        output_idx += sizeof(Wave8Byte);
    }

    return output_idx;
}

/// @brief Encode dual-lane LED data using wave8 + 2-lane transposition
///
/// For ESP32 dual-SPI mode (2 parallel data lines). Each pair of LED bytes
/// expands to 16 bytes (2 Wave8Byte structures transposed).
///
/// @param lane0 LED byte data for lane 0
/// @param lane1 LED byte data for lane 1
/// @param output Output buffer for transposed wave8 data
/// @param lut Wave8 expansion lookup table
/// @return Number of bytes written to output (lane0.size() * 16)
inline size_t wave8EncodeDualLane(
    fl::span<const uint8_t> lane0,
    fl::span<const uint8_t> lane1,
    fl::span<uint8_t> output,
    const Wave8BitExpansionLut& lut) {

    if (lane0.size() != lane1.size()) {
        FL_WARN("wave8EncodeDualLane: Lane sizes mismatch (lane0="
                << lane0.size() << " bytes, lane1=" << lane1.size() << " bytes)");
        return 0;
    }

    const size_t required_size = lane0.size() * 16;
    if (output.size() < required_size) {
        FL_WARN("wave8EncodeDualLane: Output buffer too small (need "
                << required_size << " bytes, have " << output.size() << " bytes)");
        return 0;
    }

    size_t output_idx = 0;
    for (size_t i = 0; i < lane0.size(); i++) {
        Wave8Byte wave8_lane0, wave8_lane1;
        detail::wave8_convert_byte_to_wave8byte(lane0[i], lut, &wave8_lane0);
        detail::wave8_convert_byte_to_wave8byte(lane1[i], lut, &wave8_lane1);

        Wave8Byte lane_array[2] = {wave8_lane0, wave8_lane1};
        uint8_t transposed[2 * sizeof(Wave8Byte)];
        detail::wave8_transpose_2(lane_array, transposed);

        fl::memcpy(output.data() + output_idx, transposed, sizeof(transposed));
        output_idx += sizeof(transposed);
    }

    return output_idx;
}

/// @brief Encode quad-lane LED data using wave8 + 4-lane transposition
///
/// For ESP32-S3 quad-SPI mode (4 parallel data lines). Each set of 4 LED bytes
/// expands to 32 bytes (4 Wave8Byte structures transposed).
///
/// @param lanes Array of 4 lane spans (must all have same size)
/// @param output Output buffer for transposed wave8 data
/// @param lut Wave8 expansion lookup table
/// @return Number of bytes written to output (lanes[0].size() * 32)
inline size_t wave8EncodeQuadLane(
    fl::span<const uint8_t> lanes[4],
    fl::span<uint8_t> output,
    const Wave8BitExpansionLut& lut) {

    const size_t lane_size = lanes[0].size();
    for (int i = 1; i < 4; i++) {
        if (lanes[i].size() != lane_size) {
            FL_WARN("wave8EncodeQuadLane: Lane size mismatch (lane0="
                    << lane_size << " bytes, lane" << i << "=" << lanes[i].size() << " bytes)");
            return 0;
        }
    }

    const size_t required_size = lane_size * 32;
    if (output.size() < required_size) {
        FL_WARN("wave8EncodeQuadLane: Output buffer too small (need "
                << required_size << " bytes, have " << output.size() << " bytes)");
        return 0;
    }

    size_t output_idx = 0;
    for (size_t i = 0; i < lane_size; i++) {
        Wave8Byte wave8_lanes[4];
        for (int lane = 0; lane < 4; lane++) {
            detail::wave8_convert_byte_to_wave8byte(lanes[lane][i], lut, &wave8_lanes[lane]);
        }

        uint8_t transposed[4 * sizeof(Wave8Byte)];
        detail::wave8_transpose_4(wave8_lanes, transposed);

        fl::memcpy(output.data() + output_idx, transposed, sizeof(transposed));
        output_idx += sizeof(transposed);
    }

    return output_idx;
}

/// @brief Calculate required output buffer size for wave8 encoding
///
/// Formula:
/// - Single-lane: input_bytes * 8 (1 Wave8Byte per byte)
/// - Dual-lane:   input_bytes * 16 (2 Wave8Byte transposed per byte pair)
/// - Quad-lane:   input_bytes * 32 (4 Wave8Byte transposed per byte set)
///
/// @param input_bytes Number of input LED bytes
/// @param num_lanes Number of SPI lanes (1, 2, or 4)
/// @return Required output buffer size in bytes
constexpr size_t wave8CalculateOutputSize(size_t input_bytes, uint8_t num_lanes) {
    return input_bytes * 8 * num_lanes;
}

} // namespace fl
