#include "spi_transposer_quad.h"
#include "fl/math_macros.h"

namespace fl {

bool SPITransposerQuad::transpose(const fl::optional<LaneData>& lane0,
                                   const fl::optional<LaneData>& lane1,
                                   const fl::optional<LaneData>& lane2,
                                   const fl::optional<LaneData>& lane3,
                                   fl::span<uint8_t> output,
                                   const char** error) {
    // Validate output buffer size (must be divisible by 4)
    if (output.size() % 4 != 0) {
        if (error) {
            *error = "Output buffer size must be divisible by 4";
        }
        return false;
    }

    // Calculate max lane size from output buffer
    const size_t max_size = output.size() / 4;

    // Handle empty case
    if (max_size == 0) {
        if (error) {
            *error = nullptr;  // No error, just empty
        }
        return true;
    }

    // Create array of lane references for easier iteration
    const fl::optional<LaneData>* lanes[4] = {&lane0, &lane1, &lane2, &lane3};

    // Determine default padding byte from first available lane
    uint8_t default_padding = 0x00;
    for (size_t i = 0; i < 4; i++) {
        if (lanes[i]->has_value() && !(*lanes[i])->padding_frame.empty()) {
            default_padding = (*lanes[i])->padding_frame[0];
            break;
        }
    }

    // Process each input byte with optimized interleaving
    for (size_t byte_idx = 0; byte_idx < max_size; byte_idx++) {
        uint8_t lane_bytes[4];

        // Gather bytes from each lane (handles padding automatically)
        for (size_t lane = 0; lane < 4; lane++) {
            if (lanes[lane]->has_value()) {
                lane_bytes[lane] = getLaneByte(**lanes[lane], byte_idx, max_size);
            } else {
                // Empty lane - use default padding
                lane_bytes[lane] = default_padding;
            }
        }

        // Interleave this byte from all 4 lanes (produces 4 output bytes)
        // Uses optimized function for 2-3× performance improvement
        interleave_byte_optimized(&output[byte_idx * 4],
                                  lane_bytes[0], lane_bytes[1],
                                  lane_bytes[2], lane_bytes[3]);
    }

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

void SPITransposerQuad::interleave_byte_optimized(uint8_t* dest,
                                                   uint8_t a, uint8_t b,
                                                   uint8_t c, uint8_t d) {
    // Each output byte contains 2 bits from each input lane
    // Output format: [d1 d0 c1 c0 b1 b0 a1 a0]
    // where a=lane0, b=lane1, c=lane2, d=lane3

    // Extract and combine 2-bit pairs from each lane
    dest[0] = ((a >> 6) & 0x03) | (((b >> 6) & 0x03) << 2) |
              (((c >> 6) & 0x03) << 4) | (((d >> 6) & 0x03) << 6);
    dest[1] = ((a >> 4) & 0x03) | (((b >> 4) & 0x03) << 2) |
              (((c >> 4) & 0x03) << 4) | (((d >> 4) & 0x03) << 6);
    dest[2] = ((a >> 2) & 0x03) | (((b >> 2) & 0x03) << 2) |
              (((c >> 2) & 0x03) << 4) | (((d >> 2) & 0x03) << 6);
    dest[3] = ( a       & 0x03) | (( b       & 0x03) << 2) |
              (( c       & 0x03) << 4) | (( d       & 0x03) << 6);
}

uint8_t SPITransposerQuad::getLaneByte(const LaneData& lane, size_t byte_idx, size_t max_size) {
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
