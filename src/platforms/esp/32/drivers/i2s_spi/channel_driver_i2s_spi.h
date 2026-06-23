/// @file channel_driver_i2s_spi.h
/// @brief I2S parallel SPI channel driver for true SPI chipsets (APA102, SK9822)
///
/// Uses I2S parallel mode on ESP32dev or LCD_CAM on ESP32-S3 to drive
/// clocked SPI LED strips. Handles SPI chipsets only (not clockless).

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/i2s_spi/ii2s_spi_peripheral.h"

namespace fl {

/// @brief Channel driver for SPI chipsets via I2S parallel mode
///
/// Implements IChannelDriver for true SPI protocols (APA102, SK9822, HD108).
/// Uses I2S peripheral in parallel mode for up to 16 simultaneous data lanes
/// with a shared clock output.
///
/// Currently blocking: show() waits for transmission to complete.
class ChannelDriverI2sSpi : public IChannelDriver {
  public:
    explicit ChannelDriverI2sSpi(
        fl::shared_ptr<detail::II2sSpiPeripheral> peripheral) FL_NOEXCEPT;
    ~ChannelDriverI2sSpi() override;

    bool canHandle(const ChannelDataPtr &data) const FL_NOEXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NOEXCEPT override;
    void show() FL_NOEXCEPT override;
    DriverState poll() FL_NOEXCEPT override;

    fl::string getName() const FL_NOEXCEPT override {
        return fl::string::from_literal("I2S_SPI");
    }

    Capabilities getCapabilities() const FL_NOEXCEPT override {
        return Capabilities(false, true); // SPI only
    }

  private:
    bool beginTransmission(
        fl::span<const ChannelDataPtr> channels) FL_NOEXCEPT;

    fl::shared_ptr<detail::II2sSpiPeripheral> mPeripheral;
    bool mInitialized;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    u8 *mBuffer;
    size_t mBufferSize;
    volatile bool mBusy;
};

/// @brief Factory function to create I2S_SPI driver with real hardware
fl::shared_ptr<IChannelDriver> createI2sSpiEngine() FL_NOEXCEPT;

} // namespace fl
