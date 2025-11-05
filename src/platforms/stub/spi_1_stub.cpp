/// @file spi_1_stub.cpp
/// @brief Stub/Mock Single-SPI implementation for testing

#ifdef FASTLED_TESTING

#include "spi_1_stub.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "fl/log.h"

namespace fl {

SpiHw1Stub::SpiHw1Stub(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mInitialized(false)
    , mClockSpeed(0)
    , mTransmitCount(0)
    , mCurrentBuffer()
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
    FL_LOG_SPI("SpiHw1Stub::end() called, mInitialized=" << (mInitialized ? "true" : "false"));
    if (!mInitialized) {
        FL_LOG_SPI("SpiHw1Stub::end() already ended, returning");
        return;  // Already ended - idempotent
    }

    FL_LOG_SPI("SpiHw1Stub::end() setting mInitialized=false");
    mInitialized = false;
    FL_LOG_SPI("SpiHw1Stub::end() clearing mLastBuffer");
    mLastBuffer.clear();

    FL_LOG_SPI("SpiHw1Stub::end() resetting mCurrentBuffer");
    // Release current buffer
    mCurrentBuffer.reset();
    FL_LOG_SPI("SpiHw1Stub::end() setting mBufferAcquired=false");
    mBufferAcquired = false;
    FL_LOG_SPI("SpiHw1Stub::end() complete");
}

DMABuffer SpiHw1Stub::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // For single-lane SPI: num_lanes = 1
    constexpr size_t num_lanes = 1;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Create a new DMABuffer with the specified size
    mCurrentBuffer = DMABuffer(total_size);

    if (!mCurrentBuffer.ok()) {
        return mCurrentBuffer;  // Return error
    }

    mBufferAcquired = true;

    // Return a copy of the buffer (shared_ptr will be shared)
    return mCurrentBuffer;
}

bool SpiHw1Stub::transmit(TransmitMode mode) {
    (void)mode;  // Unused in stub

    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    if (!mCurrentBuffer.ok() || mCurrentBuffer.data().size() == 0) {
        return true;  // Nothing to transmit
    }

    // Capture data for inspection (simulates blocking transmission)
    mLastBuffer.clear();
    fl::span<uint8_t> buffer_span = mCurrentBuffer.data();
    mLastBuffer.reserve(buffer_span.size());
    for (size_t i = 0; i < buffer_span.size(); ++i) {
        mLastBuffer.push_back(buffer_span[i]);
    }

    mTransmitCount++;

    // Note: In real implementation, this is blocking, so transmission is already complete
    return true;
}

bool SpiHw1Stub::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused in stub (transmission already complete)

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;

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
