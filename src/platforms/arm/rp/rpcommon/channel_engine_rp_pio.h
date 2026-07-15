#pragma once

// IWYU pragma: private

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/arm/rp/rpcommon/irp_pio_tx_peripheral.h"

namespace fl {

/// Single-lane PIO clockless engine. Consecutive multi-lane batching is only
/// admitted once all lanes have exactly the same timing; this engine keeps the
/// initial single-lane path explicit rather than silently batching mismatch.
class ChannelEngineRpPio final : public IChannelDriver {
  public:
    explicit ChannelEngineRpPio(fl::shared_ptr<IRpPioTxPeripheral> peripheral,
                                u8 which) FL_NO_EXCEPT;
    ~ChannelEngineRpPio() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        return mWhich == 0 ? fl::string::from_literal("PIO0")
                           : fl::string::from_literal("PIO1");
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }

  private:
    bool startNextTransmission() FL_NO_EXCEPT;
    bool beginTransmission(const ChannelDataPtr& channel) FL_NO_EXCEPT;
    void releaseInFlight() FL_NO_EXCEPT;
    DriverState fail(const char* message) FL_NO_EXCEPT;

    fl::shared_ptr<IRpPioTxPeripheral> mPeripheral;
    fl::vector<ChannelDataPtr> mPendingChannels;
    fl::vector<ChannelDataPtr> mInFlightChannels;
    fl::vector<u32> mPioWords;
    u8 mWhich;
    size_t mCurrentChannel;
    u32 mLatchStartUs;
    u32 mLatchDurationUs;
    u8 mActiveLaneCount;
    bool mActive;
    bool mLatchPending;
    bool mFailed;
    fl::string mError;
};

}  // namespace fl
