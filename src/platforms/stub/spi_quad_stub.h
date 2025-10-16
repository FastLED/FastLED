/// @file spi_quad_stub.h
/// @brief Stub/Mock Quad-SPI interface for testing
///
/// This header exposes the SPIQuadStub class for tests to access
/// test-specific inspection methods.

#pragma once

#include "platforms/shared/spi_hw_4.h"

#ifdef FASTLED_TESTING

namespace fl {

/// Mock Quad-SPI driver for testing without real hardware
/// Implements SpiHw4 interface with data capture for validation
class SPIQuadStub : public SpiHw4 {
public:
    explicit SPIQuadStub(int bus_id = -1, const char* name = "MockSPI");
    ~SPIQuadStub() override = default;

    bool begin(const SpiHw4::Config& config) override;
    void end() override;
    bool transmitAsync(fl::span<const uint8_t> buffer) override;
    bool waitComplete(uint32_t timeout_ms = UINT32_MAX) override;
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
};

/// Cast SpiHw4* to SPIQuadStub* for test inspection
/// @param driver SpiHw4 pointer (must be from test environment)
/// @returns SPIQuadStub pointer, or nullptr if cast fails
/// @note Also works with SPIQuad* (deprecated alias) due to type aliasing
inline SPIQuadStub* toStub(SpiHw4* driver) {
    return static_cast<SPIQuadStub*>(driver);
}

}  // namespace fl

#endif  // FASTLED_TESTING
