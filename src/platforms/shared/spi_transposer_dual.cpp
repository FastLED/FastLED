#include "spi_transposer_dual.h"
#include "fl/math_macros.h"

namespace fl {

bool SPITransposerDual::transpose(const fl::optional<LaneData>& lane0,
                                   const fl::optional<LaneData>& lane1,
                                   fl::span<uint8_t> output,
                                   const char** error) {
    // Validate output buffer size (must be divisible by 2)
    if (output.size() % 2 != 0) {
        if (error) {
            *error = "Output buffer size must be divisible by 2";
        }
        return false;
    }

    // Calculate max lane size from output buffer
    const size_t max_size = output.size() / 2;

    // Handle empty case
    if (max_size == 0) {
        if (error) {
            *error = nullptr;  // No error, just empty
        }
        return true;
    }

    // Create array of lane references for easier iteration
    const fl::optional<LaneData>* lanes[2] = {&lane0, &lane1};

    // Determine default padding byte from first available lane
    uint8_t default_padding = 0x00;
    for (size_t i = 0; i < 2; i++) {
        if (lanes[i]->has_value() && !(*lanes[i])->padding_frame.empty()) {
            default_padding = (*lanes[i])->padding_frame[0];
            break;
        }
    }

    // Process each input byte with optimized interleaving
    for (size_t byte_idx = 0; byte_idx < max_size; byte_idx++) {
        uint8_t lane_bytes[2];

        // Gather bytes from each lane (handles padding automatically)
        for (size_t lane = 0; lane < 2; lane++) {
            if (lanes[lane]->has_value()) {
                lane_bytes[lane] = getLaneByte(**lanes[lane], byte_idx, max_size);
            } else {
                // Empty lane - use default padding
                lane_bytes[lane] = default_padding;
            }
        }

        // Interleave this byte from both lanes (produces 2 output bytes)
        // Uses optimized function for better performance
        interleave_byte_optimized(&output[byte_idx * 2],
                                  lane_bytes[0], lane_bytes[1]);
    }

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

void SPITransposerDual::interleave_byte_optimized(uint8_t* dest,
                                                   uint8_t a, uint8_t b) {
    // Each output byte contains 4 bits from each input lane
    // Output format: [b3 a3 b2 a2 b1 a1 b0 a0]
    // where a=lane0, b=lane1

    // Extract and combine 4-bit nibbles from each lane
    // First output byte: bits 7:4 from each lane
    dest[0] = ((a >> 4) & 0x0F) | (((b >> 4) & 0x0F) << 4);

    // Second output byte: bits 3:0 from each lane
    dest[1] = (a & 0x0F) | ((b & 0x0F) << 4);
}

uint8_t SPITransposerDual::getLaneByte(const LaneData& lane, size_t byte_idx, size_t max_size) {
    // Calculate padding needed for this lane
    const size_t lane_size = lane.payload.size();
    const size_t padding_bytes = max_size - lane_size;

    // If we're in the padding region (prepended to beginning)
    if (byte_idx < padding_bytes) {
        // Return padding frame byte (repeating pattern)
        if (!lane.padding_frame.empty()) {
            return lane.padding_frame[byte_idx % lane.padding_frame.size()];
        }
        return 0x00;  // Fallback to zero
    }

    // We're in the data region
    const size_t data_idx = byte_idx - padding_bytes;
    if (data_idx < lane_size) {
        return lane.payload[data_idx];
    }

    // Should never reach here if max_size is correct
    return 0x00;
}

}  // namespace fl
