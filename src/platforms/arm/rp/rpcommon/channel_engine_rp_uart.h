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

    bool lastStartAttempted() const FL_NO_EXCEPT { return mLastStartAttempted; }
    bool lastStartSucceeded() const FL_NO_EXCEPT { return mLastStartSucceeded; }
    size_t lastEncodedSize() const FL_NO_EXCEPT { return mLastEncodedSize; }
    u32 lastActualBaud() const FL_NO_EXCEPT { return mLastActualBaud; }
    const fl::string& lastError() const FL_NO_EXCEPT { return mLastError; }

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
    // Mutable alongside mLastError: canHandle() is const and is where a
    // decline is decided, including declines that never reach enqueue()
    // because the manager is only asking whether this engine could take the
    // channel. Clearing them there is what keeps a declined channel from
    // reporting the previous run's numbers. See FastLED#4375.
    mutable bool mLastStartAttempted;
    mutable bool mLastStartSucceeded;
    mutable size_t mLastEncodedSize;
    mutable u32 mLastActualBaud;
    mutable fl::string mLastError;   // also set from const canHandle()
};

}  // namespace fl
