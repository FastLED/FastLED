/// @file spi_4_stub.h
/// @brief Stub/Mock Quad-SPI interface for testing
///
/// This header exposes the SPIQuadStub class for tests to access
/// test-specific inspection methods.

#pragma once

// IWYU pragma: private

#include "platforms/shared/spi_hw_4.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

/// Mock Quad-SPI driver for testing without real hardware
/// Implements SpiHw4 interface with data capture for validation
class SpiHw4Stub : public SpiHw4 {
public:
    explicit SpiHw4Stub(int bus_id = -1, const char* name = "MockSPI");
    ~SpiHw4Stub() override = default;

    bool begin(const SpiHw4::Config& config) override;
    void end() override;
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override;
    bool waitComplete(u32 timeout_ms = fl::numeric_limits<u32>::max()) override;
    bool isBusy() const override;
    bool isInitialized() const override;
    int getBusId() const override;
    const char* getName() const override;

    // Test inspection methods
    const fl::vector<u8>& getLastTransmission() const;
    u32 getTransmissionCount() const;
    u32 getClockSpeed() const;
    bool isTransmissionActive() const;
    void reset();

    // De-interleave transmitted data to extract per-lane data (for testing)
    fl::vector<fl::vector<u8>> extractLanes(u8 num_lanes, size_t bytes_per_lane) const;

private:
    int mBusId;
    const char* mName;
    bool mInitialized;
    bool mBusy;
    u32 mClockSpeed;
    u32 mTransmitCount;
    fl::vector<u8> mLastBuffer;

    // DMA buffer management
    DMABuffer mCurrentBuffer;            // Current active buffer
    bool mBufferAcquired;
};

/// Cast SpiHw4* to SpiHw4Stub* for test inspection
/// @param driver SpiHw4 pointer (must be from test environment)
/// @returns SpiHw4Stub pointer, or nullptr if cast fails
inline SpiHw4Stub* toStub(SpiHw4* driver) {
    return static_cast<SpiHw4Stub*>(driver);
}

}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
