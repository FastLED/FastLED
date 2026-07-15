#pragma once

// IWYU pragma: private

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/irp_spi_peripheral.h"

namespace fl {

class RpSpiPeripheral final : public IRpSpiPeripheral {
  public:
    RpSpiPeripheral() FL_NO_EXCEPT;
    ~RpSpiPeripheral() override;

    bool configure(const RpSpiConfig& config) FL_NO_EXCEPT override;
    u32 actualClockHz() const FL_NO_EXCEPT override;
    bool startTxDma(const u8* data, size_t size) FL_NO_EXCEPT override;
    bool startTxDmaCaptureRx(const u8* data, size_t size,
                             u8* rx_data, size_t rx_size) FL_NO_EXCEPT override;
    bool isTxDmaBusy() const FL_NO_EXCEPT override;
    bool isRxDmaBusy() const FL_NO_EXCEPT override;
    bool isWireBusy() const FL_NO_EXCEPT override;
    bool hasError() const FL_NO_EXCEPT override;
    u32 nowMicros() const FL_NO_EXCEPT override;
    void abort() FL_NO_EXCEPT override;
    void deinitialize() FL_NO_EXCEPT override;

  private:
    bool startTxDmaImpl(const u8* data, size_t size, u8* rx_data,
                        size_t rx_size) FL_NO_EXCEPT;

    int mTxDmaChannel;
    int mRxDmaChannel;
    int mSpiIndex;
    int mMosiPin;
    int mMisoPin;
    int mSckPin;
    u8 mRxSink;
    u32 mActualClockHz;
    bool mInitialized;
    bool mOwnsSpi;
    bool mOwnsPins;
};

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
