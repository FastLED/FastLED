// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/lpuart_driver.h"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/lpuart_encoder.h"

#include "fl/log/log.h"
#include "fl/stl/cstring.h"

// IWYU pragma: begin_keep
#include <Arduino.h>
#include <imxrt.h>
#include <DMAChannel.h>
// IWYU pragma: end_keep

namespace fl {

// ============================================================================
// Pin map
// ============================================================================
//
// Each entry binds a Teensy digital pin to:
//   - the LPUARTn index it muxes to (ALT2 on every supported pin),
//   - the IOMUXC mux + pad register addresses for that pad.
//
// The mux ALT value is always 2 for LPUART TX on the i.MX RT1062. The
// LPUART instance index differs between Teensy 4.0 and 4.1 on pin 8
// (and only pin 8): T4.0 -> LPUART4, T4.1 -> LPUART3. Pin tables in
// this file follow the T4.0 mapping where they conflict because the
// autoresearch hardware in #3023.D and the goal-defined loopback rig
// run on T4.0.
//
// IOMUXC offsets are byte offsets from IOMUXC base 0x401F8000 and are
// cross-checked against Teensyduino imxrt.h IOMUXC_SW_MUX_CTL_PAD_*
// / IOMUXC_SW_PAD_CTL_PAD_* macros (same approach as the FlexIO mux
// fix in #3416 FX-CRIT-1).

struct LpuartPinEntry {
    u8 teensy_pin;
    u8 lpuart_index;
    u8 mux_alt;             ///< IOMUXC ALT value to select LPUART TX
                            ///  on this pad. Almost always 2; the
                            ///  T4.1 LPUART5 TX (pin 35) is the lone
                            ///  exception at ALT 1.
    u32 mux_reg_offset;
    u32 pad_reg_offset;
    // #3402-class IOMUXC DAISY: each LPUART TX peripheral's input
    // selector points at a specific pad. `select_input_offset` is the
    // byte offset within the IOMUXC_b region (base 0x401F8400) of the
    // LPUARTn_TX_SELECT_INPUT register, and `select_input_value` is
    // what to write into it to pick THIS pad. Set both to 0 if the
    // LPUART has no SELECT_INPUT (LPUART1's TX is fixed-routed).
    u32 select_input_offset;
    u32 select_input_value;
};

// Renamed kIomuxcBase / sLp* to avoid unity-build symbol collisions with
// the FlexIO driver which also lives in `namespace fl` at file scope.
static constexpr u32 kLpuartIomuxcBase = 0x401F8000u;

// Per-LPUART TX_SELECT_INPUT register offsets (within IOMUXC_b at
// IOMUXC_BASE + 0x400). LPUART1 TX has no select-input register
// (fixed pad routing on Teensy 4.x); set offset=0 and the driver skips
// the write.
static constexpr u32 kSelectInputLpuart2Tx = 0x130;
static constexpr u32 kSelectInputLpuart3Tx = 0x13C;
static constexpr u32 kSelectInputLpuart4Tx = 0x144;
static constexpr u32 kSelectInputLpuart5Tx = 0x14C;
static constexpr u32 kSelectInputLpuart6Tx = 0x154;
static constexpr u32 kSelectInputLpuart7Tx = 0x15C;
static constexpr u32 kSelectInputLpuart8Tx = 0x164;

// Per-pin entries audited against Teensyduino HardwareSerial*.cpp
// (`UARTn_Hardware` struct's `{pin, mux_val, select_input_register,
// select_input_value}` 4-tuple) and the IOMUXC_SW_MUX_CTL_PAD_GPIO_*
// offsets in imxrt.h. Each pad's PAD_CTL register lives at +0x1F0
// from its MUX_CTL register (universal IOMUXC layout).
static constexpr LpuartPinEntry kLpuartPins[] = {
    // pin, LPUARTn, alt, mux_offset (PAD_GPIO_*), pad_offset (mux+0x1F0),
    //                                            sel_input_offset, sel_input_value
    //
    // T4.0 / T4.1 shared entries (top header rows). Verified against
    // Teensyduino HardwareSerial[1-7].cpp UARTn_Hardware structs.
    { 1,  6, 2, 0x0EC, 0x2DC, kSelectInputLpuart6Tx, 1},  // GPIO_AD_B0_12 (Serial1)
    { 8,  4, 2, 0x17C, 0x36C, kSelectInputLpuart4Tx, 2},  // GPIO_B1_00    (Serial2)
    {14,  2, 2, 0x104, 0x2F4, kSelectInputLpuart2Tx, 1},  // GPIO_AD_B1_02 (Serial3)
    {17,  3, 2, 0x114, 0x304, kSelectInputLpuart3Tx, 0},  // GPIO_AD_B1_06 (Serial4)
    {20,  8, 2, 0x124, 0x314, kSelectInputLpuart8Tx, 1},  // GPIO_AD_B1_10 (Serial5)
    {24,  1, 2, 0x0F0, 0x2E0, 0,                     0},  // GPIO_AD_B0_13 (Serial6) LPUART1 fixed-route
    {29,  7, 2, 0x090, 0x280, kSelectInputLpuart7Tx, 1},  // GPIO_EMC_31   (Serial7)
#if defined(ARDUINO_TEENSY41)
    // T4.1-only entries (bottom SD pad row + B1 pad row). Verified
    // against Teensyduino HardwareSerial[5,8].cpp T4.1 alt tuples
    // (the second per-port `{pin, mux_val, sel_input_reg, val}`
    // tuple). Pin 53 is also LPUART6 ALT2; LPUART8 ALT2 is pin 47.
    {35,  5, 1, 0x1AC, 0x39C, kSelectInputLpuart5Tx, 1},  // GPIO_B1_12    (Serial8 T4.1)
    {47,  8, 2, 0x1CC, 0x3BC, kSelectInputLpuart8Tx, 0},  // GPIO_SD_B0_04 (Serial5 alt T4.1)
    {53,  6, 2, 0x078, 0x268, kSelectInputLpuart6Tx, 0},  // GPIO_EMC_25   (Serial1 alt T4.1)
#endif
};
static constexpr int kNumLpuartPins =
    sizeof(kLpuartPins) / sizeof(kLpuartPins[0]);

bool lpuart_lookup_pin(u8 teensy_pin, LpuartPinInfo* info) {
    for (int i = 0; i < kNumLpuartPins; ++i) {
        if (kLpuartPins[i].teensy_pin == teensy_pin) {
            info->teensy_pin = teensy_pin;
            info->lpuart_index = kLpuartPins[i].lpuart_index;
            info->mux_alt = kLpuartPins[i].mux_alt;
            info->mux_reg = (volatile u32*)(kLpuartIomuxcBase + kLpuartPins[i].mux_reg_offset);
            info->pad_reg = (volatile u32*)(kLpuartIomuxcBase + kLpuartPins[i].pad_reg_offset);
            // IOMUXC_b lives at IOMUXC_BASE + 0x400. select_input_offset
            // is the byte offset within IOMUXC_b.
            if (kLpuartPins[i].select_input_offset) {
                info->select_input_reg = (volatile u32*)(
                    kLpuartIomuxcBase + 0x400u + kLpuartPins[i].select_input_offset);
                info->select_input_value = kLpuartPins[i].select_input_value;
            } else {
                info->select_input_reg = nullptr;
                info->select_input_value = 0;
            }
            return true;
        }
    }
    return false;
}

// ============================================================================
// Driver state
// ============================================================================

// LPUART base address lookup (index 1..8).
static IMXRT_LPUART_t* lpuart_base(u8 index) {
    switch (index) {
        case 1: return &IMXRT_LPUART1;
        case 2: return &IMXRT_LPUART2;
        case 3: return &IMXRT_LPUART3;
        case 4: return &IMXRT_LPUART4;
        case 5: return &IMXRT_LPUART5;
        case 6: return &IMXRT_LPUART6;
        case 7: return &IMXRT_LPUART7;
        case 8: return &IMXRT_LPUART8;
        default: return nullptr;
    }
}

// CCM clock gate helper (index 1..8).
static void lpuart_clock_gate_on(u8 index) {
    switch (index) {
        case 1: CCM_CCGR5 |= CCM_CCGR5_LPUART1(CCM_CCGR_ON); break;
        case 2: CCM_CCGR0 |= CCM_CCGR0_LPUART2(CCM_CCGR_ON); break;
        case 3: CCM_CCGR0 |= CCM_CCGR0_LPUART3(CCM_CCGR_ON); break;
        case 4: CCM_CCGR1 |= CCM_CCGR1_LPUART4(CCM_CCGR_ON); break;
        case 5: CCM_CCGR3 |= CCM_CCGR3_LPUART5(CCM_CCGR_ON); break;
        case 6: CCM_CCGR3 |= CCM_CCGR3_LPUART6(CCM_CCGR_ON); break;
        case 7: CCM_CCGR5 |= CCM_CCGR5_LPUART7(CCM_CCGR_ON); break;
        case 8: CCM_CCGR6 |= CCM_CCGR6_LPUART8(CCM_CCGR_ON); break;
        default: break;
    }
}

// DMAMUX TX source per LPUART instance.
static u8 lpuart_dmamux_tx_source(u8 index) {
    switch (index) {
        case 1: return DMAMUX_SOURCE_LPUART1_TX;
        case 2: return DMAMUX_SOURCE_LPUART2_TX;
        case 3: return DMAMUX_SOURCE_LPUART3_TX;
        case 4: return DMAMUX_SOURCE_LPUART4_TX;
        case 5: return DMAMUX_SOURCE_LPUART5_TX;
        case 6: return DMAMUX_SOURCE_LPUART6_TX;
        case 7: return DMAMUX_SOURCE_LPUART7_TX;
        case 8: return DMAMUX_SOURCE_LPUART8_TX;
        default: return 0;
    }
}

static DMAChannel* sLpDmaChannel = nullptr;
static volatile bool sLpDmaComplete = true;
static bool sLpInitialized = false;
static LpuartPinInfo sLpPinInfo{};
// CodeRabbit-flagged: honour the public reset_us contract. We record
// the requested reset/latch micros at init time and the millis() at
// which the last frame finished; lpuart_show_encoded waits for the
// reset window to elapse before starting the next frame.
static u32 sLpResetUs = 280;
static volatile u32 sLpLastFrameEndUs = 0;

// Max strip size: 1024 raw bytes -> 4096 UART bytes. Larger strips would
// need a heap allocation; defer to FX-MED-2-style truncation warning.
static constexpr u32 kMaxRawBytes = 1024;
DMAMEM static u8 sLpUartBuffer[kMaxRawBytes * 4] __attribute__((aligned(32)));

// ============================================================================
// Pin + clock + LPUART setup
// ============================================================================
//
// Encoder lives in lpuart_encoder.h (host-testable).

static void lpuart_pin_init(const LpuartPinInfo& pin_info) {
    // ALT2 + SION. SION (bit 4) is empirically required for FlexIO and
    // FlexPWM input capture; for LPUART TX it shouldn't matter, but
    // mirror the FlexIO pattern for safety.
    *(pin_info.mux_reg) = pin_info.mux_alt | 0x10u;
    // Drive strength + speed + keeper. Mirror RX_FLEXPWM PAD_CTL --
    // the actual drive needs to be strong (DSE=6) for 4 Mbps edges.
    *(pin_info.pad_reg) = (6u << 3) |       // DSE = R0/6 (~30 ohm)
                          (2u << 6) |       // SPEED = 150 MHz
                          (1u << 12);       // PKE (keeper)
    // IOMUXC SELECT_INPUT daisy: connect this pad to the LPUART
    // internal TX output. The #3402-class fix. Without this write
    // the LPUART runs but its TX output is routed to a different
    // (possibly unrouted) pad.
    if (pin_info.select_input_reg) {
        *(pin_info.select_input_reg) = pin_info.select_input_value;
    }
}

// Configure clock and LPUART registers for 4 Mbps, TXINV=1, 8N1.
//
// LPUART input clock on i.MX RT1062 defaults to PLL3_80M (80 MHz)
// regardless of CSCDR1 setting (post-reset). For 4 Mbps:
//   80 MHz / total_div = 4 MHz   -> total_div = 20
//   With OSR = 4, SBR = total_div / (OSR + 1) = 20 / 5 = 4
//
// OSR is the oversample rate (number of samples per UART bit MINUS ONE
// per the RM); 4 means 5x oversampling which is the LPUART minimum.
static void lpuart_configure(u8 lpuart_index) {
    IMXRT_LPUART_t* lp = lpuart_base(lpuart_index);
    if (!lp) return;

    // Force LPUART clock source = PLL3_80M (80 MHz), divider = 1.
    // Empirical bring-up (hardware run on Teensy 4.0) showed the
    // post-Teensyduino-boot LPUART clock running at 24 MHz, producing
    // ~1.2 Mbps with our OSR=4/SBR=4 BAUD config instead of the
    // intended 4 Mbps. Explicitly select PLL3_80M and divide-by-1
    // here so the baud math is stable regardless of what other
    // peripherals did to CSCDR1.
    CCM_CSCDR1 = (CCM_CSCDR1 & ~(CCM_CSCDR1_UART_CLK_SEL |
                                  CCM_CSCDR1_UART_CLK_PODF(0x3F)));
    // After clearing: SEL=0 (PLL3_80M) and PODF=0 (divide by 1).

    lpuart_clock_gate_on(lpuart_index);

    // Disable TX/RX while reconfiguring.
    lp->CTRL = 0;

    // BAUD: OSR=4 (5x oversample), SBR=4, TDMAE (TX DMA enable) at bit 23.
    // 80 MHz / (5 * 4) = 4 Mbps -> 250 ns per UART bit.
    lp->BAUD = LPUART_BAUD_OSR(4) | LPUART_BAUD_SBR(4) | LPUART_BAUD_TDMAE;

    // Clear status flags (W1C).
    lp->STAT = 0;

    // PINCFG default. FIFO enable TX FIFO so DMA writes can stage data.
    lp->FIFO = LPUART_FIFO_TXFE;
    lp->WATER = 0;  // TX watermark 0 -> raise TDRE as soon as 1 slot free

    // CTRL: TXINV=1 (bit 28) inverts the TX wire output (start becomes
    // HIGH, stop becomes LOW). TE=1 (bit 19) enables the transmitter.
    lp->CTRL = LPUART_CTRL_TXINV | LPUART_CTRL_TE;
}

// ============================================================================
// DMA setup
// ============================================================================

static void lpuart_dma_isr() {
    if (sLpDmaChannel != nullptr) {
        sLpDmaChannel->clearInterrupt();
    }
    asm volatile("dsb" ::: "memory");
    sLpDmaComplete = true;
}

static bool lpuart_dma_init() {
    if (sLpDmaChannel) return true;
    sLpDmaChannel = new DMAChannel();  // ok bare allocation - Teensyduino DMAChannel has no fl smart-pointer factory
    if (!sLpDmaChannel) return false;
    sLpDmaChannel->triggerAtHardwareEvent(lpuart_dmamux_tx_source(sLpPinInfo.lpuart_index));
    sLpDmaChannel->attachInterrupt(lpuart_dma_isr);
    return true;
}

// ============================================================================
// Public API
// ============================================================================

bool lpuart_init(const LpuartPinInfo& pin_info, u32 reset_us) {
    if (sLpInitialized) lpuart_deinit();

    sLpPinInfo = pin_info;
    sLpResetUs = (reset_us > 0) ? reset_us : 280u;
    sLpLastFrameEndUs = 0;

    lpuart_configure(pin_info.lpuart_index);
    lpuart_pin_init(pin_info);

    if (!lpuart_dma_init()) return false;

    sLpInitialized = true;
    sLpDmaComplete = true;
    return true;
}

// Shared internal entry point: arms the eDMA TCD to stream `uart_count`
// bytes from `src` to LPUARTn_DATA. Caller has already flushed any
// dirty cache lines covering `src`.
static bool lpuart_arm_dma(const u8* src, u32 uart_count) {
    IMXRT_LPUART_t* lp = lpuart_base(sLpPinInfo.lpuart_index);
    if (!lp) return false;

    sLpDmaChannel->disable();
    sLpDmaChannel->clearComplete();
    sLpDmaChannel->clearInterrupt();
    sLpDmaChannel->clearError();
    sLpDmaComplete = false;

    sLpDmaChannel->TCD->SADDR = src;
    sLpDmaChannel->TCD->SOFF = 1;
    sLpDmaChannel->TCD->ATTR = DMA_TCD_ATTR_SSIZE(0) | DMA_TCD_ATTR_DSIZE(0);
    sLpDmaChannel->TCD->NBYTES_MLNO = 1;
    sLpDmaChannel->TCD->SLAST = -(i32)uart_count;
    sLpDmaChannel->TCD->DADDR = &(lp->DATA);
    sLpDmaChannel->TCD->DOFF = 0;
    sLpDmaChannel->TCD->CITER_ELINKNO = uart_count;
    sLpDmaChannel->TCD->BITER_ELINKNO = uart_count;
    sLpDmaChannel->TCD->DLASTSGA = 0;
    sLpDmaChannel->TCD->CSR = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_DREQ;

    sLpDmaChannel->enable();
    return true;
}

bool lpuart_show_encoded(const u8* encoded, u32 num_uart_bytes) {
    if (!sLpInitialized || !sLpDmaChannel || !encoded || num_uart_bytes == 0) {
        return false;
    }
    // CodeRabbit-flagged: honour reset_us. Hold the previous-frame's
    // latch window before arming the next TX so back-to-back show()
    // calls can't start a new WS2812 frame before the strip has
    // latched the previous one.
    if (sLpLastFrameEndUs != 0) {
        const u32 elapsed = (u32)(micros() - sLpLastFrameEndUs);
        if (elapsed < sLpResetUs) {
            delayMicroseconds(sLpResetUs - elapsed);
        }
    }
    lpuart_wait();
    arm_dcache_flush_delete(const_cast<u8*>(encoded), num_uart_bytes);
    bool ok = lpuart_arm_dma(encoded, num_uart_bytes);
    if (!ok) return false;
    // Block until DMA major-loop ISR fires, then wait for LPUART TC
    // (transmit complete) so the final byte fully drains through the
    // FIFO + shift register before we let the caller return.
    lpuart_wait();
    IMXRT_LPUART_t* lp = lpuart_base(sLpPinInfo.lpuart_index);
    if (lp) {
        const u32 tc_start = micros();
        while ((lp->STAT & (1u << 22)) == 0) {  // TC = bit 22
            if ((u32)(micros() - tc_start) > 1000u) break;
        }
    }
    sLpLastFrameEndUs = micros();
    return true;
}

bool lpuart_show(const u8* pixel_data, u32 num_pixel_bytes) {
    if (!sLpInitialized || !sLpDmaChannel || !pixel_data || num_pixel_bytes == 0) {
        return false;
    }

    lpuart_wait();

    // CodeRabbit-flagged: reject oversize transfers instead of
    // silently truncating. A partial frame with the caller believing
    // the update succeeded would leave the tail LEDs stale and
    // misreport success.
    if (num_pixel_bytes > kMaxRawBytes) {
        FL_LOG_FLEXIO_F("LPUART: strip too large -- requested %d bytes exceeds buffer cap %d (~341 RGB LEDs max). Rejecting frame.",
                        (int)num_pixel_bytes, (int)kMaxRawBytes);
        return false;
    }

    // Encode raw WS2812 bytes -> 4 UART bytes each.
    for (u32 i = 0; i < num_pixel_bytes; ++i) {
        lpuart_encode_byte(pixel_data[i], fl::span<u8, 4>{&sLpUartBuffer[i * 4u], 4});
    }
    const u32 uart_count = num_pixel_bytes * 4u;
    arm_dcache_flush_delete(sLpUartBuffer, uart_count);

    return lpuart_arm_dma(sLpUartBuffer, uart_count);
}

bool lpuart_is_done() { return sLpDmaComplete; }

void lpuart_wait() {
    const u32 start = millis();
    const u32 timeout_ms = 50;
    while (!sLpDmaComplete) {
        if ((u32)(millis() - start) >= timeout_ms) {
            if (sLpDmaChannel) {
                sLpDmaChannel->disable();
                sLpDmaChannel->clearComplete();
                sLpDmaChannel->clearError();
            }
            sLpDmaComplete = true;
            return;
        }
    }
}

void lpuart_deinit() {
    if (!sLpInitialized) return;
    lpuart_wait();
    IMXRT_LPUART_t* lp = lpuart_base(sLpPinInfo.lpuart_index);
    if (lp) {
        lp->CTRL = 0;       // disable TX
        lp->BAUD &= ~LPUART_BAUD_TDMAE;
    }
    if (sLpDmaChannel) {
        sLpDmaChannel->disable();
        sLpDmaChannel->detachInterrupt();
        sLpDmaChannel->clearInterrupt();
        DMAChannel* to_delete = sLpDmaChannel;
        sLpDmaChannel = nullptr;
        delete to_delete;  // ok bare allocation - paired with bare 'new' above
    }
    sLpInitialized = false;
    sLpDmaComplete = true;
}

void lpuart_read_diagnostics(LpuartDiagnostics* out) {
    if (!out) return;
    noInterrupts();
    *out = LpuartDiagnostics{};
    out->initialized = sLpInitialized;
    out->dma_complete = sLpDmaComplete;
    out->lpuart_index = sLpPinInfo.lpuart_index;
    out->pin = sLpPinInfo.teensy_pin;
    if (sLpInitialized) {
        IMXRT_LPUART_t* lp = lpuart_base(sLpPinInfo.lpuart_index);
        if (lp) {
            out->baud = lp->BAUD;
            out->ctrl = lp->CTRL;
            out->stat = lp->STAT;
            out->fifo = lp->FIFO;
        }
        if (sLpDmaChannel && sLpDmaChannel->TCD) {
            out->tcd_saddr = (u32)sLpDmaChannel->TCD->SADDR;
            out->tcd_daddr = (u32)sLpDmaChannel->TCD->DADDR;
            out->tcd_citer = sLpDmaChannel->TCD->CITER_ELINKNO;
            out->tcd_biter = sLpDmaChannel->TCD->BITER_ELINKNO;
            out->tcd_csr = sLpDmaChannel->TCD->CSR;
        }
    }
    interrupts();
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
