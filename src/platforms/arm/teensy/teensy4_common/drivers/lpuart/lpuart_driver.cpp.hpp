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

// Per-pin select_input_value taken from Teensyduino HardwareSerial*.cpp
// where each Serial port's `hardware_t` records `{pin, mux_val,
// select_input_register, select_input_value}` (the 4-tuple, last field).
static constexpr LpuartPinEntry kLpuartPins[] = {
    // pin, LPUARTn, mux_offset, pad_offset, sel_input_offset, sel_input_value
    { 1,  6, 0x0CC, 0x2BC, kSelectInputLpuart6Tx, 1},  // GPIO_AD_B0_12 (Serial1)
    { 8,  4, 0x17C, 0x36C, kSelectInputLpuart4Tx, 2},  // GPIO_B1_00    (Serial2 T4.0)
    {14,  2, 0x0F0, 0x2E0, kSelectInputLpuart2Tx, 1},  // GPIO_AD_B1_02 (Serial3)
    {17,  3, 0x0EC, 0x2DC, kSelectInputLpuart3Tx, 1},  // GPIO_AD_B1_07 (Serial4 T4.0)
    {20,  8, 0x100, 0x2F0, kSelectInputLpuart8Tx, 2},  // GPIO_AD_B1_10 (Serial5)
    {24,  1, 0x0B0, 0x2A0, 0,                     0},  // Serial6 LPUART1 (no SELECT_INPUT)
    {29,  7, 0x048, 0x238, kSelectInputLpuart7Tx, 1},  // GPIO_EMC_31   (Serial7)
};
static constexpr int kNumLpuartPins =
    sizeof(kLpuartPins) / sizeof(kLpuartPins[0]);

bool lpuart_lookup_pin(u8 teensy_pin, LpuartPinInfo* info) {
    for (int i = 0; i < kNumLpuartPins; ++i) {
        if (kLpuartPins[i].teensy_pin == teensy_pin) {
            info->teensy_pin = teensy_pin;
            info->lpuart_index = kLpuartPins[i].lpuart_index;
            info->mux_alt = 2;
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

    lpuart_clock_gate_on(lpuart_index);

    // Disable TX/RX while reconfiguring.
    lp->CTRL = 0;

    // BAUD: OSR=4, SBR=4, TDMAE (TX DMA enable) at bit 23.
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
    sLpDmaChannel = new DMAChannel();
    if (!sLpDmaChannel) return false;
    return true;
}

// ============================================================================
// Public API
// ============================================================================

bool lpuart_init(const LpuartPinInfo& pin_info, u32 reset_us) {
    (void)reset_us;
    if (sLpInitialized) lpuart_deinit();

    sLpPinInfo = pin_info;

    Serial.printf("LPUART: init pin=%u idx=%u\r\n", pin_info.teensy_pin, pin_info.lpuart_index);
    lpuart_configure(pin_info.lpuart_index);
    Serial.printf("LPUART: configured\r\n");
    lpuart_pin_init(pin_info);
    Serial.printf("LPUART: pin muxed mux=0x%lx pad=0x%lx\r\n",
                  (unsigned long)*(pin_info.mux_reg),
                  (unsigned long)*(pin_info.pad_reg));

    if (!lpuart_dma_init()) {
        Serial.printf("LPUART: DMA init failed\r\n");
        return false;
    }
    Serial.printf("LPUART: DMA channel ready\r\n");

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

    sLpDmaChannel->triggerAtHardwareEvent(lpuart_dmamux_tx_source(sLpPinInfo.lpuart_index));
    sLpDmaChannel->attachInterrupt(lpuart_dma_isr);

    sLpDmaChannel->enable();
    return true;
}

bool lpuart_show_encoded(const u8* encoded, u32 num_uart_bytes) {
    if (!sLpInitialized || !sLpDmaChannel || !encoded || num_uart_bytes == 0) {
        return false;
    }
    // DIAGNOSTIC: bypass DMA, do polled writes. If LPUART is configured
    // correctly we'll see the bytes on the wire and the RX side will
    // decode SOMETHING. If LPUART is dead, the polled writes will block
    // on a never-asserted TDRE -- but we have a per-byte deadline.
    IMXRT_LPUART_t* lp = lpuart_base(sLpPinInfo.lpuart_index);
    if (!lp) return false;
    Serial.printf("LPUART: polled tx n=%lu BAUD=0x%lx CTRL=0x%lx STAT=0x%lx\r\n",
                  (unsigned long)num_uart_bytes,
                  (unsigned long)lp->BAUD, (unsigned long)lp->CTRL,
                  (unsigned long)lp->STAT);
    const u32 start = micros();
    for (u32 i = 0; i < num_uart_bytes; ++i) {
        // Wait for TDRE (bit 23 of STAT) with bounded deadline.
        const u32 byte_start = micros();
        while ((lp->STAT & (1u << 23)) == 0) {
            if ((u32)(micros() - byte_start) > 1000u) {
                Serial.printf("LPUART: TDRE never asserted at byte %lu STAT=0x%lx\r\n",
                              (unsigned long)i, (unsigned long)lp->STAT);
                sLpDmaComplete = true;
                return false;
            }
        }
        lp->DATA = encoded[i];
    }
    // Wait for transmit complete.
    while ((lp->STAT & (1u << 22)) == 0) {
        if ((u32)(micros() - start) > 500000u) break;  // 500 ms
    }
    Serial.printf("LPUART: polled tx done %luus\r\n", (unsigned long)(micros() - start));
    sLpDmaComplete = true;
    return true;
}

bool lpuart_show(const u8* pixel_data, u32 num_pixel_bytes) {
    if (!sLpInitialized || !sLpDmaChannel || !pixel_data || num_pixel_bytes == 0) {
        return false;
    }

    lpuart_wait();

    if (num_pixel_bytes > kMaxRawBytes) {
        FL_LOG_FLEXIO_F("LPUART: strip truncated -- requested %d bytes exceeds buffer cap %d (~341 RGB LEDs max). Tail LEDs will not update.",
                        (int)num_pixel_bytes, (int)kMaxRawBytes);
        num_pixel_bytes = kMaxRawBytes;
    }

    // Encode raw WS2812 bytes -> 4 UART bytes each.
    for (u32 i = 0; i < num_pixel_bytes; ++i) {
        lpuart_encode_byte(pixel_data[i], &sLpUartBuffer[i * 4u]);
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
        delete to_delete;
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
