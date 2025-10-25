/// @file spi_dual_stub.cpp
/// @brief Stub/Mock Dual-SPI implementation for testing

#ifdef FASTLED_TESTING

#include "spi_dual_stub.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

SpiHw2Stub::SpiHw2Stub(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mInitialized(false)
    , mBusy(false)
    , mClockSpeed(0)
    , mTransmitCount(0)
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false) {
}

bool SpiHw2Stub::begin(const SpiHw2::Config& config) {
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

void SpiHw2Stub::end() {
    mInitialized = false;
    mBusy = false;
    mLastBuffer.clear();

    // Free DMA buffer
    mDMABuffer.clear();
    mMaxBytesPerLane = 0;
    mCurrentTotalSize = 0;
    mBufferAcquired = false;
}

DMABuffer SpiHw2Stub::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return SPIError::NOT_INITIALIZED;
    }

    // Auto-wait if previous transmission still active
    if (mBusy) {
        waitComplete();
    }

    // For dual-lane SPI: num_lanes = 2
    constexpr size_t num_lanes = 2;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Reallocate buffer only if we need more capacity
    if (bytes_per_lane > mMaxBytesPerLane) {
        mDMABuffer.clear();
        mDMABuffer.resize(total_size);
        mMaxBytesPerLane = bytes_per_lane;
    }

    mBufferAcquired = true;
    mCurrentTotalSize = total_size;

    // Return span of current size (not max allocated size)
    return fl::span<uint8_t>(mDMABuffer.data(), total_size);
}

bool SpiHw2Stub::transmit(TransmitMode mode) {
    (void)mode;  // Unused in stub

    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Capture data for inspection
    mLastBuffer.clear();
    mLastBuffer.reserve(mCurrentTotalSize);
    for (size_t i = 0; i < mCurrentTotalSize; ++i) {
        mLastBuffer.push_back(mDMABuffer[i]);
    }

    mTransmitCount++;
    mBusy = true;

    return true;
}

bool SpiHw2Stub::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused in stub
    mBusy = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SpiHw2Stub::isBusy() const {
    return mBusy;
}

bool SpiHw2Stub::isInitialized() const {
    return mInitialized;
}

int SpiHw2Stub::getBusId() const {
    return mBusId;
}

const char* SpiHw2Stub::getName() const {
    return mName;
}

const fl::vector<uint8_t>& SpiHw2Stub::getLastTransmission() const {
    return mLastBuffer;
}

uint32_t SpiHw2Stub::getTransmissionCount() const {
    return mTransmitCount;
}

uint32_t SpiHw2Stub::getClockSpeed() const {
    return mClockSpeed;
}

bool SpiHw2Stub::isTransmissionActive() const {
    return mBusy;
}

void SpiHw2Stub::reset() {
    mLastBuffer.clear();
    mTransmitCount = 0;
    mBusy = false;
}

fl::vector<fl::vector<uint8_t>> SpiHw2Stub::extractLanes(uint8_t num_lanes, size_t bytes_per_lane) const {
    fl::vector<fl::vector<uint8_t>> lanes(num_lanes);

    if (num_lanes != 2) {
        return lanes;  // Only support 2 lanes for dual-SPI
    }

    // Initialize lane vectors
    for (uint8_t i = 0; i < num_lanes; i++) {
        lanes[i].resize(bytes_per_lane);
    }

    // De-interleave: reverse the interleaving process
    // Each 2 input bytes produce 1 byte per lane
    for (size_t byte_idx = 0; byte_idx < bytes_per_lane; byte_idx++) {
        size_t input_offset = byte_idx * 2;
        if (input_offset + 1 >= mLastBuffer.size()) {
            break;  // Incomplete data
        }

        // Extract bit pairs from interleaved data
        uint8_t byte0 = mLastBuffer[input_offset];     // Bit positions 7,6,5,4
        uint8_t byte1 = mLastBuffer[input_offset + 1];  // Bit positions 3,2,1,0

        // Reconstruct lane bytes by extracting individual bits
        // Each pair has: lane0 bit at position 0, lane1 bit at position 1
        uint8_t lane0_byte = 0;
        uint8_t lane1_byte = 0;

        // Extract from byte0 (bits 7,6,5,4)
        lane0_byte |= ((byte0 >> 0) & 0x01) << 7;  // bit 7
        lane1_byte |= ((byte0 >> 1) & 0x01) << 7;
        lane0_byte |= ((byte0 >> 2) & 0x01) << 6;  // bit 6
        lane1_byte |= ((byte0 >> 3) & 0x01) << 6;
        lane0_byte |= ((byte0 >> 4) & 0x01) << 5;  // bit 5
        lane1_byte |= ((byte0 >> 5) & 0x01) << 5;
        lane0_byte |= ((byte0 >> 6) & 0x01) << 4;  // bit 4
        lane1_byte |= ((byte0 >> 7) & 0x01) << 4;

        // Extract from byte1 (bits 3,2,1,0)
        lane0_byte |= ((byte1 >> 2) & 0x01) << 3;  // bit 3
        lane1_byte |= ((byte1 >> 3) & 0x01) << 3;
        lane0_byte |= ((byte1 >> 0) & 0x01) << 2;  // bit 2
        lane1_byte |= ((byte1 >> 1) & 0x01) << 2;
        lane0_byte |= ((byte1 >> 4) & 0x01) << 1;  // bit 1
        lane1_byte |= ((byte1 >> 5) & 0x01) << 1;
        lane0_byte |= ((byte1 >> 6) & 0x01) << 0;  // bit 0
        lane1_byte |= ((byte1 >> 7) & 0x01) << 0;

        lanes[0][byte_idx] = lane0_byte;
        lanes[1][byte_idx] = lane1_byte;
    }

    return lanes;
}

// ============================================================================
// Factory Implementation (Weak Linkage for Testing)
// ============================================================================

fl::vector<SpiHw2*> SpiHw2::createInstances() {
    fl::vector<SpiHw2*> controllers;

    // Create two mock controllers for testing
    static SpiHw2Stub controller0(0, "MockDual0");
    static SpiHw2Stub controller1(1, "MockDual1");

    controllers.push_back(&controller0);
    controllers.push_back(&controller1);

    return controllers;
}

}  // namespace fl

#endif  // FASTLED_TESTING
