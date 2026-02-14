/// @file spi_1_stub.h
/// @brief Stub/Mock Single-SPI interface for testing
///
/// This header exposes the SPISingleStub class for tests to access
/// test-specific inspection methods.

#pragma once

// IWYU pragma: private

#include "platforms/shared/spi_hw_1.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

/// Mock Single-SPI driver for testing without real hardware
/// Implements SpiHw1 interface with data capture for validation
class SpiHw1Stub : public SpiHw1 {
public:
    explicit SpiHw1Stub(int bus_id = -1, const char* name = "MockSPI");
    ~SpiHw1Stub() override = default;

    bool begin(const SpiHw1::Config& config) override;
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
    void reset();

private:
    int mBusId;
    const char* mName;
    bool mInitialized;
    u32 mClockSpeed;
    u32 mTransmitCount;
    fl::vector<u8> mLastBuffer;

    // DMA buffer management
    DMABuffer mCurrentBuffer;            // Current active buffer
    bool mBufferAcquired;
};

/// Cast SpiHw1* to SpiHw1Stub* for test inspection
/// @param driver SpiHw1 pointer (must be from test environment)
/// @returns SpiHw1Stub pointer, or nullptr if cast fails
inline SpiHw1Stub* toStub(SpiHw1* driver) {
    return static_cast<SpiHw1Stub*>(driver);
}

}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
