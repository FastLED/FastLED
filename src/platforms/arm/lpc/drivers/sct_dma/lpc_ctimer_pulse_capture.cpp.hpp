#pragma once

// IWYU pragma: private

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

// Built only with the opt-in SCT+DMA engine it verifies.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)

#include "platforms/arm/lpc/drivers/sct_dma/lpc_ctimer_pulse_capture.h"
#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.h"
#include "platforms/arm/lpc/lpc_dma_descriptor_table.h"
#include "fl/stl/static_assert.h"

namespace fl {

// The unity build shares this TU with other DMA users, so these names carry
// a kCap / lpcCtimer prefix.

constexpr u32 kCapCh = FL_LPC_CTIMER_CAPTURE_DMA_CH;
FL_STATIC_ASSERT(kCapCh < 25u, "FL_LPC_CTIMER_CAPTURE_DMA_CH must be a DMA0 channel");
FL_STATIC_ASSERT(kCapCh != FASTLED_LPC_PWM_DMA_BASECH,
                 "FL_LPC_CTIMER_CAPTURE_DMA_CH must differ from the "
                 "transmitter's FASTLED_LPC_PWM_DMA_BASECH");

// INPUTMUX DMA_ITRIG sources (UM11029 Table 288).
constexpr u32 kCapItrigT0DmaReqM0 = 0x9u;
constexpr u32 kCapItrigNone = 0xFu;
constexpr u32 kCapSwmUnassigned = 0xFFu;

// CTIMER register fields (UM11029 ch. 20).
constexpr u32 kCapTcrCen = (1u << 0);
constexpr u32 kCapTcrCrst = (1u << 1);
constexpr u32 kCapMcrMr1Stop = (1u << 5);    // MR1S: stop the timer on MR1
constexpr u32 kCapCcrCap0Fe = (1u << 1);     // CR0 <- TC on CAP0 falling edge
constexpr u32 kCapCtcrEncc = (1u << 4);      // clear TC on SELCC edge
constexpr u32 kCapCtcrSelccCap0Rise = (0u << 5);

// DMA channel: one CR0 read per MR0 trigger.
constexpr u32 kCapDmaCfg = (1u << 1) | (1u << 4) | (1u << 6);  // HWTRIGEN | rising | TRIGBURST
constexpr u32 kCapXferValid = (1u << 0);
constexpr u32 kCapXferWidth32 = (2u << 8);
constexpr u32 kCapXferDstInc1 = (1u << 14);

inline void lpcCtimerSetCap0Pin(u32 pin) FL_NO_EXCEPT {
    SWM0->PINASSIGN.PINASSIGN14 =
        (SWM0->PINASSIGN.PINASSIGN14 & ~SWM_PINASSIGN14_T0_CAP0_MASK) |
        SWM_PINASSIGN14_T0_CAP0(pin);
}

bool LpcCtimerPulseCapture::begin(int pin, u32 sample_ticks, u32 max_gap_ticks,
                                  fl::span<u32> out) FL_NO_EXCEPT {
    if (pin < 0 || pin > 31 || out.empty() || out.size() > 1023u ||
        sample_ticks == 0u || max_gap_ticks <= sample_ticks) {
        return false;
    }
    mCapacity = static_cast<u32>(out.size());

    SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_CTIMER_MASK |
                              SYSCON_SYSAHBCLKCTRL0_DMA_MASK |
                              SYSCON_SYSAHBCLKCTRL0_SWM_MASK;
    // CTIMER0 is ours to reset; DMA0 and SWM are shared and are not.
    SYSCON->PRESETCTRL0 &= ~SYSCON_PRESETCTRL0_CTIMER_RST_N_MASK;
    SYSCON->PRESETCTRL0 |= SYSCON_PRESETCTRL0_CTIMER_RST_N_MASK;

    lpcCtimerSetCap0Pin(static_cast<u32>(pin));

    CTIMER0->TCR = kCapTcrCrst;
    CTIMER0->PR = 0u;
    CTIMER0->MCR = kCapMcrMr1Stop;
    CTIMER0->CCR = kCapCcrCap0Fe;
    CTIMER0->CTCR = kCapCtcrEncc | kCapCtcrSelccCap0Rise;   // timer mode
    CTIMER0->MR[0] = sample_ticks;
    CTIMER0->MR[1] = max_gap_ticks;
    CTIMER0->TCR = 0u;
    // Park the counter far past MR0/MR1 so neither matches before the first
    // rising edge clears it (0x10000000 ticks to the wrap: ~11 s at 24 MHz,
    // so start the frame within that).
    CTIMER0->TC = 0xF0000000u;

    DMA0->CTRL = 1u;
    fl::lpc::ensureDmaSramBase();
    lpcDmaStopChannel(kCapCh);
    INPUTMUX->DMA_ITRIG_INMUX[kCapCh] = kCapItrigT0DmaReqM0;
    volatile fl::lpc::DmaChannelDescriptor& d =
        reinterpret_cast<volatile fl::lpc::DmaChannelDescriptor*>(  // ok reinterpret cast - DMA descriptor table base (SRAMBASE may be another component's table)
            DMA0->SRAMBASE)[kCapCh];
    d.src_end = reinterpret_cast<fl::u32>(&CTIMER0->CR[0]);  // ok reinterpret cast - MMIO source address
    d.dst_end = reinterpret_cast<fl::u32>(&out[mCapacity - 1u]);  // ok reinterpret cast - RAM destination address
    d.link = 0u;
    DMA0->CHANNEL[kCapCh].CFG = kCapDmaCfg;
    DMA0->CHANNEL[kCapCh].XFERCFG =
        kCapXferValid | kCapXferWidth32 | kCapXferDstInc1 | (((mCapacity - 1u) & 0x3FFu) << 16);
    // A timer DMA request can be pending before any match; writing the
    // interrupt flags clears it (UM11029 20.8.2).
    CTIMER0->IR = 0xFFu;
    DMA0->COMMON[0].SETVALID = (1u << kCapCh);
    DMA0->COMMON[0].ENABLESET = (1u << kCapCh);
    CTIMER0->TCR = kCapTcrCen;
    return true;
}

u32 LpcCtimerPulseCapture::count() const FL_NO_EXCEPT {
    // While armed, the XFERCFG residual is (N-1) - K. When the descriptor
    // runs out it wraps to 0x3FF and CFGVALID stays set (#4676), so a full
    // buffer is "left ACTIVE0 with residual 0x3FF". N <= 1023 keeps that
    // apart from an armed, empty channel (residual N-1 < 0x3FF).
    const u32 residual = (DMA0->CHANNEL[kCapCh].XFERCFG >> 16) & 0x3FFu;
    const bool active = (DMA0->COMMON[0].ACTIVE & (1u << kCapCh)) != 0u;
    if (!active && residual == 0x3FFu) {
        return mCapacity;
    }
    if (residual >= mCapacity) {
        return 0u;
    }
    return mCapacity - 1u - residual;
}

void LpcCtimerPulseCapture::end() FL_NO_EXCEPT {
    CTIMER0->TCR = 0u;
    lpcDmaStopChannel(kCapCh);
    INPUTMUX->DMA_ITRIG_INMUX[kCapCh] = kCapItrigNone;
    lpcCtimerSetCap0Pin(kCapSwmUnassigned);
}

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA
