/// @file spi_2_stub.h
/// @brief Stub/Mock Dual-SPI interface for testing
///
/// This header exposes the SPIDualStub class for tests to access
/// test-specific inspection methods.

#pragma once

#include "platforms/shared/spi_hw_2.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

/// Mock Dual-SPI driver for testing without real hardware
/// Implements SpiHw2 interface with data capture for validation
class SpiHw2Stub : public SpiHw2 {
public:
    explicit SpiHw2Stub(int bus_id = -1, const char* name = "MockSPI");
    ~SpiHw2Stub() override = default;

    bool begin(const SpiHw2::Config& config) override;
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

/// Cast SpiHw2* to SpiHw2Stub* for test inspection
/// @param driver SpiHw2 pointer (must be from test environment)
/// @returns SpiHw2Stub pointer, or nullptr if cast fails
inline SpiHw2Stub* toStub(SpiHw2* driver) {
    return static_cast<SpiHw2Stub*>(driver);
}

}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
