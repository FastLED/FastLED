/// @file spi_single_stub.h
/// @brief Stub/Mock Single-SPI interface for testing
///
/// This header exposes the SPISingleStub class for tests to access
/// test-specific inspection methods.

#pragma once

#include "platforms/shared/spi_single.h"

#ifdef FASTLED_TESTING

namespace fl {

/// Mock Single-SPI driver for testing without real hardware
/// Implements SPISingle interface with data capture for validation
class SPISingleStub : public SPISingle {
public:
    explicit SPISingleStub(int bus_id = -1, const char* name = "MockSPI");
    ~SPISingleStub() override = default;

    bool begin(const SPISingle::Config& config) override;
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
    void reset();

private:
    int mBusId;
    const char* mName;
    bool mInitialized;
    uint32_t mClockSpeed;
    uint32_t mTransmitCount;
    fl::vector<uint8_t> mLastBuffer;
};

/// Cast SPISingle* to SPISingleStub* for test inspection
/// @param driver SPISingle pointer (must be from test environment)
/// @returns SPISingleStub pointer, or nullptr if cast fails
inline SPISingleStub* toStub(SPISingle* driver) {
    return static_cast<SPISingleStub*>(driver);
}

}  // namespace fl

#endif  // FASTLED_TESTING
