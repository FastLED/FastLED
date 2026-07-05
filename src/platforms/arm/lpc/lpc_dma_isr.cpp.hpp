#pragma once

// IWYU pragma: private

/// @file platforms/arm/lpc/lpc_dma_isr.cpp.hpp
/// @brief Single-TU home of the strong `DMA0_IRQHandler` for the shared
///        DMA interrupt hub (see lpc_dma_isr.h for the design notes).
///
/// The handler must be a strong, non-inline, extern "C" definition in
/// exactly ONE translation unit — it overrides the ACLPC startup's weak
/// vector alias (framework-arduino-lpc8xx#38) at link time. Defining it
/// in the hub header emitted one copy per including TU and the LPC845
/// AutoResearch build failed with 10+ "multiple definition" link errors
/// (2026-07-02); this unity-build slot is the fix.

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/lpc_dma_isr.h"

// ok no namespace fl — the file defines only the extern "C" DMA0_IRQHandler
// vector-table override; everything else lives in lpc_dma_isr.h.

#if defined(FL_IS_ARM_LPC_845) && FASTLED_LPC_DMA_ISR

// Dispatches INTA completion flags to the registered per-channel
// callbacks. INTA bits are W1C; clear BEFORE the callback so a callback
// that immediately re-arms and completes cannot lose its next edge.
extern "C" void DMA0_IRQHandler(void) {
    const fl::u32 pending =
        DMA0->COMMON[0].INTA & DMA0->COMMON[0].INTENSET;
    DMA0->COMMON[0].INTA = pending;  // W1C
    fl::u32 bits = pending;
    while (bits != 0u) {
        // Cortex-M0+ has no CLZ instruction; walk set bits from the LSB.
        const fl::u32 ch = static_cast<fl::u32>(__builtin_ctz(bits));
        bits &= bits - 1u;
        const fl::lpc::DmaIsrSlot slot = fl::lpc::dmaIsrTable()[ch];
        if (slot.fn != nullptr) {
            slot.fn(slot.ctx);
        }
    }
}

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_DMA_ISR
