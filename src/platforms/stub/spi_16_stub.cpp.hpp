/// @file spi_16_stub.cpp
/// @brief Stub/Mock implementation of 16-lane (Hexadeca) SPI for testing
///
/// This provides a mock SpiHw16 implementation for unit tests.
/// It captures transmitted data for inspection without real hardware.

#include "platforms/stub/spi_16_stub.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "fl/log.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

// ============================================================================
// SpiHw16Stub Implementation
// ============================================================================

SpiHw16Stub::SpiHw16Stub(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mInitialized(false)
    , mBusy(false)
    , mClockSpeed(10000000)  // 10 MHz default (conservative for I2S)
    , mTransmitCount(0)
    , mCurrentBuffer()
    , mBufferAcquired(false) {
}

bool SpiHw16Stub::begin(const SpiHw16::Config& config) {
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

void SpiHw16Stub::end() {
    mInitialized = false;
    mBusy = false;
    mLastBuffer.clear();

    // Release current buffer
    mCurrentBuffer.reset();
    mBufferAcquired = false;
}

DMABuffer SpiHw16Stub::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mBusy) {
        waitComplete();
    }

    // For hexadeca-lane SPI: num_lanes = 16
    constexpr size_t num_lanes = 16;
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

bool SpiHw16Stub::transmit(TransmitMode mode) {
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

bool SpiHw16Stub::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused in mock
    mBusy = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;

    return true;  // Always succeeds instantly
}

bool SpiHw16Stub::isBusy() const {
    return mBusy;
}

bool SpiHw16Stub::isInitialized() const {
    return mInitialized;
}

int SpiHw16Stub::getBusId() const {
    return mBusId;
}

const char* SpiHw16Stub::getName() const {
    return mName;
}

const fl::vector<uint8_t>& SpiHw16Stub::getLastTransmission() const {
    return mLastBuffer;
}

uint32_t SpiHw16Stub::getTransmissionCount() const {
    return mTransmitCount;
}

uint32_t SpiHw16Stub::getClockSpeed() const {
    return mClockSpeed;
}

bool SpiHw16Stub::isTransmissionActive() const {
    return mBusy;
}

void SpiHw16Stub::reset() {
    mLastBuffer.clear();
    mTransmitCount = 0;
    mBusy = false;
}

fl::vector<fl::vector<uint8_t>> SpiHw16Stub::extractLanes(uint8_t num_lanes, size_t bytes_per_lane) const {
    fl::vector<fl::vector<uint8_t>> lanes(num_lanes);

    // Pre-allocate per-lane buffers
    for (uint8_t lane = 0; lane < num_lanes; ++lane) {
        lanes[lane].resize(bytes_per_lane);
    }

    // De-interleave: reverse the 16-lane (hexadeca) SPI bit interleaving
    // For hexadeca-SPI: each output byte contains 1 bit from each of 16 lanes
    // Since we have 16 lanes but only 8 bits per byte, we need 2 bytes per bit position
    // Byte format (alternating pairs):
    // Byte 0: [G7 F7 E7 D7 C7 B7 A7 ...] (lanes 0-7, bit 7)
    // Byte 1: [O7 N7 M7 L7 K7 J7 I7 H7] (lanes 8-15, bit 7)
    // Byte 2: [G6 F6 E6 D6 C6 B6 A6 ...] (lanes 0-7, bit 6)
    // Byte 3: [O6 N6 M6 L6 K6 J6 I6 H6] (lanes 8-15, bit 6)
    // ...and so on

    size_t output_bytes = bytes_per_lane * 16;  // Interleaved size

    for (size_t out_idx = 0; out_idx < output_bytes && out_idx < mLastBuffer.size(); ++out_idx) {
        uint8_t interleaved_byte = mLastBuffer[out_idx];

        // Which input byte and bit does this correspond to?
        size_t bit_group = out_idx / 2;         // Every 2 output bytes = 1 bit position
        size_t byte_half = out_idx % 2;         // 0 = lanes 0-7, 1 = lanes 8-15
        size_t in_byte_idx = bit_group / 8;     // Which input byte
        size_t bit_idx = 7 - (bit_group % 8);   // Which bit (7-0, MSB first)

        if (in_byte_idx >= bytes_per_lane) break;

        // Extract 1 bit for each lane (8 lanes per byte)
        uint8_t lane_offset = byte_half * 8;
        for (uint8_t lane = 0; lane < 8 && (lane_offset + lane) < num_lanes; ++lane) {
            uint8_t bit = (interleaved_byte >> lane) & 1;
            lanes[lane_offset + lane][in_byte_idx] |= (bit << bit_idx);
        }
    }

    return lanes;
}

// ============================================================================
// Instance Registration
// ============================================================================

namespace {
// Singleton getters for mock controller instances (Meyer's Singleton pattern)
fl::shared_ptr<SpiHw16Stub>& getController2_Spi16() {
    static fl::shared_ptr<SpiHw16Stub> instance = fl::make_shared<SpiHw16Stub>(2, "MockHexadeca2");
    return instance;
}

fl::shared_ptr<SpiHw16Stub>& getController3_Spi16() {
    static fl::shared_ptr<SpiHw16Stub> instance = fl::make_shared<SpiHw16Stub>(3, "MockHexadeca3");
    return instance;
}
}  // anonymous namespace

}  // namespace fl

// Platform-specific initialization for stub SPI hardware
namespace fl {
namespace platform {

/// @brief Initialize stub SpiHw16 instances for testing
///
/// Called lazily on first access to SpiHw16::getAll().
/// Registers mock SpiHw16 controller instances for testing.
void initSpiHw16Instances() {
    FL_WARN("Registering SpiHw16 stub instances...");
    SpiHw16::registerInstance(getController2_Spi16());
    SpiHw16::registerInstance(getController3_Spi16());
    FL_WARN("SpiHw16 stub instances registered!");
}

}  // namespace platform
}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
