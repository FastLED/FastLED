/// @file i2s_spi_peripheral_mock.h
/// @brief Mock I2S parallel SPI peripheral for unit testing

#pragma once

// IWYU pragma: private

#include "platforms/esp/32/drivers/i2s_spi/ii2s_spi_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

/// @brief Mock I2S parallel SPI peripheral for unit testing
///
/// Simulates I2S parallel SPI hardware with data capture and error injection.
class I2sSpiPeripheralMock : public II2sSpiPeripheral {
public:
    static I2sSpiPeripheralMock& instance() FL_NOEXCEPT;

    ~I2sSpiPeripheralMock() override = default;

    // II2sSpiPeripheral interface
    bool initialize(const I2sSpiConfig& config) FL_NOEXCEPT override = 0;
    void deinitialize() FL_NOEXCEPT override = 0;
    bool isInitialized() const FL_NOEXCEPT override = 0;

    u8* allocateBuffer(size_t size_bytes) FL_NOEXCEPT override = 0;
    void freeBuffer(u8* buffer) FL_NOEXCEPT override = 0;

    bool transmit(const u8* buffer,
                  size_t size_bytes) FL_NOEXCEPT override = 0;
    bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT override = 0;
    bool isBusy() const FL_NOEXCEPT override = 0;

    bool registerTransmitCallback(void* callback,
                                  void* user_ctx) FL_NOEXCEPT override = 0;
    const I2sSpiConfig& getConfig() const FL_NOEXCEPT override = 0;

    u64 getMicroseconds() FL_NOEXCEPT override = 0;
    void delay(u32 ms) FL_NOEXCEPT override = 0;

    // Mock-specific API
    struct TransmitRecord {
        fl::vector<u8> buffer_copy;
        size_t size_bytes;
        u64 timestamp_us;
    };

    virtual void simulateTransmitComplete() FL_NOEXCEPT = 0;
    virtual void setTransmitFailure(bool should_fail) FL_NOEXCEPT = 0;
    virtual void setTransmitDelay(u32 microseconds) FL_NOEXCEPT = 0;
    virtual void setAutoComplete(bool auto_complete) FL_NOEXCEPT = 0;

    virtual const fl::vector<TransmitRecord>&
    getTransmitHistory() const FL_NOEXCEPT = 0;
    virtual void clearTransmitHistory() FL_NOEXCEPT = 0;
    virtual fl::span<const u8> getLastTransmitData() const FL_NOEXCEPT = 0;

    virtual bool isEnabled() const FL_NOEXCEPT = 0;
    virtual size_t getTransmitCount() const FL_NOEXCEPT = 0;
    virtual void reset() FL_NOEXCEPT = 0;
};

} // namespace detail
} // namespace fl
