#pragma once

// IWYU pragma: private

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/irp_uart_peripheral.h"

namespace fl {

class RpUartPeripheral final : public IRpUartPeripheral {
  public:
    RpUartPeripheral() FL_NO_EXCEPT;
    ~RpUartPeripheral() override;

    bool configure(const RpUartConfig& config) FL_NO_EXCEPT override;
    u32 actualBaudRate() const FL_NO_EXCEPT override;
    bool startTxDma(const u8* data, size_t size) FL_NO_EXCEPT override;
    bool isDmaBusy() const FL_NO_EXCEPT override;
    bool isWireBusy() const FL_NO_EXCEPT override;
    bool hasError() const FL_NO_EXCEPT override;
    u32 nowMicros() const FL_NO_EXCEPT override;
    void abort() FL_NO_EXCEPT override;
    void deinitialize() FL_NO_EXCEPT override;

  private:
    bool isValidTxPin(const RpUartConfig& config) const FL_NO_EXCEPT;

    int mDmaChannel;
    int mUartIndex;
    int mTxPin;
    u32 mActualBaudRate;
    bool mInitialized;
    bool mOwnsUart;
    bool mOwnsPin;
};

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
