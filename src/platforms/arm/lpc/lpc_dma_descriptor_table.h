#pragma once

/// @file platforms/arm/lpc/lpc_dma_descriptor_table.h
/// @brief Shared LPC845 DMA0 channel-descriptor table + SRAMBASE arming.
///
/// The LPC845 DMA controller fetches each channel's transfer descriptor
/// from a table in SRAM at `DMA0->SRAMBASE + 16*channel` (UM11029 §17.4).
/// SRAMBASE resets to 0 and NOTHING in the ArduinoCore-LPC8xx startup
/// programs it — so any driver that arms a channel without first owning
/// SRAMBASE makes the engine fetch its descriptor from the FLASH VECTOR
/// TABLE at address 0 and then DMA-write through whatever garbage
/// pointers it finds there. That exact failure corrupted `.bss` and sent
/// the CPU executing a static object on real silicon (FastLED #3580,
/// silicon-diagnosed 2026-07-02 via the WWDT NMI backtrace:
/// `FAULT: HardFault PC=0x100018BC` → `harnessDriver()::drv`).
///
/// Every LPC845 DMA user (SPI+DMA driver, SCT+DMA clockless engine)
/// must call `fl::lpc::ensureDmaSramBase()` before arming a channel.
///
/// Sharing semantics: the table covers all 25 channels, so multiple
/// drivers share one table as long as their channel numbers differ —
/// which the drivers already guarantee (SPI uses the SPI-TX request
/// channel, the SCT engine uses the SCT-request channels). If some
/// other component (e.g. `rx_sct_capture.cpp.hpp`, which predates this
/// header and owns a private full-size table) programmed SRAMBASE
/// first, `ensureDmaSramBase()` leaves it alone — any full 25-channel
/// table is interchangeable.
///
/// RAM cost: 400 bytes used + up to 112 bytes of alignment slack
/// (SRAMBASE bits 8:0 are reserved → the table must be 512-byte
/// aligned). Materialised only in builds that call the accessor.

#include "fl/stl/int.h"
#include "fl/stl/align.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/is_arm.h"

#if defined(FL_IS_ARM_LPC_845)

// IWYU pragma: begin_keep
#include <LPC845.h>
// IWYU pragma: end_keep

namespace fl {
namespace lpc {

/// One LPC845 DMA channel descriptor (UM11029 §17.4.3). 16 bytes.
struct FL_ALIGNAS(16) DmaChannelDescriptor {
    fl::u32 reserved;  ///< XFERCFG mirror slot — live config is the register.
    fl::u32 src_end;   ///< Address of the LAST source transfer unit.
    fl::u32 dst_end;   ///< Address of the LAST destination transfer unit.
    fl::u32 link;      ///< Next descriptor (0 = one-shot).
};

/// The process-wide 25-channel descriptor table. Function-local static in
/// an inline function → exactly one instance across all TUs (ODR), only
/// materialised in builds that reference it.
inline volatile DmaChannelDescriptor* dmaDescriptorTable() FL_NO_EXCEPT {
    FL_ALIGNAS(512) static volatile DmaChannelDescriptor table[25] = {};
    return table;
}

/// Program DMA0->SRAMBASE with the shared table if nothing owns it yet.
/// Idempotent; respects a table installed earlier by another component.
/// Callers must have already enabled the DMA0 AHB clock.
inline void ensureDmaSramBase() FL_NO_EXCEPT {
    if (DMA0->SRAMBASE == 0u) {
        DMA0->SRAMBASE = reinterpret_cast<fl::u32>(dmaDescriptorTable());  // ok reinterpret cast — DMA descriptor table base is a raw MMIO pointer
    }
}

}  // namespace lpc
}  // namespace fl

#endif  // FL_IS_ARM_LPC_845
