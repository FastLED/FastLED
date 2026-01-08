/// @file spi_8_stub.cpp
/// @brief Stub/Mock implementation of 8-lane (Octal) SPI for testing
///
/// This provides a mock SpiHw8 implementation for unit tests.
/// It captures transmitted data for inspection without real hardware.

#include "platforms/stub/spi_8_stub.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "fl/log.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

// ============================================================================
// SpiHw8Stub Implementation
// ============================================================================

SpiHw8Stub::SpiHw8Stub(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mInitialized(false)
    , mBusy(false)
    , mClockSpeed(20000000)
    , mTransmitCount(0)
    , mCurrentBuffer()
    , mBufferAcquired(false) {
}

bool SpiHw8Stub::begin(const SpiHw8::Config& config) {
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

void SpiHw8Stub::end() {
    mInitialized = false;
    mBusy = false;
    mLastBuffer.clear();

    // Release current buffer
    mCurrentBuffer.reset();
    mBufferAcquired = false;
}

DMABuffer SpiHw8Stub::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mBusy) {
        waitComplete();
    }

    // For octal-lane SPI: num_lanes = 8
    constexpr size_t num_lanes = 8;
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

bool SpiHw8Stub::transmit(TransmitMode mode) {
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

bool SpiHw8Stub::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused in mock
    mBusy = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;

    return true;  // Always succeeds instantly
}

bool SpiHw8Stub::isBusy() const {
    return mBusy;
}

bool SpiHw8Stub::isInitialized() const {
    return mInitialized;
}

int SpiHw8Stub::getBusId() const {
    return mBusId;
}

const char* SpiHw8Stub::getName() const {
    return mName;
}

const fl::vector<uint8_t>& SpiHw8Stub::getLastTransmission() const {
    return mLastBuffer;
}

uint32_t SpiHw8Stub::getTransmissionCount() const {
    return mTransmitCount;
}

uint32_t SpiHw8Stub::getClockSpeed() const {
    return mClockSpeed;
}

bool SpiHw8Stub::isTransmissionActive() const {
    return mBusy;
}

void SpiHw8Stub::reset() {
    mLastBuffer.clear();
    mTransmitCount = 0;
    mBusy = false;
}

fl::vector<fl::vector<uint8_t>> SpiHw8Stub::extractLanes(uint8_t num_lanes, size_t bytes_per_lane) const {
    fl::vector<fl::vector<uint8_t>> lanes(num_lanes);

    // Pre-allocate per-lane buffers
    for (uint8_t lane = 0; lane < num_lanes; ++lane) {
        lanes[lane].resize(bytes_per_lane);
    }

    // De-interleave: reverse the 8-lane (octal) SPI bit interleaving
    // For octal-SPI: each output byte contains 1 bit from each of 8 lanes
    // Byte format: [H7 G7 F7 E7 D7 C7 B7 A7] (1 bit per lane)

    size_t output_bytes = bytes_per_lane * 8;  // Interleaved size

    for (size_t out_idx = 0; out_idx < output_bytes && out_idx < mLastBuffer.size(); ++out_idx) {
        uint8_t interleaved_byte = mLastBuffer[out_idx];

        // Which input byte does this correspond to?
        size_t in_byte_idx = out_idx / 8;
        size_t bit_idx = out_idx % 8;  // Which bit (0-7)

        if (in_byte_idx >= bytes_per_lane) break;

        // Extract 1 bit for each lane
        for (uint8_t lane = 0; lane < num_lanes && lane < 8; ++lane) {
            uint8_t bit = (interleaved_byte >> lane) & 1;
            lanes[lane][in_byte_idx] |= (bit << (7 - bit_idx));
        }
    }

    return lanes;
}

// ============================================================================
// Instance Registration
// ============================================================================

namespace {
// Singleton getters for mock controller instances (Meyer's Singleton pattern)
fl::shared_ptr<SpiHw8Stub>& getController2_Spi8() {
    static fl::shared_ptr<SpiHw8Stub> instance = fl::make_shared<SpiHw8Stub>(2, "MockOctal2");
    return instance;
}

fl::shared_ptr<SpiHw8Stub>& getController3_Spi8() {
    static fl::shared_ptr<SpiHw8Stub> instance = fl::make_shared<SpiHw8Stub>(3, "MockOctal3");
    return instance;
}
}  // anonymous namespace

}  // namespace fl

// Platform-specific initialization for stub SPI hardware
namespace fl {
namespace platform {

/// @brief Initialize stub SpiHw8 instances for testing
///
/// Called lazily on first access to SpiHw8::getAll().
/// Registers mock SpiHw8 controller instances for testing.
void initSpiHw8Instances() {
    FL_WARN("Registering SpiHw8 stub instances...");
    SpiHw8::registerInstance(getController2_Spi8());
    SpiHw8::registerInstance(getController3_Spi8());
    FL_WARN("SpiHw8 stub instances registered!");
}

}  // namespace platform
}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
