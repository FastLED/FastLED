/// @file wave8_encoder_i2s.cpp
/// @brief Implementation of wave8 encoding for ESP32-S3 I2S LCD_CAM

#include "fl/compiler_control.h"
#include "wave8_encoder_i2s.h"
#include "fl/channels/detail/wave8.hpp"
#include "fl/stl/algorithm.h"
#include "fl/dbg.h"
#include "fl/warn.h"

namespace fl {

size_t wave8EncodeI2sSingleLane(
    fl::span<const uint8_t> input,
    fl::span<uint16_t> output,
    const Wave8BitExpansionLut& lut) {

    // Calculate required output size
    const size_t required_words = wave8CalculateI2sOutputSize(input.size());
    if (output.size() < required_words) {
        FL_WARN("wave8EncodeI2sSingleLane: Output buffer too small (need "
                << required_words << " words, have " << output.size() << ")");
        return 0;
    }

    size_t output_idx = 0;

    // Process each input byte
    for (size_t i = 0; i < input.size(); i++) {
        // Convert byte to Wave8Byte (8 bytes = 64 bits of waveform)
        Wave8Byte wave8_output;
        detail::wave8_convert_byte_to_wave8byte(input[i], lut, &wave8_output);

        // Pack Wave8Byte into I2S 16-bit words
        // Each Wave8Bit (1 byte) contains 8 pulses, but we need to expand each
        // pulse to one clock cycle on D0
        //
        // For I2S: each uint16_t represents one parallel output cycle
        // We output 1 bit per clock for single-lane mode
        //
        // Wave8Byte = 8 bytes = 64 bits of pulse data
        // Output = 64 uint16_t words (each with D0 = pulse value)

        for (int sym = 0; sym < 8; sym++) {
            uint8_t wave8_bits = wave8_output.symbols[sym].data;

            // Each bit in wave8_bits becomes one output word
            for (int bit = 7; bit >= 0; bit--) {
                uint16_t word = ((wave8_bits >> bit) & 1);
                output[output_idx++] = word;
            }
        }
    }

    return output_idx;
}

size_t wave8EncodeI2sMultiLane(
    fl::span<const uint8_t>* lanes,
    int num_lanes,
    fl::span<uint16_t> output,
    const Wave8BitExpansionLut& lut) {

    if (num_lanes < 1 || num_lanes > 16 || lanes == nullptr) {
        return 0;
    }

    // All lanes must have same size
    const size_t lane_size = lanes[0].size();
    for (int i = 1; i < num_lanes; i++) {
        if (lanes[i].size() != lane_size) {
            FL_WARN("wave8EncodeI2sMultiLane: Lane size mismatch");
            return 0;
        }
    }

    // Calculate required output size
    const size_t required_words = wave8CalculateI2sOutputSize(lane_size);
    if (output.size() < required_words) {
        FL_WARN("wave8EncodeI2sMultiLane: Output buffer too small");
        return 0;
    }

    size_t output_idx = 0;

    // Process each byte position across all lanes
    for (size_t byte_idx = 0; byte_idx < lane_size; byte_idx++) {
        // Convert each lane's byte to Wave8Byte
        Wave8Byte wave8_lanes[16];
        for (int lane = 0; lane < num_lanes; lane++) {
            detail::wave8_convert_byte_to_wave8byte(
                lanes[lane][byte_idx], lut, &wave8_lanes[lane]);
        }

        // Zero unused lanes
        for (int lane = num_lanes; lane < 16; lane++) {
            fl::memset(&wave8_lanes[lane], 0, sizeof(Wave8Byte));
        }

        // Interleave all lanes into output
        // For each of the 8 symbols (bits from original byte):
        for (int sym = 0; sym < 8; sym++) {
            // For each of the 8 pulses in the symbol:
            for (int pulse = 7; pulse >= 0; pulse--) {
                uint16_t word = 0;

                // Gather pulse bit from each lane
                for (int lane = 0; lane < num_lanes; lane++) {
                    uint8_t wave8_bits = wave8_lanes[lane].symbols[sym].data;
                    uint16_t bit = (wave8_bits >> pulse) & 1;
                    word |= (bit << lane);
                }

                output[output_idx++] = word;
            }
        }
    }

    return output_idx;
}

uint32_t calculateI2sClockHz(const ChipsetTiming& timing) {
    // Calculate total period for one LED bit
    // WS2812: T1 (T0H/T1H start) + T2 (middle) + T3 (T0L/T1L end)
    // Total period = T1 + T2 + T3 nanoseconds
    uint32_t total_period_ns = timing.T1 + timing.T2 + timing.T3;

    // Wave8 expands each bit to 8 pulses
    // Pulse period = total_period_ns / 8
    uint32_t pulse_period_ns = total_period_ns / 8;

    // Clock frequency = 1 / pulse_period
    // f = 1e9 / pulse_period_ns
    uint32_t clock_hz = 1000000000U / pulse_period_ns;

    return clock_hz;
}

} // namespace fl
