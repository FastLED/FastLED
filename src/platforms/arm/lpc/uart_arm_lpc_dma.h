#pragma once

/// @file platforms/arm/lpc/uart_arm_lpc_dma.h
/// @brief LPC845 async UART TX driver — DMA-fed, ISR-refilled, console-safe.
///
/// Streams arbitrary-length byte frames out of USART1 (or 2) TX while the
/// main thread keeps running. USART0 is untouched — it remains the
/// FastLED console / AutoResearch RPC port, which is the whole point:
/// the LPC845 has five USARTs, each with a dedicated DMA request channel
/// (UM11029 §16.3.3 Table 296: USART0_TX=ch1, USART1_TX=ch3, USART2_TX=ch5,
/// USART3_TX=ch7, USART4_TX=ch9), so one can drive LEDs/data while
/// another prints.
///
/// Architecture mirrors `ARMHardwareSPIOutputDMA<>`'s streaming path
/// (#3453, silicon-validated in #3580): DMA0 channel paced by the USART
/// TXRDY request, one-shot descriptors from the shared table
/// (lpc_dma_descriptor_table.h), ping-pong chunk refill from the DMA0
/// completion ISR (lpc_dma_isr.h hub, `FASTLED_LPC_DMA_ISR=1` builds).
/// Unlike SPI there is NO encode widening — UART TXDAT takes the byte
/// directly, so DMA reads the caller's buffer in place (width=8, no
/// staging copy, no per-chunk CPU work beyond the descriptor re-arm).
/// One descriptor covers at most 1024 transfers (XFERCOUNT is 10-bit),
/// so frames longer than 1024 bytes are chained by the ISR.
///
/// The driver owns its full peripheral plumbing (the #3580 lesson):
/// SYSAHBCLKCTRL0 clock, PRESETCTRL0 reset release, FCLKSEL function
/// clock, SWM TXD pin routing, BRG/OSR baud setup.
///
/// vvv usage
/// @code
///     fl::ARMHardwareUARTOutputDMA<9 /*TXD pin*/, 3000000 /*baud*/> uart;
///     uart.init();
///     uart.kickDmaStreamAsync(buf, len);   // returns immediately
///     // ... main thread free ...
///     while (!uart.streamDone()) {}        // or poll from loop()
/// @endcode
/// ^^^ usage

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/is_arm.h"

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_UART_DMA)

// IWYU pragma: begin_keep
#include <LPC845.h>
// IWYU pragma: end_keep

#include "platforms/arm/lpc/lpc_dma_descriptor_table.h"
#include "platforms/arm/lpc/lpc_dma_isr.h"

#if defined(FL_LPC_UART_DMA_LOOPBACK_TEST)
extern "C" {
extern uint8_t* g_fastled_lpc_uart_loopback_rx_dst;
extern uint32_t g_fastled_lpc_uart_loopback_rx_len;
extern uint32_t g_fastled_lpc_uart_loopback_rx_arm_status;
extern uint32_t g_fastled_lpc_uart_loopback_rx_arm_tx_len;
extern uint32_t g_fastled_lpc_uart_loopback_rx_arm_xfer_readback;
extern uint32_t g_fastled_lpc_uart_loopback_rx_arm_desc0_readback;
}
#endif

namespace fl {

// USART instance selector. Only USART1/USART2 are wired here — USART0 is
// reserved for the console, USART3/4 share their IRQ slots with pin
// interrupts and their FCLKSEL/clock plumbing differs (no FRG output on
// some muxes); add them when a board actually needs four lanes.
#ifndef FASTLED_LPC_UART_DMA_INSTANCE
#define FASTLED_LPC_UART_DMA_INSTANCE 1
#endif

/// @brief LPC845 USARTn TX + DMA0 async output.
/// @tparam _TX_PIN  PIO0_n index the SWM routes Un_TXD onto.
/// @tparam _BAUD    Wire baud rate (main_clk 24 MHz / 16x OSR ⇒ max
///                  practical ~1.5 Mbaud at integer BRG; 3 Mbaud needs
///                  OSR=8 — the driver picks OSR/BRG for minimal error).
namespace lpc {

class LpcUartDmaRuntime {
    static inline USART_Type* uartBlock() FL_NO_EXCEPT {
#if FASTLED_LPC_UART_DMA_INSTANCE == 2
        return USART2;
#else
        return USART1;
#endif
    }

    static constexpr u32 kDmaChannel =
        (FASTLED_LPC_UART_DMA_INSTANCE == 2) ? 5u : 3u;
#if defined(FL_LPC_UART_DMA_LOOPBACK_TEST)
    // UM11029 Table 296: USART1_RX is DMA channel 2; USART2_RX is channel 4.
    static constexpr u32 kLoopbackRxDmaChannel =
        (FASTLED_LPC_UART_DMA_INSTANCE == 2) ? 4u : 2u;
#endif
    static constexpr u32 kMaxChunk = 1024u;
    static constexpr u32 kUsartCfgLoop = (1UL << 15);
    static constexpr u32 kUsartCfgRxPol = (1UL << 22);
    static constexpr u32 kUsartCfgTxPol = (1UL << 23);
    static constexpr u32 kSwmPinMask = 0xFFu;
    static constexpr u32 swmMask(u32 shift) FL_NO_EXCEPT {
        return kSwmPinMask << shift;
    }
    static constexpr u32 swmValue(u8 pin, u32 shift) FL_NO_EXCEPT {
        return (static_cast<u32>(pin) & kSwmPinMask) << shift;
    }
#if defined(FL_LPC_UART_DMA_LOOPBACK_TEST)
    static constexpr u32 kDmaBase = 0x50008000u;
    static volatile u32& dmaReg(u32 offset) FL_NO_EXCEPT {
        return *reinterpret_cast<volatile u32*>(kDmaBase + offset);  // ok reinterpret cast - DMA0 MMIO
    }
    static constexpr u32 dmaChCfg(u32 ch) FL_NO_EXCEPT { return 0x400u + 16u * ch; }
    static constexpr u32 dmaChCtlStat(u32 ch) FL_NO_EXCEPT { return 0x404u + 16u * ch; }
    static constexpr u32 dmaChXferCfg(u32 ch) FL_NO_EXCEPT { return 0x408u + 16u * ch; }
#endif

public:
    struct StreamState {
        const u8* volatile src = nullptr;
        volatile u32 remaining = 0;
        volatile bool running = false;
    };

#if defined(FL_LPC_UART_DMA_LOOPBACK_TEST)
    static void prepareLoopbackRxBuffer(u8* dst, u32 len) FL_NO_EXCEPT {
        g_fastled_lpc_uart_loopback_rx_dst = dst;
        g_fastled_lpc_uart_loopback_rx_len = len;
        g_fastled_lpc_uart_loopback_rx_arm_status = 0;
        g_fastled_lpc_uart_loopback_rx_arm_tx_len = 0;
        g_fastled_lpc_uart_loopback_rx_arm_xfer_readback = 0;
        g_fastled_lpc_uart_loopback_rx_arm_desc0_readback = 0;
    }

    static u32 loopbackRxReceivedCount() FL_NO_EXCEPT {
        const u32 len = g_fastled_lpc_uart_loopback_rx_len;
        if (g_fastled_lpc_uart_loopback_rx_dst == nullptr || len == 0u) {
            return 0;
        }
        const u32 mask = (1UL << kLoopbackRxDmaChannel);
        if ((dmaReg(0x058u) & mask) != 0u) {  // INTA0: SETINTA completion.
            return len;
        }
        const u32 xfer = dmaReg(dmaChXferCfg(kLoopbackRxDmaChannel));
        if ((xfer & 1u) == 0u) {
            return len;
        }
        const u32 residual = (xfer >> 16) & 0x3FFu;
        if (residual >= len) {
            return 0;
        }
        return len - 1u - residual;
    }

    static void clearLoopbackRxBuffer() FL_NO_EXCEPT {
        const u32 mask = (1UL << kLoopbackRxDmaChannel);
        dmaReg(0x028u) = mask;  // ENABLECLR0
        dmaReg(0x078u) = mask;  // ABORT0
        dmaReg(0x058u) = mask;  // INTA0 W1C
        g_fastled_lpc_uart_loopback_rx_dst = nullptr;
        g_fastled_lpc_uart_loopback_rx_len = 0;
    }
#endif

    static void init(u8 txPin, u32 baud, bool invertTx) FL_NO_EXCEPT {
#if FASTLED_LPC_UART_DMA_INSTANCE == 2
        SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_UART2_MASK |
                                  SYSCON_SYSAHBCLKCTRL0_SWM_MASK;
        SYSCON->PRESETCTRL0 |= SYSCON_PRESETCTRL0_UART2_RST_N_MASK;
        SYSCON->FCLKSEL[2] = SYSCON_FCLKSEL_SEL(0x1u);
#if defined(FL_LPC_UART_DMA_CLOCKLESS_RX_PIN)
        // UM11029 Table 182: PINASSIGN2[23:16]=U2_TXD_O,
        // PINASSIGN2[31:24]=U2_RXD_I.
        SWM0->PINASSIGN.PINASSIGN2 =
            (SWM0->PINASSIGN.PINASSIGN2 &
             ~(swmMask(16u) | swmMask(24u))) |
            swmValue(txPin, 16u) |
            swmValue(static_cast<u8>(FL_LPC_UART_DMA_CLOCKLESS_RX_PIN), 24u);
#else
        SWM0->PINASSIGN.PINASSIGN2 =
            (SWM0->PINASSIGN.PINASSIGN2 & ~SWM_PINASSIGN2_U2_TXD_O_MASK) |
            SWM_PINASSIGN2_U2_TXD_O(txPin);
#endif
#else
        SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_UART1_MASK |
                                  SYSCON_SYSAHBCLKCTRL0_SWM_MASK;
        SYSCON->PRESETCTRL0 |= SYSCON_PRESETCTRL0_UART1_RST_N_MASK;
        SYSCON->FCLKSEL[1] = SYSCON_FCLKSEL_SEL(0x1u);
#if defined(FL_LPC_UART_DMA_CLOCKLESS_RX_PIN)
        // UM11029 Table 181: PINASSIGN1[15:8]=U1_TXD_O,
        // PINASSIGN1[23:16]=U1_RXD_I.
        SWM0->PINASSIGN.PINASSIGN1 =
            (SWM0->PINASSIGN.PINASSIGN1 &
             ~(swmMask(8u) | swmMask(16u))) |
            swmValue(txPin, 8u) |
            swmValue(static_cast<u8>(FL_LPC_UART_DMA_CLOCKLESS_RX_PIN), 16u);
#else
        SWM0->PINASSIGN.PINASSIGN1 =
            (SWM0->PINASSIGN.PINASSIGN1 & ~SWM_PINASSIGN1_U1_TXD_O_MASK) |
            SWM_PINASSIGN1_U1_TXD_O(txPin);
#endif
#endif

        USART_Type* u = uartBlock();
        u->CFG = 0;
        u32 bestOsr = 15u;
        u32 bestBrg = 0u;
        u32 bestErr = 0xFFFFFFFFu;
        for (u32 osr = 15u; osr >= 4u; --osr) {
            const u32 div = (osr + 1u) * baud;
            u32 brg = (F_CPU + div / 2u) / div;
            if (brg == 0u) {
                brg = 1u;
            }
            const u32 actual = F_CPU / ((osr + 1u) * brg);
            const u32 err = actual > baud ? actual - baud : baud - actual;
            if (err < bestErr) {
                bestErr = err;
                bestOsr = osr;
                bestBrg = brg - 1u;
            }
        }
        u->OSR = bestOsr & USART_OSR_OSRVAL_MASK;
        u->BRG = bestBrg & USART_BRG_BRGVAL_MASK;

        u32 cfg = USART_CFG_ENABLE_MASK | USART_CFG_DATALEN(0x1u);
        if (invertTx) {
            // UM11029 Table 324: USART CFG bit 23 is TXPOL.
            cfg |= kUsartCfgTxPol;
        }
#if defined(FL_LPC_UART_DMA_LOOPBACK_TEST)
        // AutoResearch-only: when an RX pin is provided, verify through the
        // board-level jumper. Otherwise fall back to UM11029 Table 324 LOOP.
#if !defined(FL_LPC_UART_DMA_CLOCKLESS_RX_PIN)
        cfg |= kUsartCfgLoop;
#endif
        // The clockless UART path intentionally inverts TX, so the verifier
        // inverts RX too whether the signal arrives by pin or internal LOOP.
        if (invertTx) {
            cfg |= kUsartCfgRxPol;
        }
#endif
        u->CFG = cfg;

        SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_DMA_MASK;
        DMA0->CTRL = 1UL;
        fl::lpc::ensureDmaSramBase();
        DMA0->CHANNEL[kDmaChannel].CFG = (1UL << 0);
        DMA0->COMMON[0].ENABLESET = (1UL << kDmaChannel);
    }

    static bool done() FL_NO_EXCEPT {
        return (DMA0->COMMON[0].ACTIVE & (1UL << kDmaChannel)) == 0u;
    }

    static bool armNextChunk() FL_NO_EXCEPT {
        StreamState& stream = streamState();
        const u32 n = stream.remaining < kMaxChunk
                          ? static_cast<u32>(stream.remaining)
                          : kMaxChunk;
        if (n == 0u) {
            return false;
        }
        const u32 srambase = DMA0->SRAMBASE;
        if (srambase == 0u) {
            return false;
        }

        const u8* base = stream.src;
        stream.src = base + n;
        stream.remaining = stream.remaining - n;

        volatile u32* descr =
            reinterpret_cast<volatile u32*>(srambase + 16u * kDmaChannel);  // ok reinterpret cast - raw DMA descriptor address
        descr[0] = 0u;
        descr[1] = reinterpret_cast<u32>(&base[n - 1u]);  // ok reinterpret cast - DMA descriptor source end address
        descr[2] = reinterpret_cast<u32>(&uartBlock()->TXDAT);  // ok reinterpret cast - DMA descriptor destination address
        descr[3] = 0u;

        DMA0->CHANNEL[kDmaChannel].XFERCFG =
            (1UL << 0) | (1UL << 2) | (1UL << 4) |
            (0UL << 8) |
            (1UL << 12) |
            (0UL << 14) |
            (((n - 1u) & 0x3FFu) << 16);
        DMA0->COMMON[0].SETVALID = (1UL << kDmaChannel);
        return true;
    }

    static void onDmaChunkDone(void*) FL_NO_EXCEPT {
        if (!armNextChunk()) {
            DMA0->COMMON[0].INTENCLR = (1UL << kDmaChannel);
            streamState().running = false;
        }
    }

    static bool kickDmaStreamAsync(const u8* src, u32 len) FL_NO_EXCEPT {
        if (len == 0u) {
            return true;
        }
#if !FASTLED_LPC_DMA_ISR
        if (len > kMaxChunk) {
            return false;
        }
#endif
        StreamState& stream = streamState();
        while (!done()) {
            __asm volatile("wfi" ::: "memory");
        }
        while (stream.running) {
            __asm volatile("wfi" ::: "memory");
        }
        fl::lpc::registerDmaChannelIsr(kDmaChannel, &onDmaChunkDone, nullptr);
#if defined(FL_LPC_UART_DMA_LOOPBACK_TEST)
        armLoopbackRxIfRequested(len);
#else
        (void)len;
#endif
        stream.src = src;
        stream.remaining = len;
        stream.running = true;
        if (!armNextChunk()) {
            stream.running = false;
            return false;
        }
        return true;
    }

    static bool streamDone() FL_NO_EXCEPT {
        if (streamState().running || !done()) {
            return false;
        }
        return (uartBlock()->STAT & USART_STAT_TXIDLE_MASK) != 0u;
    }

private:
#if defined(FL_LPC_UART_DMA_LOOPBACK_TEST)
    static void armLoopbackRxIfRequested(u32 txLen) FL_NO_EXCEPT {
        u8* const dst = g_fastled_lpc_uart_loopback_rx_dst;
        const u32 len = g_fastled_lpc_uart_loopback_rx_len;
        g_fastled_lpc_uart_loopback_rx_arm_tx_len = txLen;
        if (dst == nullptr || len == 0u) {
            g_fastled_lpc_uart_loopback_rx_arm_status = 1;
            return;
        }
        if (len > txLen) {
            g_fastled_lpc_uart_loopback_rx_arm_status = 2;
            return;
        }
        if (len > kMaxChunk) {
            g_fastled_lpc_uart_loopback_rx_arm_status = 3;
            return;
        }
        const u32 srambase = DMA0->SRAMBASE;
        if (srambase == 0u) {
            g_fastled_lpc_uart_loopback_rx_arm_status = 4;
            return;
        }

        USART_Type* u = uartBlock();
        while ((u->STAT & (1u << 0)) != 0u) {
            (void)u->RXDATSTAT;
        }
        u->STAT = (1u << 8) | (1u << 13) | (1u << 14);

        const u32 mask = (1UL << kLoopbackRxDmaChannel);
        dmaReg(0x028u) = mask;  // ENABLECLR0
        while ((dmaReg(0x038u) & mask) != 0u) {
        }
        dmaReg(0x078u) = mask;  // ABORT0
        dmaReg(0x058u) = mask;  // INTA0 W1C
        dmaReg(dmaChXferCfg(kLoopbackRxDmaChannel)) = 0u;

        volatile u32* descr =
            reinterpret_cast<volatile u32*>(srambase + 16u * kLoopbackRxDmaChannel);  // ok reinterpret cast - raw DMA descriptor address
        const u32 xfercfg =
            (1UL << 0) |        // CFGVALID
            (1UL << 2) |        // SWTRIG: request paces after channel is triggered
            (1UL << 4) |        // SETINTA: AutoResearch completion probe
            (0UL << 8) |        // WIDTH = 8-bit
            (0UL << 12) |       // SRCINC = none
            (1UL << 14) |       // DSTINC = +1 byte
            (((len - 1u) & 0x3FFu) << 16);
        // UM11029 Table 299: the initial single-buffer descriptor's +0 word
        // is reserved; the live transfer config is DMA CHANNEL[n].XFERCFG.
        descr[0] = 0u;
        descr[1] = reinterpret_cast<u32>(&u->RXDAT);  // ok reinterpret cast - USART RX data register
        descr[2] = reinterpret_cast<u32>(&dst[len - 1u]);  // ok reinterpret cast - DMA descriptor destination end
        descr[3] = 0u;

        dmaReg(dmaChCfg(kLoopbackRxDmaChannel)) = (1UL << 0);  // PERIPHREQEN
        dmaReg(0x020u) = mask;  // ENABLESET0
        dmaReg(dmaChXferCfg(kLoopbackRxDmaChannel)) = xfercfg;
        dmaReg(0x068u) = mask;  // SETVALID0
        g_fastled_lpc_uart_loopback_rx_arm_xfer_readback =
            dmaReg(dmaChXferCfg(kLoopbackRxDmaChannel));
        g_fastled_lpc_uart_loopback_rx_arm_desc0_readback = descr[0];
        g_fastled_lpc_uart_loopback_rx_arm_status = 5;
    }
#endif

    static StreamState& streamState() FL_NO_EXCEPT {
        static StreamState state;
        return state;
    }
};

}  // namespace lpc

template <u8 _TX_PIN, u32 _BAUD>
class ARMHardwareUARTOutputDMA {
    static inline USART_Type* uart_block() __attribute__((always_inline)) {
#if FASTLED_LPC_UART_DMA_INSTANCE == 2
        return USART2;
#else
        return USART1;
#endif
    }

    // UM11029 §16.3.3 Table 296 — USARTn_TX request lines.
    static constexpr u32 kDmaChannel =
        (FASTLED_LPC_UART_DMA_INSTANCE == 2) ? 5u : 3u;

    // XFERCOUNT is 10-bit → 1024 transfers per one-shot descriptor.
    static constexpr u32 kMaxChunk = 1024u;

public:
    void init() FL_NO_EXCEPT {
        // ----- SYSCON clock + reset + function clock + SWM ---------------
        // Reading an unclocked LPC peripheral stalls the AHB bus forever
        // (#3580); the driver owns all of its plumbing.
#if FASTLED_LPC_UART_DMA_INSTANCE == 2
        SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_UART2_MASK |
                                  SYSCON_SYSAHBCLKCTRL0_SWM_MASK;
        SYSCON->PRESETCTRL0 |= SYSCON_PRESETCTRL0_UART2_RST_N_MASK;
        SYSCON->FCLKSEL[2] = SYSCON_FCLKSEL_SEL(0x1u);  // main_clk
        SWM0->PINASSIGN.PINASSIGN2 =
            (SWM0->PINASSIGN.PINASSIGN2 & ~SWM_PINASSIGN2_U2_TXD_O_MASK) |
            SWM_PINASSIGN2_U2_TXD_O(_TX_PIN);
#else
        SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_UART1_MASK |
                                  SYSCON_SYSAHBCLKCTRL0_SWM_MASK;
        SYSCON->PRESETCTRL0 |= SYSCON_PRESETCTRL0_UART1_RST_N_MASK;
        SYSCON->FCLKSEL[1] = SYSCON_FCLKSEL_SEL(0x1u);  // main_clk
        SWM0->PINASSIGN.PINASSIGN1 =
            (SWM0->PINASSIGN.PINASSIGN1 & ~SWM_PINASSIGN1_U1_TXD_O_MASK) |
            SWM_PINASSIGN1_U1_TXD_O(_TX_PIN);
#endif

        // ----- Baud: pick OSR (5..16x) + integer BRG for minimal error ---
        // baud = main_clk / ((OSR+1) * (BRG+1)); main_clk = F_CPU (24 MHz).
        USART_Type* u = uart_block();
        u->CFG = 0;  // disable during reconfig
        u32 best_osr = 15u, best_brg = 0u;
        u32 best_err = 0xFFFFFFFFu;
        for (u32 osr = 15u; osr >= 4u; --osr) {
            const u32 div = (osr + 1u) * _BAUD;
            u32 brg = (F_CPU + div / 2u) / div;  // rounded divisor
            if (brg == 0u) brg = 1u;
            const u32 actual = F_CPU / ((osr + 1u) * brg);
            const u32 err = actual > _BAUD ? actual - _BAUD : _BAUD - actual;
            if (err < best_err) {
                best_err = err;
                best_osr = osr;
                best_brg = brg - 1u;
            }
        }
        u->OSR = best_osr & USART_OSR_OSRVAL_MASK;
        u->BRG = best_brg & USART_BRG_BRGVAL_MASK;

        // 8N1, enabled. DATALEN=0x1 is 8-bit per the vendor field encoding.
        u->CFG = USART_CFG_ENABLE_MASK | USART_CFG_DATALEN(0x1u);

        // ----- DMA0 channel ----------------------------------------------
        SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_DMA_MASK;
        DMA0->CTRL = 1UL;
        fl::lpc::ensureDmaSramBase();
        DMA0->CHANNEL[kDmaChannel].CFG = (1UL << 0);  // PERIPHREQEN
        DMA0->COMMON[0].ENABLESET = (1UL << kDmaChannel);
    }

    /// DMA drained for the current descriptor (not the full stream).
    static bool done() FL_NO_EXCEPT {
        return (DMA0->COMMON[0].ACTIVE & (1UL << kDmaChannel)) == 0u;
    }

    // ----- ISR-chained streaming ------------------------------------------

    struct StreamState {
        const u8* volatile src = nullptr;
        volatile u32 remaining = 0;
        volatile bool running = false;
    };

    static StreamState sStream;

    /// Arm the next ≤1024-byte chunk directly from the caller's buffer
    /// (no staging copy — UART TXDAT takes bytes as-is).
    static bool armNextChunk() FL_NO_EXCEPT {
        const u32 n = sStream.remaining < kMaxChunk
                          ? static_cast<u32>(sStream.remaining)
                          : kMaxChunk;
        if (n == 0u) return false;
        const u32 srambase = DMA0->SRAMBASE;
        if (srambase == 0u) return false;  // kick before init()

        const u8* base = sStream.src;
        sStream.src = base + n;
        sStream.remaining = sStream.remaining - n;

        volatile u32* descr =
            reinterpret_cast<volatile u32*>(srambase + 16u * kDmaChannel);  // ok reinterpret cast - SRAMBASE + channel offset = raw descriptor table address
        descr[0] = 0u;
        descr[1] = reinterpret_cast<u32>(&base[n - 1u]);  // ok reinterpret cast - LPC DMA descriptor requires raw address
        descr[2] = reinterpret_cast<u32>(&uart_block()->TXDAT);  // ok reinterpret cast - LPC DMA descriptor requires raw address
        descr[3] = 0u;

        // XFERCFG: CFGVALID | SWTRIG (see #3580 — requests only PACE a
        // channel; software must TRIGGER it) | SETINTA | width=8-bit |
        // SRCINC=+1 | DSTINC=0 | count-1.
        DMA0->CHANNEL[kDmaChannel].XFERCFG =
            (1UL << 0) | (1UL << 2) | (1UL << 4) |
            (0UL << 8) |           // WIDTH = 8-bit
            (1UL << 12) |          // SRCINC = +1 byte
            (0UL << 14) |          // DSTINC = none
            (((n - 1u) & 0x3FFu) << 16);
        DMA0->COMMON[0].SETVALID = (1UL << kDmaChannel);
        return true;
    }

    static void onDmaChunkDone(void*) FL_NO_EXCEPT {
        if (!armNextChunk()) {
            // Quiesce the channel IRQ at stream end — same hygiene as the
            // SPI sibling, where a residual completion firing after the
            // stream HardFaulted the RPC reply path (silicon, 2026-07-02).
            DMA0->COMMON[0].INTENCLR = (1UL << kDmaChannel);
            sStream.running = false;
        }
    }

    /// Stream `len` bytes from `src` without blocking. The buffer must
    /// stay alive/unmodified until streamDone(). Returns false when the
    /// ISR hub is compiled out (`FASTLED_LPC_DMA_ISR=0`) and the frame
    /// exceeds one descriptor — callers then chunk synchronously.
    static bool kickDmaStreamAsync(const u8* src, u32 len) FL_NO_EXCEPT {
        if (len == 0u) return true;
#if !FASTLED_LPC_DMA_ISR
        if (len > kMaxChunk) return false;
#endif
        while (!done()) {
            __asm volatile("wfi" ::: "memory");
        }
        while (sStream.running) {
            __asm volatile("wfi" ::: "memory");
        }
        fl::lpc::registerDmaChannelIsr(kDmaChannel, &onDmaChunkDone, nullptr);
        sStream.src = src;
        sStream.remaining = len;
        sStream.running = true;
        if (!armNextChunk()) {
            sStream.running = false;
            return false;
        }
        return true;
    }

    /// Full stream drained (all chained chunks) and the last byte left
    /// the transmit path.
    static bool streamDone() FL_NO_EXCEPT {
        if (sStream.running || !done()) return false;
        return (uart_block()->STAT & USART_STAT_TXIDLE_MASK) != 0u;
    }
};

template <u8 T, u32 B>
typename ARMHardwareUARTOutputDMA<T, B>::StreamState
    ARMHardwareUARTOutputDMA<T, B>::sStream;

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_UART_DMA
