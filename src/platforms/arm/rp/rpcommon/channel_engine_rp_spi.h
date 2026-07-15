#pragma once

// IWYU pragma: private

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/arm/rp/rpcommon/irp_spi_peripheral.h"

namespace fl {

class ChannelEngineRpSpi final : public IChannelDriver {
  public:
    ChannelEngineRpSpi(fl::shared_ptr<IRpSpiPeripheral> peripheral,
                       u8 spi_index) FL_NO_EXCEPT;
    ~ChannelEngineRpSpi() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        return mSpiIndex == 0 ? fl::string::from_literal("SPI0")
                              : fl::string::from_literal("SPI1");
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(false, true);
    }

  private:
    bool startNextTransmission() FL_NO_EXCEPT;
    bool beginTransmission(const ChannelDataPtr& channel) FL_NO_EXCEPT;
    bool pinsForChannel(const ChannelDataPtr& channel, u8* miso_pin) const FL_NO_EXCEPT;
    void releaseInFlight() FL_NO_EXCEPT;
    DriverState fail(const char* message) FL_NO_EXCEPT;

    fl::shared_ptr<IRpSpiPeripheral> mPeripheral;
    fl::vector<ChannelDataPtr> mPendingChannels;
    fl::vector<ChannelDataPtr> mInFlightChannels;
    u8 mSpiIndex;
    size_t mCurrentChannel;
    u32 mStartUs;
    bool mActive;
    bool mFailed;
    fl::string mError;
};

}  // namespace fl
