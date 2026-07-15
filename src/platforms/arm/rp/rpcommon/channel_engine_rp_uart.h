#pragma once

// IWYU pragma: private

/// @file channel_engine_rp_uart.h
/// @brief RP2040/RP2350 UART TX DMA clockless channel engine.

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/arm/rp/rpcommon/irp_uart_peripheral.h"
#include "fl/channels/uart_wave_encoder.h"

namespace fl {

class ChannelEngineRpUart final : public IChannelDriver {
  public:
    ChannelEngineRpUart(fl::shared_ptr<IRpUartPeripheral> peripheral,
                        u8 uart_index) FL_NO_EXCEPT;
    ~ChannelEngineRpUart() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        return mUartIndex == 0 ? fl::string::from_literal("UART0")
                               : fl::string::from_literal("UART1");
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }

  private:
    bool startNextTransmission() FL_NO_EXCEPT;
    bool beginTransmission(const ChannelDataPtr& channel) FL_NO_EXCEPT;
    void releaseInFlight() FL_NO_EXCEPT;
    DriverState fail(const char* message) FL_NO_EXCEPT;
    bool isValidTxPin(int pin) const FL_NO_EXCEPT;

    fl::shared_ptr<IRpUartPeripheral> mPeripheral;
    fl::vector<ChannelDataPtr> mPendingChannels;
    fl::vector<ChannelDataPtr> mInFlightChannels;
    fl::vector<u8> mEncodedBuffer;
    u8 mUartIndex;
    size_t mCurrentChannel;
    u32 mLatchStartUs;
    u32 mLatchDurationUs;
    bool mActive;
    bool mLatchPending;
    bool mFailed;
    fl::string mError;
};

}  // namespace fl
