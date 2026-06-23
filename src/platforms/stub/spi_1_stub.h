/// @file spi_1_stub.h
/// @brief Stub/Mock Single-SPI interface for testing
///
/// This header exposes the SPISingleStub class for tests to access
/// test-specific inspection methods.

#pragma once

// IWYU pragma: private

#include "platforms/shared/spi_hw_1.h"
#include "fl/stl/noexcept.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

/// Mock Single-SPI driver for testing without real hardware
/// Implements SpiHw1 interface with data capture for validation
class SpiHw1Stub : public SpiHw1 {
public:
    explicit SpiHw1Stub(int bus_id = -1, const char* name = "MockSPI") FL_NO_EXCEPT;
    ~SpiHw1Stub() override = default;

    bool begin(const SpiHw1::Config& config) FL_NO_EXCEPT override;
    void end() FL_NO_EXCEPT override;
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) FL_NO_EXCEPT override;
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) FL_NO_EXCEPT override;
    bool waitComplete(u32 timeout_ms = fl::numeric_limits<u32>::max()) FL_NO_EXCEPT override;
    bool isBusy() const FL_NO_EXCEPT override;
    bool isInitialized() const FL_NO_EXCEPT override;
    int getBusId() const FL_NO_EXCEPT override;
    const char* getName() const FL_NO_EXCEPT override;

    // Test inspection methods
    const fl::vector<u8>& getLastTransmission() const FL_NO_EXCEPT;
    u32 getTransmissionCount() const FL_NO_EXCEPT;
    u32 getClockSpeed() const FL_NO_EXCEPT;
    void reset() FL_NO_EXCEPT;

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
inline SpiHw1Stub* toStub(SpiHw1* driver) FL_NO_EXCEPT {
    return static_cast<SpiHw1Stub*>(driver);
}

}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
