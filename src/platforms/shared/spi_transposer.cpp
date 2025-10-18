#include "spi_transposer.h"
#include "fl/math_macros.h"

namespace fl {

// ============================================================================
// 2-Way Transpose (Dual-SPI)
// ============================================================================

bool SPITransposer::transpose2(const fl::optional<LaneData>& lane0,
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
        interleave_byte_2way(&output[byte_idx * 2],
                             lane_bytes[0], lane_bytes[1]);
    }

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

void SPITransposer::interleave_byte_2way(uint8_t* dest, uint8_t a, uint8_t b) {
    // Each output byte contains 4 bits from each input lane
    // Output format: [b3 a3 b2 a2 b1 a1 b0 a0]
    // where a=lane0, b=lane1

    // Extract and combine 4-bit nibbles from each lane
    // First output byte: bits 7:4 from each lane
    dest[0] = ((a >> 4) & 0x0F) | (((b >> 4) & 0x0F) << 4);

    // Second output byte: bits 3:0 from each lane
    dest[1] = (a & 0x0F) | ((b & 0x0F) << 4);
}

// ============================================================================
// 4-Way Transpose (Quad-SPI)
// ============================================================================

bool SPITransposer::transpose4(const fl::optional<LaneData>& lane0,
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
        interleave_byte_4way(&output[byte_idx * 4],
                             lane_bytes[0], lane_bytes[1],
                             lane_bytes[2], lane_bytes[3]);
    }

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

void SPITransposer::interleave_byte_4way(uint8_t* dest,
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

// ============================================================================
// 8-Way Transpose (Octal-SPI)
// ============================================================================

bool SPITransposer::transpose8(const fl::optional<LaneData> lanes[8],
                                fl::span<uint8_t> output,
                                const char** error) {
    // Validate output buffer size (must be divisible by 8)
    if (output.size() % 8 != 0) {
        if (error) {
            *error = "Output buffer size must be divisible by 8";
        }
        return false;
    }

    // Calculate max lane size from output buffer
    const size_t max_size = output.size() / 8;

    // Handle empty case
    if (max_size == 0) {
        if (error) {
            *error = nullptr;  // No error, just empty
        }
        return true;
    }

    // Determine default padding byte from first available lane
    uint8_t default_padding = 0x00;
    for (size_t i = 0; i < 8; i++) {
        if (lanes[i].has_value() && !lanes[i]->padding_frame.empty()) {
            default_padding = lanes[i]->padding_frame[0];
            break;
        }
    }

    // Process each input byte with optimized interleaving
    for (size_t byte_idx = 0; byte_idx < max_size; byte_idx++) {
        uint8_t lane_bytes[8];

        // Gather bytes from each lane (handles padding automatically)
        for (size_t lane = 0; lane < 8; lane++) {
            if (lanes[lane].has_value()) {
                lane_bytes[lane] = getLaneByte(*lanes[lane], byte_idx, max_size);
            } else {
                // Empty lane - use default padding
                lane_bytes[lane] = default_padding;
            }
        }

        // Interleave this byte from all 8 lanes (produces 8 output bytes)
        interleave_byte_8way(&output[byte_idx * 8], lane_bytes);
    }

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

void SPITransposer::interleave_byte_8way(uint8_t* dest, const uint8_t lane_bytes[8]) {
    // Each output byte contains 1 bit from each of the 8 lanes
    // Output format for each byte: [L7_bitN L6_bitN L5_bitN L4_bitN L3_bitN L2_bitN L1_bitN L0_bitN]
    // where N goes from 7 (MSB) down to 0 (LSB)

    // Process each bit position (7 = MSB, 0 = LSB)
    for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
        uint8_t output_byte = 0;

        // Extract bit from each lane and pack into output byte
        for (int lane = 0; lane < 8; lane++) {
            uint8_t bit = (lane_bytes[lane] >> bit_pos) & 0x01;
            output_byte |= (bit << lane);
        }

        dest[7 - bit_pos] = output_byte;
    }
}

// ============================================================================
// 16-Way Transpose (Hex-SPI)
// ============================================================================

bool SPITransposer::transpose16(const fl::optional<LaneData> lanes[16],
                                fl::span<uint8_t> output,
                                const char** error) {
    // Validate output buffer size (must be divisible by 16)
    if (output.size() % 16 != 0) {
        if (error) {
            *error = "Output buffer size must be divisible by 16";
        }
        return false;
    }

    // Calculate max lane size from output buffer
    const size_t max_size = output.size() / 16;

    // Handle empty case
    if (max_size == 0) {
        if (error) {
            *error = nullptr;  // No error, just empty
        }
        return true;
    }

    // Determine default padding byte from first available lane
    uint8_t default_padding = 0x00;
    for (size_t i = 0; i < 16; i++) {
        if (lanes[i].has_value() && !lanes[i]->padding_frame.empty()) {
            default_padding = lanes[i]->padding_frame[0];
            break;
        }
    }

    // Process each input byte with optimized interleaving
    for (size_t byte_idx = 0; byte_idx < max_size; byte_idx++) {
        uint8_t lane_bytes[16];

        // Gather bytes from each lane (handles padding automatically)
        for (size_t lane = 0; lane < 16; lane++) {
            if (lanes[lane].has_value()) {
                lane_bytes[lane] = getLaneByte(*lanes[lane], byte_idx, max_size);
            } else {
                // Empty lane - use default padding
                lane_bytes[lane] = default_padding;
            }
        }

        // Interleave this byte from all 16 lanes (produces 16 output bytes)
        interleave_byte_16way(&output[byte_idx * 16], lane_bytes);
    }

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

void SPITransposer::interleave_byte_16way(uint8_t* dest, const uint8_t lane_bytes[16]) {
    // 16-way transpose: each input byte is split across 16 output bytes
    // Each output byte N contains bit N from all 16 input lanes
    // Output format: dest[N] = [L15_bitN L14_bitN ... L1_bitN L0_bitN]
    // where N is the bit position (0-7 for a byte with 8 bits)
    // We need 16 output bytes to handle 16 lanes, but each byte only has 8 bits

    // Actually, for 16 lanes with 8 bits each:
    // We output 16 bytes, where each byte contains 1 bit from each lane
    // Byte 0 contains bit 7 from all 16 lanes
    // Byte 1 contains bit 6 from all 16 lanes
    // ... and so on for all 8 bit positions
    // Plus 8 more bytes for lanes 8-15 with their own bits

    // This creates a 16-byte output per input set
    // For lanes 0-7 and 8-15, we need to split the output:
    // dest[0-7]: bits from lanes 0-7
    // dest[8-15]: bits from lanes 8-15

    // Process bit positions 7 down to 0
    for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
        // Output byte for this bit position across lanes 0-7
        uint8_t output_byte_lo = 0;
        // Output byte for this bit position across lanes 8-15
        uint8_t output_byte_hi = 0;

        // Extract bit from lanes 0-7
        for (int lane = 0; lane < 8; lane++) {
            uint8_t bit = (lane_bytes[lane] >> bit_pos) & 0x01;
            output_byte_lo |= (bit << lane);
        }

        // Extract bit from lanes 8-15
        for (int lane = 8; lane < 16; lane++) {
            uint8_t bit = (lane_bytes[lane] >> bit_pos) & 0x01;
            output_byte_hi |= (bit << (lane - 8));
        }

        dest[7 - bit_pos] = output_byte_lo;
        dest[15 - bit_pos] = output_byte_hi;
    }
}

// ============================================================================
// Common Helper Functions
// ============================================================================

uint8_t SPITransposer::getLaneByte(const LaneData& lane, size_t byte_idx, size_t max_size) {
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
