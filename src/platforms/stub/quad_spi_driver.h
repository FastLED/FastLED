#pragma once

// Mock Quad-SPI driver for testing without real ESP32 hardware
// This simulates DMA transmission and provides inspection methods for validation

#include "fl/stl/vector.h"

namespace fl {

class MockQuadSPIDriver {
private:
    fl::vector<u8> mLastDMABuffer;
    u32 mClockSpeed;
    u32 mTransmissionCount;
    bool mTransmissionActive;

public:
    MockQuadSPIDriver()
        : mClockSpeed(40000000)  // Default 40 MHz
        , mTransmissionCount(0)
        , mTransmissionActive(false) {
    }

    // Fake DMA transmission (just captures data)
    void transmitDMA(const u8* buffer, size_t length) {
        mLastDMABuffer.clear();
        mLastDMABuffer.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            mLastDMABuffer.push_back(buffer[i]);
        }
        mTransmissionCount++;
        mTransmissionActive = true;
    }

    // Wait for transmission to complete (simulated)
    void waitComplete() {
        mTransmissionActive = false;
    }

    // Test inspection methods
    const fl::vector<u8>& getLastTransmission() const {
        return mLastDMABuffer;
    }

    u32 getTransmissionCount() const {
        return mTransmissionCount;
    }

    bool isTransmissionActive() const {
        return mTransmissionActive;
    }

    void setClockSpeed(u32 hz) {
        mClockSpeed = hz;
    }

    u32 getClockSpeed() const {
        return mClockSpeed;
    }

    // Clear history
    void reset() {
        mLastDMABuffer.clear();
        mTransmissionCount = 0;
        mTransmissionActive = false;
    }

    // Simulate de-interleaving to verify each lane's data
    // This reverses the bit-interleaving to extract per-lane data
    fl::vector<fl::vector<u8>> extractLanes(u8 num_lanes, size_t bytes_per_lane) const {
        fl::vector<fl::vector<u8>> lanes(num_lanes);

        // Pre-allocate per-lane buffers
        for (u8 lane = 0; lane < num_lanes; ++lane) {
            lanes[lane].resize(bytes_per_lane);
        }

        // De-interleave: reverse the quad-SPI bit interleaving
        // For quad-SPI: each output byte contains 2 bits from each of 4 lanes
        // Byte format: [D7 C7 B7 A7 D6 C6 B6 A6] (2 bits per lane)

        size_t output_bytes = bytes_per_lane * 4;  // Interleaved size

        for (size_t out_idx = 0; out_idx < output_bytes && out_idx < mLastDMABuffer.size(); ++out_idx) {
            u8 interleaved_byte = mLastDMABuffer[out_idx];

            // Which input byte does this correspond to?
            size_t in_byte_idx = out_idx / 4;
            size_t nibble_idx = out_idx % 4;  // Which 2-bit chunk (0-3)

            if (in_byte_idx >= bytes_per_lane) break;

            // Extract 2 bits for each lane
            for (u8 lane = 0; lane < num_lanes && lane < 4; ++lane) {
                u8 bits = (interleaved_byte >> (lane * 2)) & 0b11;
                lanes[lane][in_byte_idx] |= (bits << ((3 - nibble_idx) * 2));
            }
        }

        return lanes;
    }

    // Timing simulation
    u64 estimateTransmissionTimeMicros(size_t buffer_size) const {
        // Calculate based on clock speed
        // Time = (bytes * 8 bits/byte) / (clock_hz) * 1,000,000 us/s
        return (buffer_size * 8 * 1000000ULL) / mClockSpeed;
    }
};

}  // namespace fl
