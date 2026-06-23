/// @file lcd_spi_peripheral_mock.h
/// @brief Mock LCD_CAM SPI peripheral for unit testing

#pragma once

// IWYU pragma: private

#include "platforms/esp/32/drivers/lcd_spi/ilcd_spi_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

/// @brief Mock LCD_CAM SPI peripheral for unit testing
class LcdSpiPeripheralMock : public ILcdSpiPeripheral {
  public:
    static LcdSpiPeripheralMock &instance() FL_NOEXCEPT;

    ~LcdSpiPeripheralMock() override = default;

    // ILcdSpiPeripheral interface
    bool initialize(const LcdSpiConfig &config) FL_NOEXCEPT override = 0;
    void deinitialize() FL_NOEXCEPT override = 0;
    bool isInitialized() const FL_NOEXCEPT override = 0;

    u16 *allocateBuffer(size_t size_bytes) FL_NOEXCEPT override = 0;
    void freeBuffer(u16 *buffer) FL_NOEXCEPT override = 0;

    bool transmit(const u16 *buffer,
                  size_t size_bytes) FL_NOEXCEPT override = 0;
    bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT override = 0;
    bool isBusy() const FL_NOEXCEPT override = 0;

    bool registerTransmitCallback(void *callback,
                                  void *user_ctx) FL_NOEXCEPT override = 0;
    const LcdSpiConfig &getConfig() const FL_NOEXCEPT override = 0;

    u64 getMicroseconds() FL_NOEXCEPT override = 0;
    void delay(u32 ms) FL_NOEXCEPT override = 0;

    // Mock-specific API
    struct TransmitRecord {
        fl::vector<u16> buffer_copy;
        size_t size_bytes;
        u64 timestamp_us;
    };

    virtual void simulateTransmitComplete() FL_NOEXCEPT = 0;
    virtual void setTransmitFailure(bool should_fail) FL_NOEXCEPT = 0;
    virtual void setTransmitDelay(u32 microseconds) FL_NOEXCEPT = 0;
    virtual void setAutoComplete(bool auto_complete) FL_NOEXCEPT = 0;

    virtual const fl::vector<TransmitRecord> &
    getTransmitHistory() const FL_NOEXCEPT = 0;
    virtual void clearTransmitHistory() FL_NOEXCEPT = 0;
    virtual fl::span<const u16> getLastTransmitData() const FL_NOEXCEPT = 0;

    virtual bool isEnabled() const FL_NOEXCEPT = 0;
    virtual size_t getTransmitCount() const FL_NOEXCEPT = 0;
    virtual size_t getDeinitCount() const FL_NOEXCEPT = 0;
    virtual void reset() FL_NOEXCEPT = 0;
};

} // namespace detail
} // namespace fl
