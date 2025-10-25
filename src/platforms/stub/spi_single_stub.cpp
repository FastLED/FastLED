/// @file spi_single_stub.cpp
/// @brief Stub/Mock Single-SPI implementation for testing

#ifdef FASTLED_TESTING

#include "spi_single_stub.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

SpiHw1Stub::SpiHw1Stub(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mInitialized(false)
    , mClockSpeed(0)
    , mTransmitCount(0)
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false) {
}

bool SpiHw1Stub::begin(const SpiHw1::Config& config) {
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

void SpiHw1Stub::end() {
    mInitialized = false;
    mLastBuffer.clear();

    // Free DMA buffer
    mDMABuffer.clear();
    mMaxBytesPerLane = 0;
    mCurrentTotalSize = 0;
    mBufferAcquired = false;
}

DMABuffer SpiHw1Stub::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return SPIError::NOT_INITIALIZED;
    }

    // For single-lane SPI: num_lanes = 1
    constexpr size_t num_lanes = 1;
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

bool SpiHw1Stub::transmit(TransmitMode mode) {
    (void)mode;  // Unused in stub

    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Capture data for inspection (simulates blocking transmission)
    mLastBuffer.clear();
    mLastBuffer.reserve(mCurrentTotalSize);
    for (size_t i = 0; i < mCurrentTotalSize; ++i) {
        mLastBuffer.push_back(mDMABuffer[i]);
    }

    mTransmitCount++;

    // Note: In real implementation, this is blocking, so transmission is already complete
    return true;
}

bool SpiHw1Stub::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused in stub (transmission already complete)

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SpiHw1Stub::isBusy() const {
    // Never busy since transmission is blocking
    return false;
}

bool SpiHw1Stub::isInitialized() const {
    return mInitialized;
}

int SpiHw1Stub::getBusId() const {
    return mBusId;
}

const char* SpiHw1Stub::getName() const {
    return mName;
}

const fl::vector<uint8_t>& SpiHw1Stub::getLastTransmission() const {
    return mLastBuffer;
}

uint32_t SpiHw1Stub::getTransmissionCount() const {
    return mTransmitCount;
}

uint32_t SpiHw1Stub::getClockSpeed() const {
    return mClockSpeed;
}

void SpiHw1Stub::reset() {
    mLastBuffer.clear();
    mTransmitCount = 0;
}

// ============================================================================
// Factory Implementation (Weak Linkage for Testing)
// ============================================================================

fl::vector<SpiHw1*> SpiHw1::createInstances() {
    fl::vector<SpiHw1*> controllers;

    // Create two mock controllers for testing
    static SpiHw1Stub controller0(0, "MockSingle0");
    static SpiHw1Stub controller1(1, "MockSingle1");

    controllers.push_back(&controller0);
    controllers.push_back(&controller1);

    return controllers;
}

}  // namespace fl

#endif  // FASTLED_TESTING
