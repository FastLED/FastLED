/// @file transposition.cpp
/// @brief Implementation of bit transposition functions

#include "transposition.h"
#include "fl/math_macros.h"
#include "fl/stl/vector.h"

namespace fl {

// ============================================================================
// Core 8x1 Bit Transpose Implementation
// ============================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

void FL_OPTIMIZE_FUNCTION FL_IRAM transpose8x1_noinline(unsigned char *A, unsigned char *B) {
    uint32_t x, y, t;

    // Load the array and pack it into x and y.
    y = *(unsigned int*)(A);
    x = *(unsigned int*)(A+4);

    // pre-transform x
    t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
    t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);

    // pre-transform y
    t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
    t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

    // final transform
    t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
    y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
    x = t;

    *((uint32_t*)B) = y;
    *((uint32_t*)(B+4)) = x;
}

#pragma GCC diagnostic pop

// ============================================================================
// SPI Multi-Lane Transposer Implementation
// ============================================================================

// ----------------------------------------------------------------------------
// 2-Way Transpose (Dual-SPI)
// ----------------------------------------------------------------------------

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

    // Gather all bytes from each lane into temporary buffers
    fl::vector<uint8_t> lane0_buffer(max_size);
    fl::vector<uint8_t> lane1_buffer(max_size);

    for (size_t byte_idx = 0; byte_idx < max_size; byte_idx++) {
        lane0_buffer[byte_idx] = lanes[0]->has_value() ?
            getLaneByte(**lanes[0], byte_idx, max_size) : default_padding;
        lane1_buffer[byte_idx] = lanes[1]->has_value() ?
            getLaneByte(**lanes[1], byte_idx, max_size) : default_padding;
    }

    // Perform transposition using ISR-safe primitive
    transpose_2lane_inline(lane0_buffer.data(), lane1_buffer.data(), output.data(), max_size);

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

// interleave_byte_2way removed - now uses transpose_2lane_inline() primitive

// ----------------------------------------------------------------------------
// 4-Way Transpose (Quad-SPI)
// ----------------------------------------------------------------------------

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

    // Gather all bytes from each lane into temporary buffers
    fl::vector<uint8_t> lane_buffers[4];
    const uint8_t* lane_ptrs[4];

    for (size_t lane = 0; lane < 4; lane++) {
        lane_buffers[lane].resize(max_size);
        lane_ptrs[lane] = lane_buffers[lane].data();

        for (size_t byte_idx = 0; byte_idx < max_size; byte_idx++) {
            lane_buffers[lane][byte_idx] = lanes[lane]->has_value() ?
                getLaneByte(**lanes[lane], byte_idx, max_size) : default_padding;
        }
    }

    // Perform transposition using ISR-safe primitive
    transpose_4lane_inline(lane_ptrs, output.data(), max_size);

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

// interleave_byte_4way removed - now uses transpose_4lane_inline() primitive

// ----------------------------------------------------------------------------
// 8-Way Transpose (Octal-SPI)
// ----------------------------------------------------------------------------

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

    // Gather all bytes from each lane into temporary buffers
    fl::vector<uint8_t> lane_buffers[8];
    const uint8_t* lane_ptrs[8];

    for (size_t lane = 0; lane < 8; lane++) {
        lane_buffers[lane].resize(max_size);
        lane_ptrs[lane] = lane_buffers[lane].data();

        for (size_t byte_idx = 0; byte_idx < max_size; byte_idx++) {
            lane_buffers[lane][byte_idx] = lanes[lane].has_value() ?
                getLaneByte(*lanes[lane], byte_idx, max_size) : default_padding;
        }
    }

    // Perform transposition using ISR-safe primitive
    transpose_8lane_inline(lane_ptrs, output.data(), max_size);

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

// interleave_byte_8way removed - now uses transpose_8lane_inline() primitive

// ----------------------------------------------------------------------------
// 16-Way Transpose (Hex-SPI)
// ----------------------------------------------------------------------------

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

    // Gather all bytes from each lane into temporary buffers
    fl::vector<uint8_t> lane_buffers[16];
    const uint8_t* lane_ptrs[16];

    for (size_t lane = 0; lane < 16; lane++) {
        lane_buffers[lane].resize(max_size);
        lane_ptrs[lane] = lane_buffers[lane].data();

        for (size_t byte_idx = 0; byte_idx < max_size; byte_idx++) {
            lane_buffers[lane][byte_idx] = lanes[lane].has_value() ?
                getLaneByte(*lanes[lane], byte_idx, max_size) : default_padding;
        }
    }

    // Perform transposition using ISR-safe primitive
    transpose_16lane_inline(lane_ptrs, output.data(), max_size);

    if (error) {
        *error = nullptr;  // Success, no error
    }
    return true;
}

// interleave_byte_16way removed - now uses transpose_16lane_inline() primitive

// ----------------------------------------------------------------------------
// Common Helper Functions
// ----------------------------------------------------------------------------

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
