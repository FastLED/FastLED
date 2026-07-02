// AutoResearchUartDma.h — LPC845 async UART TX driver AutoResearch
// validation harness (#3453 follow-up: ISR-refillable async drivers).
//
// Device-side RPC handlers exercising `ARMHardwareUARTOutputDMA<>`
// (`platforms/arm/lpc/uart_arm_lpc_dma.h`) on real silicon: USART1 TX
// driven by DMA0 channel 3, chunks chained from the DMA completion ISR
// (lpc_dma_isr.h hub), while USART0 keeps serving the RPC console —
// the "drive data and keep printing" proof.
//
// Gated on `FL_IS_ARM_LPC_845 && FASTLED_LPC_UART_DMA`. Mutually
// exclusive with the SPI/PWM DMA harnesses at the flash-budget level
// (enforced host-side by `bash autoresearch`).
//
// Handlers (bind via `autoresearch::uart_dma::bind(remote)`):
//
//   `uartDmaStreamOnce(byte_count, byte_pattern)`
//     Kick an async stream and CPU-wait for completion. CSV
//     `"success,total_us,byte_count"` — host sanity-checks the wall
//     time against the configured baud.
//
//   `uartDmaStreamOverlap(byte_count, byte_pattern)`
//     Kick the stream and beacon-toggle a volatile counter until
//     `streamDone()`. CSV `"success,total_us,toggle_count"`. Non-zero
//     toggles while a multi-chunk stream drains is the async proof —
//     and with byte_count > 1024 it also proves the ISR chunk-chaining
//     (a stalled chain would time out).

#pragma once

#include <FastLED.h>

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_UART_DMA)

#include "fl/remote/remote.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/sstream.h"
#include "platforms/arm/lpc/uart_arm_lpc_dma.h" // ok platform headers — AutoResearch driver-specific test needs the concrete UART DMA driver

namespace autoresearch {
namespace uart_dma {

// Default TXD routing + baud. PIO0_9 is unclaimed on the LPC845-BRK
// header; the bench never reads the wire (wall-clock timing only), the
// pin just has to exist for SWM. 1 Mbaud keeps a 2048-byte stream at
// ~20 ms — inside the RPC window with margin.
#ifndef FASTLED_LPC_UART_DMA_HARNESS_TX_PIN
#define FASTLED_LPC_UART_DMA_HARNESS_TX_PIN 9
#endif
#ifndef FASTLED_LPC_UART_DMA_HARNESS_BAUD
#define FASTLED_LPC_UART_DMA_HARNESS_BAUD 1000000
#endif

using HarnessDriver = fl::ARMHardwareUARTOutputDMA<
    static_cast<fl::u8>(FASTLED_LPC_UART_DMA_HARNESS_TX_PIN),
    static_cast<fl::u32>(FASTLED_LPC_UART_DMA_HARNESS_BAUD)>;

// Stream source. 2048 bytes = two full 1024-transfer descriptors →
// at least one ISR chunk-chain per stream, which is the property under
// test. Must outlive the stream (DMA reads it in place).
#ifndef FASTLED_LPC_UART_DMA_HARNESS_BYTES
#define FASTLED_LPC_UART_DMA_HARNESS_BYTES 2048
#endif

inline fl::u8* harnessBuffer() FL_NO_EXCEPT {
    static fl::u8 buf[FASTLED_LPC_UART_DMA_HARNESS_BYTES] = {0};
    return buf;
}

inline HarnessDriver& harnessDriver() FL_NO_EXCEPT {
    static HarnessDriver drv;
    static bool inited = false;
    if (!inited) {
        drv.init();
        inited = true;
    }
    return drv;
}

inline int clampByteCount(int n) FL_NO_EXCEPT {
    if (n <= 0) return 0;
    const int cap = static_cast<int>(FASTLED_LPC_UART_DMA_HARNESS_BYTES);
    return (n > cap) ? cap : n;
}

// Bounded-spin guard (same pattern the SPI harness validated in #3580):
// a healthy 2048-byte stream at 1 Mbaud drains in ~20 ms; 4M iterations
// ≈ 2 s at 24 MHz. On timeout return a register snapshot instead of
// wedging into the watchdog.
inline bool spinUntilStreamDone(fl::sstream& diag) FL_NO_EXCEPT {
    fl::u32 spins = 0;
    while (!HarnessDriver::streamDone()) {
        if (++spins > 4000000u) {
            diag << 0 << ','
                 << DMA0->COMMON[0].ACTIVE << ','
                 << DMA0->CHANNEL[3].XFERCFG << ','
                 << DMA0->CHANNEL[3].CTLSTAT << ','
                 << USART1->STAT << ','
                 << DMA0->COMMON[0].ERRINT;
            return false;
        }
    }
    return true;
}

// CSV: "success,total_us,byte_count" (diag CSV on timeout).
inline fl::string streamOnceHandler(int byte_count, int byte_pattern) FL_NO_EXCEPT {
    const int len = clampByteCount(byte_count);
    if (len == 0) return fl::string("0,0,0");
    fl::u8* buf = harnessBuffer();
    const fl::u8 pat = static_cast<fl::u8>(byte_pattern & 0xFF);
    for (int i = 0; i < len; ++i) buf[i] = pat;

    HarnessDriver& drv = harnessDriver();
    (void)drv;

    const fl::u32 t_start = micros();
    if (!HarnessDriver::kickDmaStreamAsync(buf, static_cast<fl::u32>(len))) {
        return fl::string("0,kick_refused,0");
    }
    fl::sstream diag;
    if (!spinUntilStreamDone(diag)) return diag.str();
    const fl::u32 total_us = micros() - t_start;

    fl::sstream s;
    s << 1 << ',' << total_us << ',' << len;
    return s.str();
}

// CSV: "success,total_us,toggle_count" — the async proof.
inline fl::string streamOverlapHandler(int byte_count, int byte_pattern) FL_NO_EXCEPT {
    const int len = clampByteCount(byte_count);
    if (len == 0) return fl::string("0,0,0");
    fl::u8* buf = harnessBuffer();
    const fl::u8 pat = static_cast<fl::u8>(byte_pattern & 0xFF);
    for (int i = 0; i < len; ++i) buf[i] = pat;

    HarnessDriver& drv = harnessDriver();
    (void)drv;

    const fl::u32 t_start = micros();
    if (!HarnessDriver::kickDmaStreamAsync(buf, static_cast<fl::u32>(len))) {
        return fl::string("0,kick_refused,0");
    }

    // Volatile beacon: real memory traffic per iteration, so a non-zero
    // count is affirmative evidence the CPU ran application code while
    // the DMA/ISR machinery drove the UART.
    volatile fl::u32 toggle_count = 0;
    fl::u32 spins = 0;
    while (!HarnessDriver::streamDone()) {
        ++toggle_count;
        if (++spins > 4000000u) {
            fl::sstream d;
            d << 0 << ',' << DMA0->COMMON[0].ACTIVE << ','
              << DMA0->CHANNEL[3].CTLSTAT << ',' << USART1->STAT;
            return d.str();
        }
    }
    const fl::u32 total_us = micros() - t_start;

    fl::sstream s;
    s << 1 << ',' << total_us << ',' << toggle_count;
    return s.str();
}

inline void bind(fl::Remote& remote) FL_NO_EXCEPT {
    remote.bind("uartDmaStreamOnce",
        [](int byte_count, int byte_pattern) -> fl::string {
            return streamOnceHandler(byte_count, byte_pattern);
        });
    remote.bind("uartDmaStreamOverlap",
        [](int byte_count, int byte_pattern) -> fl::string {
            return streamOverlapHandler(byte_count, byte_pattern);
        });
}

}  // namespace uart_dma
}  // namespace autoresearch

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_UART_DMA
