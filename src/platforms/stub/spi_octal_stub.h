/// @file spi_octal_stub.h
/// @brief Stub/Mock 8-lane (Octal) SPI interface for testing
///
/// This header exposes the SPIOctalStub class for tests to access
/// test-specific inspection methods.

#pragma once

#include "platforms/shared/spi_hw_8.h"

#ifdef FASTLED_TESTING

namespace fl {

/// Mock 8-lane (Octal) SPI driver for testing without real hardware
/// Implements SpiHw8 interface with data capture for validation
class SpiHw8Stub : public SpiHw8 {
public:
    explicit SpiHw8Stub(int bus_id = -1, const char* name = "MockOctalSPI");
    ~SpiHw8Stub() override = default;

    bool begin(const SpiHw8::Config& config) override;
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

/// Cast SpiHw8* to SpiHw8Stub* for test inspection
/// @param driver SpiHw8 pointer (must be from test environment)
/// @returns SpiHw8Stub pointer, or nullptr if cast fails
inline SpiHw8Stub* toStub(SpiHw8* driver) {
    return static_cast<SpiHw8Stub*>(driver);
}

}  // namespace fl

#endif  // FASTLED_TESTING
