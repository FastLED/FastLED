/// @file spi_dual_stub.h
/// @brief Stub/Mock Dual-SPI interface for testing
///
/// This header exposes the SPIDualStub class for tests to access
/// test-specific inspection methods.

#pragma once

#include "platforms/shared/spi_dual.h"

#ifdef FASTLED_TESTING

namespace fl {

/// Mock Dual-SPI driver for testing without real hardware
/// Implements SPIDual interface with data capture for validation
class SPIDualStub : public SPIDual {
public:
    explicit SPIDualStub(int bus_id = -1, const char* name = "MockSPI");
    ~SPIDualStub() override = default;

    bool begin(const SPIDual::Config& config) override;
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

/// Cast SPIDual* to SPIDualStub* for test inspection
/// @param driver SPIDual pointer (must be from test environment)
/// @returns SPIDualStub pointer, or nullptr if cast fails
inline SPIDualStub* toStub(SPIDual* driver) {
    return static_cast<SPIDualStub*>(driver);
}

}  // namespace fl

#endif  // FASTLED_TESTING
