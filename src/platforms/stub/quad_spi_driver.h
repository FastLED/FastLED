#pragma once

// Mock Quad-SPI driver for testing without real ESP32 hardware
// This simulates DMA transmission and provides inspection methods for validation

#include "fl/stl/vector.h"

namespace fl {

class MockQuadSPIDriver {
private:
    fl::vector<uint8_t> mLastDMABuffer;
    uint32_t mClockSpeed;
    uint32_t mTransmissionCount;
    bool mTransmissionActive;

public:
    MockQuadSPIDriver()
        : mClockSpeed(40000000)  // Default 40 MHz
        , mTransmissionCount(0)
        , mTransmissionActive(false) {
    }

    // Fake DMA transmission (just captures data)
    void transmitDMA(const uint8_t* buffer, size_t length) {
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
    const fl::vector<uint8_t>& getLastTransmission() const {
        return mLastDMABuffer;
    }

    uint32_t getTransmissionCount() const {
        return mTransmissionCount;
    }

    bool isTransmissionActive() const {
        return mTransmissionActive;
    }

    void setClockSpeed(uint32_t hz) {
        mClockSpeed = hz;
    }

    uint32_t getClockSpeed() const {
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
    fl::vector<fl::vector<uint8_t>> extractLanes(uint8_t num_lanes, size_t bytes_per_lane) const {
        fl::vector<fl::vector<uint8_t>> lanes(num_lanes);

        // Pre-allocate per-lane buffers
        for (uint8_t lane = 0; lane < num_lanes; ++lane) {
            lanes[lane].resize(bytes_per_lane);
        }

        // De-interleave: reverse the quad-SPI bit interleaving
        // For quad-SPI: each output byte contains 2 bits from each of 4 lanes
        // Byte format: [D7 C7 B7 A7 D6 C6 B6 A6] (2 bits per lane)

        size_t output_bytes = bytes_per_lane * 4;  // Interleaved size

        for (size_t out_idx = 0; out_idx < output_bytes && out_idx < mLastDMABuffer.size(); ++out_idx) {
            uint8_t interleaved_byte = mLastDMABuffer[out_idx];

            // Which input byte does this correspond to?
            size_t in_byte_idx = out_idx / 4;
            size_t nibble_idx = out_idx % 4;  // Which 2-bit chunk (0-3)

            if (in_byte_idx >= bytes_per_lane) break;

            // Extract 2 bits for each lane
            for (uint8_t lane = 0; lane < num_lanes && lane < 4; ++lane) {
                uint8_t bits = (interleaved_byte >> (lane * 2)) & 0b11;
                lanes[lane][in_byte_idx] |= (bits << ((3 - nibble_idx) * 2));
            }
        }

        return lanes;
    }

    // Timing simulation
    uint64_t estimateTransmissionTimeMicros(size_t buffer_size) const {
        // Calculate based on clock speed
        // Time = (bytes * 8 bits/byte) / (clock_hz) * 1,000,000 us/s
        return (buffer_size * 8 * 1000000ULL) / mClockSpeed;
    }
};

}  // namespace fl
