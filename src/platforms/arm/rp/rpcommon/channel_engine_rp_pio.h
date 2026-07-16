#pragma once

// IWYU pragma: private

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/arm/rp/rpcommon/irp_pio_spi_peripheral.h"
#include "platforms/arm/rp/rpcommon/irp_pio_tx_peripheral.h"

namespace fl {

/// Unified RP PIO FLEX_IO engine. It serves clockless LED protocols and
/// mode-0 SPI: the latter is the arbitrary-pin fallback when SPI0/SPI1 cannot
/// route the requested pin pair. Consecutive clockless lanes are batched only
/// when timing and length exactly match.
class ChannelEngineRpPio final : public IChannelDriver {
  public:
    explicit ChannelEngineRpPio(fl::shared_ptr<IRpPioTxPeripheral> peripheral,
                                fl::shared_ptr<IRpPioSpiPeripheral> spi_peripheral =
                                    fl::shared_ptr<IRpPioSpiPeripheral>()) FL_NO_EXCEPT;
    ~ChannelEngineRpPio() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    bool isActive() const FL_NO_EXCEPT { return mActive; }
    const fl::string& lastError() const FL_NO_EXCEPT { return mLastError; }
    bool lastStartAttempted() const FL_NO_EXCEPT { return mLastStartAttempted; }
    bool lastStartSucceeded() const FL_NO_EXCEPT { return mLastStartSucceeded; }
    size_t lastWordCount() const FL_NO_EXCEPT { return mLastWordCount; }

    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("FLEX_IO");
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, true);
    }

  private:
    enum class Mode : u8 { Clockless, Spi };

    bool startNextTransmission() FL_NO_EXCEPT;
    bool beginTransmission(const ChannelDataPtr& channel) FL_NO_EXCEPT;
    void releaseInFlight() FL_NO_EXCEPT;
    DriverState fail(const char* message) FL_NO_EXCEPT;

    fl::shared_ptr<IRpPioTxPeripheral> mPeripheral;
    fl::shared_ptr<IRpPioSpiPeripheral> mSpiPeripheral;
    fl::vector<ChannelDataPtr> mPendingChannels;
    fl::vector<ChannelDataPtr> mInFlightChannels;
    fl::vector<u32> mPioWords;
    size_t mCurrentChannel;
    u32 mLatchStartUs;
    u32 mLatchDurationUs;
    u8 mActiveLaneCount;
    Mode mActiveMode;
    bool mActive;
    bool mLatchPending;
    bool mFailed;
    bool mLastStartAttempted;
    bool mLastStartSucceeded;
    size_t mLastWordCount;
    fl::string mError;
    fl::string mLastError;
};

}  // namespace fl
