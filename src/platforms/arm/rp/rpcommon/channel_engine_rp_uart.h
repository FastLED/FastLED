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

    /// @brief Wire timing of the geometry the last transmission actually used.
    ///
    /// Which geometry gets picked (P=5 vs P=4) depends on the backend's
    /// *maximum* baud, so a caller that has to decode this waveform cannot
    /// re-derive it from the chipset timing alone. Re-deriving it from
    /// `lastActualBaud()` is worse than not having it: the achieved baud is
    /// allowed to sit up to 1% under the requested one, and fed back in as a
    /// ceiling that shortfall makes the transmitted geometry look infeasible
    /// and selects the other one. Recorded here instead, from the same LUT the
    /// encoder ran. `T1 == 0` means nothing has been transmitted yet.
    /// See FastLED#4379.
    const ChipsetTiming& lastWireTiming() const FL_NO_EXCEPT {
        return mLastWireTiming;
    }

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
    ChipsetTiming mLastWireTiming;
    fl::string mError;
    bool mLastStartAttempted;
    bool mLastStartSucceeded;
    size_t mLastEncodedSize;
    u32 mLastActualBaud;
    mutable fl::string mLastError;   // also set from const canHandle()
};

}  // namespace fl
