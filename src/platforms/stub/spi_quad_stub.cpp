/// @file spi_quad_stub.cpp
/// @brief Stub/Mock implementation of Quad-SPI for testing
///
/// This provides a mock SPIQuad implementation for unit tests.
/// It captures transmitted data for inspection without real hardware.

#include "platforms/stub/spi_quad_stub.h"

#ifdef FASTLED_TESTING

namespace fl {

// ============================================================================
// SPIQuadStub Implementation
// ============================================================================

SPIQuadStub::SPIQuadStub(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mInitialized(false)
    , mBusy(false)
    , mClockSpeed(20000000)
    , mTransmitCount(0) {
}

bool SPIQuadStub::begin(const SPIQuad::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        return false;  // Mismatch
    }

    mClockSpeed = config.clock_speed_hz;
    mInitialized = true;
    return true;
}

void SPIQuadStub::end() {
    mInitialized = false;
    mBusy = false;
    mLastBuffer.clear();
}

bool SPIQuadStub::transmitAsync(fl::span<const uint8_t> buffer) {
    if (!mInitialized) {
        return false;
    }

    if (buffer.empty()) {
        return true;  // Nothing to transmit
    }

    // Capture data for inspection
    mLastBuffer.clear();
    mLastBuffer.reserve(buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i) {
        mLastBuffer.push_back(buffer[i]);
    }

    mTransmitCount++;
    mBusy = true;

    return true;
}

bool SPIQuadStub::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused in mock
    mBusy = false;
    return true;  // Always succeeds instantly
}

bool SPIQuadStub::isBusy() const {
    return mBusy;
}

bool SPIQuadStub::isInitialized() const {
    return mInitialized;
}

int SPIQuadStub::getBusId() const {
    return mBusId;
}

const char* SPIQuadStub::getName() const {
    return mName;
}

const fl::vector<uint8_t>& SPIQuadStub::getLastTransmission() const {
    return mLastBuffer;
}

uint32_t SPIQuadStub::getTransmissionCount() const {
    return mTransmitCount;
}

uint32_t SPIQuadStub::getClockSpeed() const {
    return mClockSpeed;
}

bool SPIQuadStub::isTransmissionActive() const {
    return mBusy;
}

void SPIQuadStub::reset() {
    mLastBuffer.clear();
    mTransmitCount = 0;
    mBusy = false;
}

fl::vector<fl::vector<uint8_t>> SPIQuadStub::extractLanes(uint8_t num_lanes, size_t bytes_per_lane) const {
    fl::vector<fl::vector<uint8_t>> lanes(num_lanes);

    // Pre-allocate per-lane buffers
    for (uint8_t lane = 0; lane < num_lanes; ++lane) {
        lanes[lane].resize(bytes_per_lane);
    }

    // De-interleave: reverse the quad-SPI bit interleaving
    // For quad-SPI: each output byte contains 2 bits from each of 4 lanes
    // Byte format: [D7 C7 B7 A7 D6 C6 B6 A6] (2 bits per lane)

    size_t output_bytes = bytes_per_lane * 4;  // Interleaved size

    for (size_t out_idx = 0; out_idx < output_bytes && out_idx < mLastBuffer.size(); ++out_idx) {
        uint8_t interleaved_byte = mLastBuffer[out_idx];

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

// ============================================================================
// Factory Implementation
// ============================================================================

/// Stub factory override - returns mock instances for testing
/// Strong definition overrides weak default
fl::vector<SPIQuad*> SPIQuad::createInstances() {
    fl::vector<SPIQuad*> controllers;

    // Provide 2 mock SPI buses for testing
    static SPIQuadStub controller2(2, "MockSPI2");  // Bus 2 - static lifetime
    static SPIQuadStub controller3(3, "MockSPI3");  // Bus 3 - static lifetime

    controllers.push_back(&controller2);
    controllers.push_back(&controller3);

    return controllers;
}

}  // namespace fl

#endif  // FASTLED_TESTING
