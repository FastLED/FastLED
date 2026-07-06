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

#if defined(FL_LPC_UART_DMA_CLOCKLESS_TEST)
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/timing_traits.h"
#endif
#include "fl/remote/remote.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/sstream.h"
#include "platforms/arm/lpc/uart_arm_lpc_dma.h" // ok platform headers — AutoResearch driver-specific test needs the concrete UART DMA driver

namespace autoresearch {
namespace uart_dma {

#if !defined(FL_LPC_UART_DMA_CLOCKLESS_TEST)

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
    drv.init();
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

// Args as a JSON array (house RpcClient convention — single `const
// fl::json&` that is the positional-arg array). Lets the bench run over
// fbuild's Rust serial monitor instead of raw pyserial.
#endif  // !FL_LPC_UART_DMA_CLOCKLESS_TEST

inline int argInt(const fl::json& args, fl::size i, int fallback) FL_NO_EXCEPT {
    return static_cast<int>(args[i].as_int().value_or(fallback));
}

#if defined(FL_LPC_UART_DMA_CLOCKLESS_TEST)

#ifndef FL_LPC_UART_DMA_CLOCKLESS_TX_PIN
#define FL_LPC_UART_DMA_CLOCKLESS_TX_PIN 10
#endif
#ifndef FL_LPC_UART_DMA_CLOCKLESS_LEDS
#define FL_LPC_UART_DMA_CLOCKLESS_LEDS 8
#endif

inline fl::CRGB* clocklessLeds() FL_NO_EXCEPT {
    static fl::CRGB leds[FL_LPC_UART_DMA_CLOCKLESS_LEDS];
    return leds;
}

inline bool ensureClocklessFastLedRegistered() FL_NO_EXCEPT {
    static bool registered = false;
    if (!registered) {
        FastLED.addLeds<WS2812,
                        static_cast<fl::u8>(FL_LPC_UART_DMA_CLOCKLESS_TX_PIN),
                        GRB>(clocklessLeds(), FL_LPC_UART_DMA_CLOCKLESS_LEDS);
        registered = true;
    }
    return registered;
}

inline USART_Type* clocklessUartBlock() FL_NO_EXCEPT {
#if FASTLED_LPC_UART_DMA_INSTANCE == 2
    return USART2;
#else
    return USART1;
#endif
}

inline volatile fl::u32& clocklessDmaReg(fl::u32 offset) FL_NO_EXCEPT {
    return *reinterpret_cast<volatile fl::u32*>(0x50008000u + offset);  // ok reinterpret cast - DMA0 MMIO
}

inline fl::u32 clocklessUartActualBaud() FL_NO_EXCEPT {
    USART_Type* u = clocklessUartBlock();
    const fl::u32 osr = (u->OSR & 0xFu) + 1u;
    const fl::u32 brg = (u->BRG & 0xFFFFu) + 1u;
    return F_CPU / (osr * brg);
}

inline void clocklessUartDrainRx(fl::u32& errors) FL_NO_EXCEPT {
    USART_Type* u = clocklessUartBlock();
    constexpr fl::u32 kRxRdy = (1u << 0);
    constexpr fl::u32 kOverrun = (1u << 8);
    constexpr fl::u32 kFrame = (1u << 13);
    constexpr fl::u32 kParity = (1u << 14);
    while ((u->STAT & kRxRdy) != 0u) {
        errors |= (u->RXDATSTAT & (kOverrun | kFrame | kParity));
    }
    u->STAT = kOverrun | kFrame | kParity;
}

inline fl::u32 clocklessAbsDiff(fl::u32 a, fl::u32 b) FL_NO_EXCEPT {
    return a > b ? a - b : b - a;
}

inline fl::u8 clocklessPulseCount(fl::u32 timing_ns, fl::u32 pulse_ns) FL_NO_EXCEPT {
    if (pulse_ns == 0u) {
        return 0;
    }
    const fl::u8 lo = static_cast<fl::u8>(timing_ns / pulse_ns);
    const fl::u8 hi = static_cast<fl::u8>(lo + 1u);
    const fl::u32 err_lo = clocklessAbsDiff(static_cast<fl::u32>(lo) * pulse_ns, timing_ns);
    const fl::u32 err_hi = clocklessAbsDiff(static_cast<fl::u32>(hi) * pulse_ns, timing_ns);
    return err_hi < err_lo ? hi : lo;
}

inline fl::u8 clocklessBuildUartByte(fl::u8 pulses_a, fl::u8 pulses_b) FL_NO_EXCEPT {
    fl::u8 value = 0;
    for (int i = 0; i < 8; ++i) {
        const int pulse = i + 1;
        const bool wire_high =
            (pulse < 5 && pulse < static_cast<int>(pulses_a)) ||
            (pulse >= 5 && (pulse - 5) < static_cast<int>(pulses_b));
        if (!wire_high) {
            value |= static_cast<fl::u8>(1u << i);
        }
    }
    return value;
}

inline bool clocklessBuildUartLut(fl::u8 lut[4], fl::u32& baud_out) FL_NO_EXCEPT {
    const fl::ChipsetTimingConfig timing =
        fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    constexpr fl::u8 kPulsesPerBit = 5;
    const fl::u32 period_ns = timing.total_period_ns();
    if (period_ns == 0u) {
        return false;
    }

    const fl::u32 pulse_ns = period_ns / kPulsesPerBit;
    const fl::u8 pulses0 = clocklessPulseCount(timing.t1_ns, pulse_ns);
    const fl::u8 pulses1 = clocklessPulseCount(timing.t1_ns + timing.t2_ns, pulse_ns);
    if (pulses0 < 1u || pulses0 > 4u || pulses1 < 1u || pulses1 > 4u ||
        pulses0 == pulses1) {
        return false;
    }

    lut[0] = clocklessBuildUartByte(pulses0, pulses0);
    lut[1] = clocklessBuildUartByte(pulses0, pulses1);
    lut[2] = clocklessBuildUartByte(pulses1, pulses0);
    lut[3] = clocklessBuildUartByte(pulses1, pulses1);
    baud_out = static_cast<fl::u32>(
        static_cast<fl::u64>(kPulsesPerBit) * 1000000000ULL / period_ns);
    return true;
}

inline fl::size clocklessEncodeExpectedUartBytes(
        int led_count, fl::u32 rgb, fl::u8* out, fl::size out_cap,
        fl::u32& expected_baud) FL_NO_EXCEPT {
    fl::u8 lut[4] = {0, 0, 0, 0};
    if (!clocklessBuildUartLut(lut, expected_baud)) {
        return 0;
    }

    const fl::size byte_count = static_cast<fl::size>(led_count) * 3u;
    const fl::size encoded_count = byte_count * 4u;
    if (encoded_count > out_cap) {
        return 0;
    }

    const fl::u8 r = static_cast<fl::u8>((rgb >> 16) & 0xFF);
    const fl::u8 g = static_cast<fl::u8>((rgb >> 8) & 0xFF);
    const fl::u8 b = static_cast<fl::u8>(rgb & 0xFF);
    fl::size o = 0;
    for (int i = 0; i < led_count; ++i) {
        const fl::u8 grb[3] = {g, r, b};
        for (fl::u8 byte : grb) {
            out[o++] = lut[(byte >> 6) & 0x03u];
            out[o++] = lut[(byte >> 4) & 0x03u];
            out[o++] = lut[(byte >> 2) & 0x03u];
            out[o++] = lut[byte & 0x03u];
        }
    }
    return encoded_count;
}

// CSV success: "1,n_decoded,<grb byte0>,<grb byte1>,...".
// CSV failure: "0,loopback,<rx_count>,<expected_count>,<stream_done>,<stat>,<cfg>,<errors>,<expected_baud>,<actual_baud>,<mismatch_index>,<expected>,<actual>".
inline fl::string clocklessLoopbackSelfHandler(
        int led_count, fl::u32 rgb, int data_pin, int rx_pin,
        int capture_ms) FL_NO_EXCEPT {
    if (led_count != FL_LPC_UART_DMA_CLOCKLESS_LEDS ||
        data_pin != FL_LPC_UART_DMA_CLOCKLESS_TX_PIN ||
        rx_pin < 0 || rx_pin > 31 || capture_ms <= 0 || capture_ms > 200) {
        return fl::string("0,args");
    }
    (void)rx_pin;

    ensureClocklessFastLedRegistered();
    FastLED.setBrightness(255);
    fl::CRGB* leds = clocklessLeds();
    const fl::u8 r = static_cast<fl::u8>((rgb >> 16) & 0xFF);
    const fl::u8 g = static_cast<fl::u8>((rgb >> 8) & 0xFF);
    const fl::u8 b = static_cast<fl::u8>(rgb & 0xFF);
    for (int i = 0; i < led_count; ++i) {
        leds[i] = fl::CRGB(r, g, b);
    }

    constexpr fl::size kMaxEncoded =
        FL_LPC_UART_DMA_CLOCKLESS_LEDS * 3u * 4u;
    static fl::u8 expected[kMaxEncoded];
    static fl::u8 received[kMaxEncoded];
    fl::u32 expected_baud = 0;
    const fl::size expected_count = clocklessEncodeExpectedUartBytes(
        led_count, rgb, expected, kMaxEncoded, expected_baud);
    if (expected_count == 0u) {
        return fl::string("0,encode");
    }

    fl::u32 errors = 0;
    clocklessUartDrainRx(errors);
    for (fl::size i = 0; i < expected_count; ++i) {
        received[i] = 0xA5u;
    }
    fl::lpc::LpcUartDmaRuntime::prepareLoopbackRxBuffer(
        received, static_cast<fl::u32>(expected_count));

    FastLED.show();

    USART_Type* u = clocklessUartBlock();
    auto& driver = fl::BusTraits<fl::Bus::UART>::instance();
    bool stream_done = false;
    const fl::u32 start = millis();
    while ((millis() - start) < static_cast<fl::u32>(capture_ms)) {
        const auto state = driver.poll();
        stream_done = (state.state == fl::IChannelDriver::DriverState::READY);
        if (stream_done &&
            fl::lpc::LpcUartDmaRuntime::loopbackRxReceivedCount() >= expected_count) {
            break;
        }
    }

    stream_done = driver.waitForReady(1000);
    const fl::size rx_count =
        static_cast<fl::size>(fl::lpc::LpcUartDmaRuntime::loopbackRxReceivedCount());
    errors |= (u->STAT & ((1u << 8) | (1u << 13) | (1u << 14)));
    constexpr fl::u32 kLoopbackRxDmaChannel =
        (FASTLED_LPC_UART_DMA_INSTANCE == 2) ? 4u : 2u;
    const fl::u32 rx_dma_base = 0x400u + 16u * kLoopbackRxDmaChannel;
    const fl::u32 rx_dma_cfg = clocklessDmaReg(rx_dma_base + 0x0u);
    const fl::u32 rx_dma_ctlstat = clocklessDmaReg(rx_dma_base + 0x4u);
    const fl::u32 rx_dma_xfercfg = clocklessDmaReg(rx_dma_base + 0x8u);
    const fl::u32 rx_dma_active = clocklessDmaReg(0x030u);
    const fl::u32 rx_dma_busy = clocklessDmaReg(0x038u);
    const fl::u32 rx_dma_err = clocklessDmaReg(0x040u);
    const fl::u32 rx_requested_len = g_fastled_lpc_uart_loopback_rx_len;
    fl::lpc::LpcUartDmaRuntime::clearLoopbackRxBuffer();

    fl::size mismatch = expected_count;
    const fl::size compare_count =
        rx_count < expected_count ? rx_count : expected_count;
    for (fl::size i = 0; i < compare_count; ++i) {
        if (received[i] != expected[i]) {
            mismatch = i;
            break;
        }
    }

    if (rx_count != expected_count || mismatch != expected_count || errors != 0u) {
        fl::sstream d;
        d << 0 << ",loopback," << static_cast<fl::u32>(rx_count) << ','
          << static_cast<fl::u32>(expected_count) << ','
          << (stream_done ? 1 : 0) << ',' << u->STAT << ',' << u->CFG << ','
          << errors << ',' << expected_baud << ',' << clocklessUartActualBaud()
          << ',' << rx_dma_cfg << ',' << rx_dma_ctlstat << ','
          << rx_dma_xfercfg << ',' << rx_dma_active << ','
          << rx_dma_busy << ',' << rx_dma_err << ','
          << rx_requested_len << ','
          << g_fastled_lpc_uart_loopback_rx_arm_status << ','
          << g_fastled_lpc_uart_loopback_rx_arm_tx_len << ','
          << g_fastled_lpc_uart_loopback_rx_arm_xfer_readback << ','
          << g_fastled_lpc_uart_loopback_rx_arm_desc0_readback << ',';
        if (mismatch != expected_count) {
            d << static_cast<fl::u32>(mismatch) << ','
              << static_cast<fl::u32>(expected[mismatch]) << ','
              << static_cast<fl::u32>(received[mismatch]);
        } else {
            d << static_cast<fl::u32>(expected_count) << ",0,0";
        }
        return d.str();
    }

    const fl::u32 n_decoded = static_cast<fl::u32>(led_count * 3);
    fl::sstream s;
    s << 1 << ',' << n_decoded;
    for (int i = 0; i < led_count; ++i) {
        s << ',' << static_cast<fl::u32>(g)
          << ',' << static_cast<fl::u32>(r)
          << ',' << static_cast<fl::u32>(b);
    }
    return s.str();
}

inline fl::string clocklessProbeHandler(
        int step, int data_pin, int rx_pin) FL_NO_EXCEPT {
    if (data_pin != FL_LPC_UART_DMA_CLOCKLESS_TX_PIN ||
        rx_pin < 0 || rx_pin > 31) {
        return fl::string("0,args");
    }
    if (step <= 0) {
        return fl::string("1,entry");
    }
    fl::sstream s;
    s << 1 << ",uart_rx_dma," << clocklessUartBlock()->CFG;
    return s.str();
}

#endif  // FL_LPC_UART_DMA_CLOCKLESS_TEST

inline void bind(fl::Remote& remote) FL_NO_EXCEPT {
#if !defined(FL_LPC_UART_DMA_CLOCKLESS_TEST)
    remote.bind("uartDmaStreamOnce",
        [](const fl::json& args) -> fl::string {
            return streamOnceHandler(argInt(args, 0, 0), argInt(args, 1, 0));
        });
    remote.bind("uartDmaStreamOverlap",
        [](const fl::json& args) -> fl::string {
            return streamOverlapHandler(argInt(args, 0, 0), argInt(args, 1, 0));
        });
#endif
#if defined(FL_LPC_UART_DMA_CLOCKLESS_TEST)
    remote.bind("uartDmaClocklessProbe",
        [](const fl::json& args) -> fl::string {
            return clocklessProbeHandler(
                argInt(args, 0, 0),
                argInt(args, 1, FL_LPC_UART_DMA_CLOCKLESS_TX_PIN),
                argInt(args, 2, 11));
        });
    remote.bind("uartDmaClocklessLoopbackSelf",
        [](const fl::json& args) -> fl::string {
            return clocklessLoopbackSelfHandler(
                argInt(args, 0, FL_LPC_UART_DMA_CLOCKLESS_LEDS),
                static_cast<fl::u32>(argInt(args, 1, 0)),
                argInt(args, 2, FL_LPC_UART_DMA_CLOCKLESS_TX_PIN),
                argInt(args, 3, 11),
                argInt(args, 4, 30));
        });
#endif
}

}  // namespace uart_dma
}  // namespace autoresearch

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_UART_DMA
