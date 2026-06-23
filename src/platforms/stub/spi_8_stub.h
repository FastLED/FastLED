/// @file spi_8_stub.h
/// @brief Stub/Mock 8-lane (Octal) SPI interface for testing
///
/// This header exposes the SPIOctalStub class for tests to access
/// test-specific inspection methods.

#pragma once

// IWYU pragma: private

#include "platforms/shared/spi_hw_8.h"
#include "fl/stl/noexcept.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

/// Mock 8-lane (Octal) SPI driver for testing without real hardware
/// Implements SpiHw8 interface with data capture for validation
class SpiHw8Stub : public SpiHw8 {
public:
    explicit SpiHw8Stub(int bus_id = -1, const char* name = "MockOctalSPI") FL_NOEXCEPT;
    ~SpiHw8Stub() override = default;

    bool begin(const SpiHw8::Config& config) FL_NOEXCEPT override;
    void end() FL_NOEXCEPT override;
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) FL_NOEXCEPT override;
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) FL_NOEXCEPT override;
    bool waitComplete(u32 timeout_ms = fl::numeric_limits<u32>::max()) FL_NOEXCEPT override;
    bool isBusy() const FL_NOEXCEPT override;
    bool isInitialized() const FL_NOEXCEPT override;
    int getBusId() const FL_NOEXCEPT override;
    const char* getName() const FL_NOEXCEPT override;

    // Test inspection methods
    const fl::vector<u8>& getLastTransmission() const FL_NOEXCEPT;
    u32 getTransmissionCount() const FL_NOEXCEPT;
    u32 getClockSpeed() const FL_NOEXCEPT;
    bool isTransmissionActive() const FL_NOEXCEPT;
    void reset() FL_NOEXCEPT;

    // De-interleave transmitted data to extract per-lane data (for testing)
    fl::vector<fl::vector<u8>> extractLanes(u8 num_lanes, size_t bytes_per_lane) const FL_NOEXCEPT;

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

/// Cast SpiHw8* to SpiHw8Stub* for test inspection
/// @param driver SpiHw8 pointer (must be from test environment)
/// @returns SpiHw8Stub pointer, or nullptr if cast fails
inline SpiHw8Stub* toStub(SpiHw8* driver) FL_NOEXCEPT {
    return static_cast<SpiHw8Stub*>(driver);
}

}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
