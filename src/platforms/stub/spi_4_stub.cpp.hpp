/// @file spi_4_stub.cpp
/// @brief Stub/Mock implementation of Quad-SPI for testing
///
/// This provides a mock SPIQuad implementation for unit tests.
/// It captures transmitted data for inspection without real hardware.

#include "platforms/stub/spi_4_stub.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "fl/log.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

// ============================================================================
// SpiHw4Stub Implementation
// ============================================================================

SpiHw4Stub::SpiHw4Stub(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mInitialized(false)
    , mBusy(false)
    , mClockSpeed(20000000)
    , mTransmitCount(0)
    , mCurrentBuffer()
    , mBufferAcquired(false) {
}

bool SpiHw4Stub::begin(const SpiHw4::Config& config) {
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

void SpiHw4Stub::end() {
    mInitialized = false;
    mBusy = false;
    mLastBuffer.clear();

    // Release current buffer
    mCurrentBuffer.reset();
    mBufferAcquired = false;
}

DMABuffer SpiHw4Stub::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mBusy) {
        waitComplete();
    }

    // For quad-lane SPI: num_lanes = 4
    constexpr size_t num_lanes = 4;
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

bool SpiHw4Stub::transmit(TransmitMode mode) {
    (void)mode;  // Unused in stub

    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    if (!mCurrentBuffer.ok() || mCurrentBuffer.data().size() == 0) {
        return true;  // Nothing to transmit
    }

    // Capture data for inspection
    mLastBuffer.clear();
    fl::span<uint8_t> buffer_span = mCurrentBuffer.data();
    mLastBuffer.reserve(buffer_span.size());
    for (size_t i = 0; i < buffer_span.size(); ++i) {
        mLastBuffer.push_back(buffer_span[i]);
    }

    mTransmitCount++;
    mBusy = true;

    return true;
}

bool SpiHw4Stub::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused in mock
    mBusy = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;

    return true;  // Always succeeds instantly
}

bool SpiHw4Stub::isBusy() const {
    return mBusy;
}

bool SpiHw4Stub::isInitialized() const {
    return mInitialized;
}

int SpiHw4Stub::getBusId() const {
    return mBusId;
}

const char* SpiHw4Stub::getName() const {
    return mName;
}

const fl::vector<uint8_t>& SpiHw4Stub::getLastTransmission() const {
    return mLastBuffer;
}

uint32_t SpiHw4Stub::getTransmissionCount() const {
    return mTransmitCount;
}

uint32_t SpiHw4Stub::getClockSpeed() const {
    return mClockSpeed;
}

bool SpiHw4Stub::isTransmissionActive() const {
    return mBusy;
}

void SpiHw4Stub::reset() {
    mLastBuffer.clear();
    mTransmitCount = 0;
    mBusy = false;
}

fl::vector<fl::vector<uint8_t>> SpiHw4Stub::extractLanes(uint8_t num_lanes, size_t bytes_per_lane) const {
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
            uint8_t bits = (interleaved_byte >> (lane * 2)) & 3;
            lanes[lane][in_byte_idx] |= (bits << ((3 - nibble_idx) * 2));
        }
    }

    return lanes;
}

// ============================================================================
// Instance Registration
// ============================================================================

namespace {
// Singleton getters for mock controller instances (Meyer's Singleton pattern)
fl::shared_ptr<SpiHw4Stub>& getController2_Spi4() {
    static fl::shared_ptr<SpiHw4Stub> instance = fl::make_shared<SpiHw4Stub>(2, "MockQuad2");
    return instance;
}

fl::shared_ptr<SpiHw4Stub>& getController3_Spi4() {
    static fl::shared_ptr<SpiHw4Stub> instance = fl::make_shared<SpiHw4Stub>(3, "MockQuad3");
    return instance;
}
}  // anonymous namespace

}  // namespace fl

// Platform-specific initialization for stub SPI hardware
namespace fl {
namespace platform {

/// @brief Initialize stub SpiHw4 instances for testing
///
/// Called lazily on first access to SpiHw4::getAll().
/// Registers mock SpiHw4 controller instances for testing.
void initSpiHw4Instances() {
    FL_WARN("Registering SpiHw4 stub instances...");
    SpiHw4::registerInstance(getController2_Spi4());
    SpiHw4::registerInstance(getController3_Spi4());
    FL_WARN("SpiHw4 stub instances registered!");
}

}  // namespace platform
}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
