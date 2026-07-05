#pragma once

/// @file platforms/arm/lpc/lpc_dma_isr.h
/// @brief Shared LPC845 DMA0 interrupt hub — one strong `DMA0_IRQHandler`,
///        per-channel callback dispatch.
///
/// The LPC845 has a single DMA interrupt line for all 25 channels, and the
/// chip has one flash vector table (Cortex-M0+, no VTOR), so exactly ONE
/// strong `DMA0_IRQHandler` can exist per image. Multiple FastLED drivers
/// want completion callbacks (SPI TX streaming, UART TX streaming), so this
/// hub owns the handler and dispatches `COMMON[0].INTA` bits to registered
/// per-channel callbacks.
///
/// Requires ArduinoCore-LPC8xx with named weak IRQ handlers in the startup
/// vector table (framework-arduino-lpc8xx#38) — on older cores the strong
/// symbol links fine but is never wired to the vector, so callbacks simply
/// never fire. Drivers must therefore keep a poll-based fallback (their
/// `done()` / `waitDma()` paths) and treat the ISR as a latency
/// optimization, or gate hard on `FASTLED_LPC_DMA_ISR=1` builds where the
/// new core is guaranteed.
///
/// Opt-in: define `FASTLED_LPC_DMA_ISR=1`. When off, this header still
/// provides the registration API as inert no-ops so driver code doesn't
/// need `#if` at every call site.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/is_arm.h"

#if defined(FL_IS_ARM_LPC_845)

// IWYU pragma: begin_keep
#include <LPC845.h>
// IWYU pragma: end_keep

#ifndef FASTLED_LPC_DMA_ISR
#define FASTLED_LPC_DMA_ISR 0
#endif

namespace fl {
namespace lpc {

/// Per-channel DMA completion callback. Runs in IRQ context — keep it
/// short (re-arm the channel, flip a flag). `ctx` is the pointer passed
/// at registration.
using DmaChannelIsr = void (*)(void* ctx);

struct DmaIsrSlot {
    DmaChannelIsr fn;
    void*         ctx;
};

/// The 25-slot dispatch table. Function-local static in an inline
/// function → single instance ODR-wide.
inline DmaIsrSlot* dmaIsrTable() FL_NO_EXCEPT {
    static DmaIsrSlot table[25] = {};
    return table;
}

/// Register (or replace) the completion callback for `channel`. Passing
/// `fn == nullptr` unregisters. Enables the channel's INTA in the DMA
/// controller and the shared DMA0 IRQ in the NVIC when the ISR build is
/// on; inert no-op otherwise (poll fallback drives the driver).
inline void registerDmaChannelIsr(fl::u32 channel, DmaChannelIsr fn,
                                  void* ctx) FL_NO_EXCEPT {
    if (channel >= 25u) return;
#if FASTLED_LPC_DMA_ISR
    dmaIsrTable()[channel] = DmaIsrSlot{fn, ctx};
    if (fn != nullptr) {
        // Clear any STALE completion before opening the gate: a prior
        // synchronous transfer on this channel (XFERCFG.SETINTA) leaves
        // INTA pending, and enabling INTENSET would fire the callback
        // immediately — before the caller finishes initializing its
        // stream state (observed on LPC845 silicon 2026-07-02).
        DMA0->COMMON[0].INTA = (1UL << channel);  // W1C
        DMA0->COMMON[0].INTENSET = (1UL << channel);
        NVIC_ClearPendingIRQ(DMA0_IRQn);
        NVIC_EnableIRQ(DMA0_IRQn);
    } else {
        DMA0->COMMON[0].INTENCLR = (1UL << channel);
    }
#else
    (void)fn;
    (void)ctx;
#endif
}

}  // namespace lpc
}  // namespace fl

// The strong `DMA0_IRQHandler` override lives in lpc_dma_isr.cpp.hpp
// (single-TU unity-build slot) — a non-inline extern "C" definition in
// this header would be emitted once per including TU and fail the link
// with "multiple definition" (observed 2026-07-02).

#endif  // FL_IS_ARM_LPC_845
