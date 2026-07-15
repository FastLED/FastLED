#pragma once

// IWYU pragma: private

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/irp_pio_tx_peripheral.h"

namespace fl {

class RpPioTxPeripheral final : public IRpPioTxPeripheral {
  public:
    explicit RpPioTxPeripheral(u8 pio_index) FL_NO_EXCEPT;
    ~RpPioTxPeripheral() override;
    bool configure(const RpPioTxConfig& config) FL_NO_EXCEPT override;
    bool startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT override;
    bool isDmaBusy() const FL_NO_EXCEPT override;
    bool isTerminalComplete() const FL_NO_EXCEPT override;
    bool hasError() const FL_NO_EXCEPT override;
    u32 nowMicros() const FL_NO_EXCEPT override;
    void abort() FL_NO_EXCEPT override;
    void deinitialize() FL_NO_EXCEPT override;

  private:
    struct ProgramStorage;
    bool createProgram(const ChipsetTimingConfig& timing) FL_NO_EXCEPT;

    void* mPio;
    int mStateMachine;
    int mDmaChannel;
    int mPin;
    int mProgramOffset;
    u8 mLaneCount;
    u8 mPioIndex;
    ProgramStorage* mProgram;
    bool mInitialized;
};

}  // namespace fl

#endif
