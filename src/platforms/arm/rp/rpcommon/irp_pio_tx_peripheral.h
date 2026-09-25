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
    /// true: the DMA stream is tightly packed bit-planes (one byte of DMA
    /// per data byte per lane, #4621). false: one 32-bit word per bit-plane
    /// (needed for XTRA0 zero tails, which break byte alignment). Opt-in so
    /// word-per-plane callers keep working.
    bool packed = false;
    ChipsetTimingConfig timing;
};

/// Bytes per DMA transfer for a packed stream: one 8*lanes-bit column of
/// bit-planes per data byte, split into transfers of at most 32 bits. 8-bit
/// and 16-bit DMA writes are replicated across the FIFO word, so the MSBs
/// the left-shifting OSR consumes hold exactly the transfer's bits.
inline u8 rpPioPackedTransferBytes(u8 lane_count) FL_NO_EXCEPT {
    return lane_count >= 4 ? 4 : lane_count;
}

/// Storage words that back @p transfer_count DMA transfers. `startTxDma`
/// takes a transfer count: packed streams use transfers of
/// rpPioPackedTransferBytes(lanes) bytes read little-endian from `words`
/// (a trailing partial word is allowed); unpacked streams use 32-bit words.
inline size_t rpPioTxStorageWords(const RpPioTxConfig& config,
                                  size_t transfer_count) FL_NO_EXCEPT {
    if (!config.packed) return transfer_count;
    const size_t bytes = transfer_count * rpPioPackedTransferBytes(config.lane_count);
    return (bytes + 3u) / 4u;
}

/// DMA completion is not wire completion: the PIO TX FIFO can be empty while
/// the state machine is still generating the final bit. `isTerminalComplete()`
/// becomes true only after the state machine reaches its next blocking `out`
/// instruction with the FIFO empty, proving the previous bit's low tail ended.
class IRpPioTxPeripheral {
  public:
    virtual ~IRpPioTxPeripheral() FL_NO_EXCEPT = default;

    virtual bool configure(const RpPioTxConfig& config) FL_NO_EXCEPT = 0;
    /// @param transfer_count DMA transfers; see rpPioTxStorageWords().
    virtual bool startTxDma(const u32* words, size_t transfer_count) FL_NO_EXCEPT = 0;
    virtual bool isDmaBusy() const FL_NO_EXCEPT = 0;
    virtual bool isTerminalComplete() const FL_NO_EXCEPT = 0;
    virtual bool hasError() const FL_NO_EXCEPT = 0;
    virtual u32 nowMicros() const FL_NO_EXCEPT = 0;
    virtual void abort() FL_NO_EXCEPT = 0;
    virtual void deinitialize() FL_NO_EXCEPT = 0;
};

}  // namespace fl
