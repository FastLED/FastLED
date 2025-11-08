/// @file spi_16_stub.h
/// @brief Stub/Mock 16-lane (Hexadeca) SPI interface for testing
///
/// This header exposes the SpiHw16Stub class for tests to access
/// test-specific inspection methods.

#pragma once

#include "platforms/shared/spi_hw_16.h"

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {

/// Mock 16-lane (Hexadeca) SPI driver for testing without real hardware
/// Implements SpiHw16 interface with data capture for validation
class SpiHw16Stub : public SpiHw16 {
public:
    explicit SpiHw16Stub(int bus_id = -1, const char* name = "MockHexadecaSPI");
    ~SpiHw16Stub() override = default;

    bool begin(const SpiHw16::Config& config) override;
    void end() override;
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override;
    bool waitComplete(uint32_t timeout_ms = fl::numeric_limits<uint32_t>::max()) override;
    bool isBusy() const override;
    bool isInitialized() const override;
    int getBusId() const override;
    const char* getName() const override;

    // Test inspection methods
    const fl::vector<uint8_t>& getLastTransmission() const;
    uint32_t getTransmissionCount() const;
    uint32_t getClockSpeed() const;
    bool isTransmissionActive() const;
    void reset();

    // De-interleave transmitted data to extract per-lane data (for testing)
    fl::vector<fl::vector<uint8_t>> extractLanes(uint8_t num_lanes, size_t bytes_per_lane) const;

private:
    int mBusId;
    const char* mName;
    bool mInitialized;
    bool mBusy;
    uint32_t mClockSpeed;
    uint32_t mTransmitCount;
    fl::vector<uint8_t> mLastBuffer;

    // DMA buffer management
    DMABuffer mCurrentBuffer;            // Current active buffer
    bool mBufferAcquired;
};

/// Cast SpiHw16* to SpiHw16Stub* for test inspection
/// @param driver SpiHw16 pointer (must be from test environment)
/// @returns SpiHw16Stub pointer, or nullptr if cast fails
inline SpiHw16Stub* toStub(SpiHw16* driver) {
    return static_cast<SpiHw16Stub*>(driver);
}

}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
