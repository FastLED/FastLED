#include "quad_spi_transposer.h"
#include "fl/math_macros.h"

namespace fl {

QuadSPITransposer::QuadSPITransposer() : mMaxLaneSize(0), mNumLanes(0) {
    // Pre-allocate lanes array to maximum size (4 for quad-SPI)
    // This prevents reallocation when adding lanes
    mLanes.reserve(4);
}

void QuadSPITransposer::addLane(uint8_t lane_id, const fl::vector<uint8_t>& data,
                                 fl::span<const uint8_t> padding_frame) {
    if (lane_id >= 4) {
        return;  // Invalid lane ID
    }

    LaneInfo lane;
    lane.lane_id = lane_id;
    lane.padding_frame_ptr = padding_frame.data();
    lane.padding_frame_size = padding_frame.size();
    lane.actual_size = data.size();
    // Store pointer to external data instead of copying
    lane.data_ptr = data.data();
    lane.data_size = data.size();

    mLanes.push_back(lane);
    mNumLanes++;
    mMaxLaneSize = fl::fl_max(mMaxLaneSize, data.size());
}

const fl::vector<uint8_t>& QuadSPITransposer::transpose() {
    if (mLanes.empty()) {
        mInterleavedBuffer.clear();
        return mInterleavedBuffer;
    }

    // Interleave the lanes directly into member buffer
    interleaveLanes();

    return mInterleavedBuffer;
}

void QuadSPITransposer::reset() {
    mLanes.clear();
    mMaxLaneSize = 0;
    mNumLanes = 0;
    // Don't clear mInterleavedBuffer - keep capacity for reuse
}

uint8_t QuadSPITransposer::getLaneByte(const LaneInfo& lane, size_t byte_idx) const {
    // Calculate padding needed for this lane
    size_t padding_bytes = mMaxLaneSize - lane.data_size;

    // If we're in the padding region (prepended to beginning)
    if (byte_idx < padding_bytes) {
        // Return padding frame byte (repeating pattern)
        if (lane.padding_frame_size > 0 && lane.padding_frame_ptr != nullptr) {
            return lane.padding_frame_ptr[byte_idx % lane.padding_frame_size];
        }
        return 0x00;  // Fallback to zero
    }

    // We're in the data region
    size_t data_idx = byte_idx - padding_bytes;
    if (data_idx < lane.data_size) {
        return lane.data_ptr[data_idx];
    }

    // Should never reach here if mMaxLaneSize is correct
    return 0x00;
}

void QuadSPITransposer::interleave_byte_optimized(uint8_t* dest,
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

void QuadSPITransposer::interleaveLanes() {
    // Calculate output buffer size
    // Each input byte generates 4 output bytes (2 bits per lane × 4 lanes = 8 bits per output byte)
    size_t output_size = mMaxLaneSize * 4;

    // Resize only if needed (preserves capacity on subsequent calls)
    if (mInterleavedBuffer.size() != output_size) {
        mInterleavedBuffer.resize(output_size);
    }

    // Default padding byte for empty lane slots
    uint8_t default_padding = 0x00;
    if (mLanes.size() > 0 && mLanes[0].padding_frame_size > 0 && mLanes[0].padding_frame_ptr != nullptr) {
        default_padding = mLanes[0].padding_frame_ptr[0];
    }

    // Process each input byte with optimized interleaving
    for (size_t byte_idx = 0; byte_idx < mMaxLaneSize; byte_idx++) {
        uint8_t lane_bytes[4];

        // Gather bytes from each lane (handles padding automatically)
        for (size_t lane = 0; lane < 4; lane++) {
            if (lane < mLanes.size()) {
                lane_bytes[lane] = getLaneByte(mLanes[lane], byte_idx);
            } else {
                lane_bytes[lane] = default_padding;
            }
        }

        // Interleave this byte from all 4 lanes (produces 4 output bytes)
        // Uses optimized function for 2-3× performance improvement
        interleave_byte_optimized(&mInterleavedBuffer[byte_idx * 4],
                                  lane_bytes[0], lane_bytes[1],
                                  lane_bytes[2], lane_bytes[3]);
    }
}

}  // namespace fl
