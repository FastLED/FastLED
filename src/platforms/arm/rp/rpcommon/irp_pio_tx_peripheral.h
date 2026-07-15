#pragma once

// IWYU pragma: private

/// @file irp_pio_tx_peripheral.h
/// @brief Host-testable terminal-completion contract for RP PIO LED TX.

#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

struct RpPioTxConfig {
    u8 tx_pin = 0;
    u8 lane_count = 1;
    ChipsetTimingConfig timing;
};

/// DMA completion is not wire completion: the PIO TX FIFO can be empty while
/// the state machine is still generating the final bit. `isTerminalComplete()`
/// becomes true only after the state machine reaches its next blocking `out`
/// instruction with the FIFO empty, proving the previous bit's low tail ended.
class IRpPioTxPeripheral {
  public:
    virtual ~IRpPioTxPeripheral() FL_NO_EXCEPT = default;

    virtual bool configure(const RpPioTxConfig& config) FL_NO_EXCEPT = 0;
    virtual bool startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT = 0;
    virtual bool isDmaBusy() const FL_NO_EXCEPT = 0;
    virtual bool isTerminalComplete() const FL_NO_EXCEPT = 0;
    virtual bool hasError() const FL_NO_EXCEPT = 0;
    virtual u32 nowMicros() const FL_NO_EXCEPT = 0;
    virtual void abort() FL_NO_EXCEPT = 0;
    virtual void deinitialize() FL_NO_EXCEPT = 0;
};

}  // namespace fl
