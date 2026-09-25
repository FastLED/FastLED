#pragma once

// IWYU pragma: private

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/arm/rp/rpcommon/irp_pio_spi_peripheral.h"
#include "platforms/arm/rp/rpcommon/irp_pio_tx_peripheral.h"

namespace fl {

/// Unified RP PIO FLEX_IO engine. It serves clockless LED protocols and
/// mode-0 SPI: the latter is the arbitrary-pin fallback when SPI0/SPI1 cannot
/// route the requested pin pair. Consecutive clockless lanes are batched only
/// when timing and length exactly match.
///
/// Independent clockless batches (non-consecutive pins, different lengths or
/// timings) transmit concurrently, each on its own state machine + DMA
/// channel, when `extra_tx_factory` can supply more TX peripherals (#4620).
/// Without a factory, or once PIO/DMA resources run out, the remaining
/// batches wait for a busy one to finish and reuse its peripheral.
class ChannelEngineRpPio final : public IChannelDriver {
  public:
    using TxPeripheralFactory = fl::function<fl::shared_ptr<IRpPioTxPeripheral>()>;

    /// Maximum concurrently transmitting clockless batches (TX peripherals):
    /// one PIO block's state machines, so an engine's extra slots never
    /// crowd out the other PIO blocks' engines.
    static constexpr u8 kMaxTxSlots = 4;

    explicit ChannelEngineRpPio(fl::shared_ptr<IRpPioTxPeripheral> peripheral,
                                fl::shared_ptr<IRpPioSpiPeripheral> spi_peripheral =
                                    fl::shared_ptr<IRpPioSpiPeripheral>(),
                                const char* driver_name = "FLEX_IO",
                                TxPeripheralFactory extra_tx_factory =
                                    TxPeripheralFactory()) FL_NO_EXCEPT;
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
        return fl::string::from_literal(mDriverName);
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, true);
    }

  private:
    /// One clockless batch: `lanes` consecutive in-flight channels.
    struct Job {
        size_t first = 0;
        u8 lanes = 1;
        bool spi = false;
    };

    /// One TX peripheral (state machine + DMA) and the batch it carries.
    struct TxSlot {
        fl::shared_ptr<IRpPioTxPeripheral> peripheral;
        fl::vector<u32> words;
        size_t job = 0;
        u32 latchStartUs = 0;
        u32 latchDurationUs = 0;
        bool active = false;
        bool latchPending = false;
    };

    void buildJobs() FL_NO_EXCEPT;
    bool dispatch() FL_NO_EXCEPT;
    bool startClockless(TxSlot& slot, size_t job_index) FL_NO_EXCEPT;
    bool startSpi(size_t job_index) FL_NO_EXCEPT;
    TxSlot* idleTxSlot() FL_NO_EXCEPT;
    bool anyActive() const FL_NO_EXCEPT;
    void releaseInFlight() FL_NO_EXCEPT;
    DriverState fail(const char* message) FL_NO_EXCEPT;

    fl::shared_ptr<IRpPioSpiPeripheral> mSpiPeripheral;
    TxPeripheralFactory mTxFactory;
    const char* mDriverName;
    fl::vector<TxSlot> mTxSlots;
    fl::vector<ChannelDataPtr> mPendingChannels;
    fl::vector<ChannelDataPtr> mInFlightChannels;
    fl::vector<Job> mJobs;
    fl::vector<u32> mSpiWords;
    size_t mNextJob;
    bool mSpiActive;
    bool mTxExhausted;
    bool mActive;
    bool mFailed;
    bool mLastStartAttempted;
    bool mLastStartSucceeded;
    size_t mLastWordCount;
    fl::string mError;
    fl::string mLastError;
};

}  // namespace fl
