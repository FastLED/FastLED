/// @file spi_4_stub.h
/// @brief Stub/Mock Quad-SPI interface for testing
///
/// This header exposes the SPIQuadStub class for tests to access
/// test-specific inspection methods.

#pragma once

// IWYU pragma: private

#include "platforms/shared/spi_hw_4.h"
#include "fl/stl/noexcept.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

/// Mock Quad-SPI driver for testing without real hardware
/// Implements SpiHw4 interface with data capture for validation
class SpiHw4Stub : public SpiHw4 {
public:
    explicit SpiHw4Stub(int bus_id = -1, const char* name = "MockSPI") FL_NO_EXCEPT;
    ~SpiHw4Stub() override = default;

    bool begin(const SpiHw4::Config& config) FL_NO_EXCEPT override;
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
    bool isTransmissionActive() const FL_NO_EXCEPT;
    void reset() FL_NO_EXCEPT;

    // De-interleave transmitted data to extract per-lane data (for testing)
    fl::vector<fl::vector<u8>> extractLanes(u8 num_lanes, size_t bytes_per_lane) const FL_NO_EXCEPT;

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
inline SpiHw4Stub* toStub(SpiHw4* driver) FL_NO_EXCEPT {
    return static_cast<SpiHw4Stub*>(driver);
}

}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
