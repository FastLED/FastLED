#pragma once
//
// examples/AutoResearch/AutoResearchEsp8266.h
//
// ESP8266 driver benches for the low-memory AutoResearch surface.
//
// uartWs2812Loopback: [nLeds, rxPin] -> json
//   Encodes a known pattern with the UART WS2812 driver's encoder, streams it
//   out of UART1 (GPIO2) and samples rxPin through GPIO_IN while the hardware
//   shifts it out, so the CPU is free to poll. Needs a jumper GPIO2 -> rxPin:
//   GPIO_IN does not see GPIO2's pad once UART1 owns it (measured: the line
//   reads low whether TX idles high or low). The decoded LED bytes and the
//   measured T0H/T1H/bit-period are returned for the host to judge.
//
// uartWs2812Show: [nLeds] -> json
//   Runs the same driver through FastLED.addLeds + FastLED.show().
//
// clocklessShow / apa102Show: [nLeds, frames] -> json
//   Run the default bit-banged WS2812 controller (GPIO4) and the APA102
//   controller (data GPIO13, clock GPIO14) through FastLED.show() and report
//   min/max frame time. A frame near twice the floor means the clockless
//   driver aborted and retried after an interrupt overran its timing window.
//

#include "platforms/esp/is_esp.h"  // ok platform headers // ok platform specific - ESP8266 bench gate

#if defined(FL_IS_ESP8266)  // ok platform specific - ESP8266-only bench file

#include "FastLED.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/json.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/wdt/watchdog.h"
#include "platforms/esp/8266/fastled_esp8266_uart.h"  // ok platform headers // ok platform specific - ESP8266 UART driver under test

// IWYU pragma: begin_keep
#include <esp8266_peri.h>     // ok platform specific - GPI, ESP8266_CLOCK for raw loopback sampling
#include <user_interface.h>   // ok platform specific - system_update_cpu_freq, wifi_* for bench setup
#include "platforms/esp/8266/i2s_ws2812_codec.h"     // ok platform headers - codec + samplesToEdges
#include "platforms/esp/8266/spi_hspi_clock.h"       // ok platform headers - HSPI clock divider
#include <core_esp8266_i2s.h>  // ok platform specific - I2S TX/RX registers for the loopback bench
#include <cont.h>             // ok platform specific - cont_get_free_stack (loop-stack headroom)
// IWYU pragma: end_keep

namespace autoresearch {
namespace esp8266 {

constexpr int kMaxLoopbackLeds = 10;  // 120 UART bytes fit the 128-byte FIFO
constexpr int kMaxEdges = kMaxLoopbackLeds * 24 * 2 + 4;

// Always inlined: the IRAM capture loop must not call back into flash.
__attribute__((always_inline)) inline fl::u32 cycles() {
    fl::u32 c;
    __asm__ __volatile__("rsr %0,ccount" : "=a"(c));  // ok platform specific - cycle-accurate edge timestamps; no core cycle counter
    return c;
}

inline fl::u8 patternByte(int i) {
    static const fl::u8 kSeed[] = {0x00, 0xFF, 0xAA, 0x55, 0x0F, 0xF0};
    if (i < static_cast<int>(sizeof(kSeed))) {
        return kSeed[i];
    }
    return static_cast<fl::u8>(i * 37 + 0x5A);
}

struct EdgeCapture {
    fl::u32 edges[kMaxEdges];
    int count;
    bool firstLevel;
};

inline EdgeCapture& edgeCapture() {
    return fl::SingletonShared<EdgeCapture>::instance();
}

// Streams `encoded` out of UART1 while timestamping every rxPin edge. Runs
// from IRAM: from flash, the cold instruction cache on the first call after
// boot stalled the loop past the first pulses (measured: 4 bits lost).
// A FIFO write stalls the CPU for ~115 ns. Filling all 120 bytes up front let
// the UART shift out 11 bits before sampling began, and writing once per
// sample missed short T0H pulses. So prime two bytes, then write one just
// after each falling edge, where the line stays low for >= 300 ns. Two edges
// fall per UART byte, so the FIFO stays ahead of the shifter.
FL_NO_INLINE static FL_IRAM void captureEdges(
        const fl::u8* encoded, int nEncoded, fl::u32 mask, fl::u32 idleCycles,
        EdgeCapture& cap) {
    bool level = (GPI & mask) != 0;  // ok platform specific - raw GPIO sampling / UART1 FIFO feed for the pin-level loopback
    cap.firstLevel = level;
    USF(fl::esp8266_uart::kUart) = encoded[0];  // ok platform specific - raw GPIO sampling / UART1 FIFO feed for the pin-level loopback
    USF(fl::esp8266_uart::kUart) = encoded[1];  // ok platform specific - raw GPIO sampling / UART1 FIFO feed for the pin-level loopback
    int fed = 2;
    fl::u32 lastEdge = cycles();
    while (cap.count < kMaxEdges) {
        const bool now = (GPI & mask) != 0;  // ok platform specific - raw GPIO sampling / UART1 FIFO feed for the pin-level loopback
        const fl::u32 t = cycles();
        if (now != level) {
            cap.edges[cap.count++] = t;
            level = now;
            lastEdge = t;
            if (!now && fed < nEncoded) {
                USF(fl::esp8266_uart::kUart) = encoded[fed++];  // ok platform specific - raw GPIO sampling / UART1 FIFO feed for the pin-level loopback
            }
        } else if (t - lastEdge > idleCycles) {
            break;
        }
    }
}

inline bool isUsableRxPin(int pin) {
    // GPIO1/3 carry the RPC console, 6-11 the flash, 2 is the TX under test.
    return pin == 0 || pin == 4 || pin == 5 || (pin >= 12 && pin <= 15);
}

inline fl::json uartWs2812Loopback(int nLeds, int rxPin) {
    if (!isUsableRxPin(rxPin)) {
        fl::json r = fl::json::object();
        r.set("success", false);
        r.set("error", "rxPin must be one of 0,4,5,12,13,14,15");
        return r;
    }
    if (nLeds < 1) nLeds = 1;
    if (nLeds > kMaxLoopbackLeds) nLeds = kMaxLoopbackLeds;
    const int nBytes = nLeds * 3;

    fl::u8 encoded[kMaxLoopbackLeds * 12];
    for (int i = 0; i < nBytes; ++i) {
        fl::esp8266_uart::encodeByte(patternByte(i), &encoded[i * 4]);
    }

    pinMode(rxPin, INPUT);
    fl::esp8266_uart::begin(FASTLED_ESP8266_UART_BAUD);
    fl::delayMicroseconds(100);  // line settles low (latch)

    const fl::u8 prevMhz = system_get_cpu_freq();  // ok platform specific - ESP8266 bring-up bench measures raw SoC behavior; no core API exposes it
    system_update_cpu_freq(160);  // ok platform specific - ESP8266 bring-up bench measures raw SoC behavior; no core API exposes it
    const fl::u32 cyclesPerUs = 160;
    const fl::u32 idleCycles = 20 * cyclesPerUs;  // 20 us of silence ends it

    EdgeCapture& cap = edgeCapture();
    cap.count = 0;
    const fl::u32 mask = 1u << rxPin;

    const fl::u32 savedPs = xt_rsil(15);  // ok platform specific - masks all IRQs for cycle-exact sampling
    captureEdges(encoded, nLeds * 12, mask, idleCycles, cap);
    xt_wsr_ps(savedPs);  // ok platform specific - restores IRQ level from xt_rsil
    system_update_cpu_freq(prevMhz);  // ok platform specific - ESP8266 bring-up bench measures raw SoC behavior; no core API exposes it

    // Pair rising/falling edges into high pulses; classify by width.
    fl::u32 t0hMin = 0xFFFFFFFF, t0hMax = 0, t1hMin = 0xFFFFFFFF, t1hMax = 0;
    fl::u32 periodMin = 0xFFFFFFFF, periodMax = 0;
    const fl::u32 splitCycles = (625 * cyclesPerUs) / 1000;  // between 312 and 937 ns
    fl::u8 decoded[kMaxLoopbackLeds * 3] = {};
    int bits = 0;
    const int start = cap.firstLevel ? 1 : 0;  // first edge must be rising
    for (int e = start; e + 1 < cap.count && bits < nBytes * 8; e += 2) {
        const fl::u32 high = cap.edges[e + 1] - cap.edges[e];
        const bool one = high > splitCycles;
        if (one) {
            decoded[bits / 8] |= static_cast<fl::u8>(0x80 >> (bits % 8));
            if (high < t1hMin) t1hMin = high;
            if (high > t1hMax) t1hMax = high;
        } else {
            if (high < t0hMin) t0hMin = high;
            if (high > t0hMax) t0hMax = high;
        }
        if (e + 2 < cap.count) {
            const fl::u32 period = cap.edges[e + 2] - cap.edges[e];
            if (period < periodMin) periodMin = period;
            if (period > periodMax) periodMax = period;
        }
        ++bits;
    }

    int byteErrors = 0;
    int firstBad = -1;
    for (int i = 0; i < nBytes; ++i) {
        if (decoded[i] != patternByte(i)) {
            ++byteErrors;
            if (firstBad < 0) firstBad = i;
        }
    }

    auto ns = [cyclesPerUs](fl::u32 c) -> fl::i64 {
        return c == 0xFFFFFFFF ? -1 : static_cast<fl::i64>(c) * 1000 / cyclesPerUs;
    };
    fl::json r = fl::json::object();
    r.set("success", bits == nBytes * 8 && byteErrors == 0);
    r.set("leds", static_cast<fl::i64>(nLeds));
    r.set("rx_pin", static_cast<fl::i64>(rxPin));
    r.set("edges", static_cast<fl::i64>(cap.count));
    r.set("bits", static_cast<fl::i64>(bits));
    r.set("byte_errors", static_cast<fl::i64>(byteErrors));
    r.set("first_bad_byte", static_cast<fl::i64>(firstBad));
    if (firstBad >= 0) {
        r.set("expected", static_cast<fl::i64>(patternByte(firstBad)));
        r.set("actual", static_cast<fl::i64>(decoded[firstBad]));
    }
    r.set("idle_low_before", !cap.firstLevel);
    r.set("t0h_ns_min", ns(t0hMin));
    r.set("t0h_ns_max", ns(t0hMax));
    r.set("t1h_ns_min", ns(t1hMin));
    r.set("t1h_ns_max", ns(t1hMax));
    r.set("period_ns_min", ns(periodMin));
    r.set("period_ns_max", ns(periodMax));
    return r;
}

// Drive the same driver through the public API (FastLED.addLeds + show) so
// the controller path runs on silicon; the pad capture above checks the
// waveform that path emits.
struct ShowState {
    CRGB leds[64];
    fl::UARTController_ESP8266<GRB> controller;
    bool added = false;
};

inline ShowState& showState() {
    return fl::SingletonShared<ShowState>::instance();
}

inline fl::json uartWs2812Show(int nLeds) {
    if (nLeds < 1) nLeds = 1;
    if (nLeds > 64) nLeds = 64;
    ShowState& st = showState();
    CRGB* leds = st.leds;
    if (!st.added) {
        FastLED.addLeds(&st.controller, leds, 64);
        st.added = true;
    }
    st.controller.setLeds(fl::span<CRGB>(leds, nLeds));
    for (int i = 0; i < nLeds; ++i) {
        leds[i] = CRGB(patternByte(i * 3), patternByte(i * 3 + 1), patternByte(i * 3 + 2));
    }
    const fl::u32 t0 = micros();
    st.controller.showLeds(255);
    const fl::u32 elapsed = micros() - t0;
    fl::json r = fl::json::object();
    r.set("success", true);
    r.set("leds", static_cast<fl::i64>(nLeds));
    r.set("show_us", static_cast<fl::i64>(elapsed));
    // 30 us of wire time per LED plus the latch is the floor.
    r.set("expected_min_us", static_cast<fl::i64>(nLeds * 30 + FASTLED_ESP8266_UART_RESET_US));
    return r;
}

constexpr int kClocklessPin = 4;
constexpr int kApaDataPin = 13;
constexpr int kApaClockPin = 14;

struct DriverShowState {
    CRGB leds[64];
    CLEDController* clockless = nullptr;
    CLEDController* apa102 = nullptr;
};

inline DriverShowState& driverShowState() {
    return fl::SingletonShared<DriverShowState>::instance();
}

// Show one controller for `frames` frames and report frame-time spread.
inline fl::json timeFrames(CLEDController* controller, int nLeds, int frames,
                           fl::i64 floorUs) {
    DriverShowState& st = driverShowState();
    if (nLeds < 1) nLeds = 1;
    if (nLeds > 64) nLeds = 64;
    if (frames < 1) frames = 1;
    if (frames > 200) frames = 200;
    controller->setLeds(fl::span<CRGB>(st.leds, nLeds));
    fl::u32 minUs = 0xFFFFFFFF, maxUs = 0;
    const fl::u32 frames0 = fl::_frame_cnt, retries0 = fl::_retry_cnt;
    int truncated = 0;  // finished faster than the wire allows: frame dropped
    int firstTruncated = -1;
    for (int f = 0; f < frames; ++f) {
        for (int i = 0; i < nLeds; ++i) {
            st.leds[i] = CHSV(static_cast<fl::u8>(f * 7 + i * 13), 255, 255);
        }
        const fl::u32 t0 = micros();
        controller->showLeds(255);
        const fl::u32 us = micros() - t0;
        if (us < minUs) minUs = us;
        if (us > maxUs) maxUs = us;
        if (static_cast<fl::i64>(us) < floorUs) {
            if (firstTruncated < 0) firstTruncated = f;
            ++truncated;
        }
    }
    fl::json r = fl::json::object();
    r.set("success", truncated == 0);
    r.set("truncated", static_cast<fl::i64>(truncated));
    r.set("first_truncated_frame", static_cast<fl::i64>(firstTruncated));
    r.set("driver_frames_ok", static_cast<fl::i64>(fl::_frame_cnt - frames0));
    r.set("driver_retries", static_cast<fl::i64>(fl::_retry_cnt - retries0));
    r.set("uptime_ms", static_cast<fl::i64>(millis()));
    r.set("wifi_opmode", static_cast<fl::i64>(wifi_get_opmode()));  // ok platform specific - ESP8266 bring-up bench measures raw SoC behavior; no core API exposes it
    r.set("leds", static_cast<fl::i64>(nLeds));
    r.set("frames", static_cast<fl::i64>(frames));
    r.set("min_us", static_cast<fl::i64>(minUs));
    r.set("max_us", static_cast<fl::i64>(maxUs));
    r.set("floor_us", floorUs);
    return r;
}

inline fl::json clocklessShow(int nLeds, int frames) {
    DriverShowState& st = driverShowState();
    if (!st.clockless) {
        st.clockless = &FastLED.addLeds<WS2812B, kClocklessPin, GRB>(st.leds, 64);
    }
    const int n = nLeds < 1 ? 1 : (nLeds > 64 ? 64 : nLeds);
    return timeFrames(st.clockless, n, frames, static_cast<fl::i64>(n) * 30);
}

inline fl::json apa102Show(int nLeds, int frames) {
    DriverShowState& st = driverShowState();
    if (!st.apa102) {
        st.apa102 = &FastLED.addLeds<APA102, kApaDataPin, kApaClockPin, BGR>(st.leds, 64);
    }
    // Start frame + 4 bytes per LED + end frame; no fixed timing floor.
    return timeFrames(st.apa102, nLeds, frames, 0);
}

// Turn the radio off so its MAC NMIs stop preempting bit-banged output.
inline fl::json wifiOff() {
    wifi_set_opmode_current(NULL_MODE);  // ok platform specific - ESP8266 bring-up bench measures raw SoC behavior; no core API exposes it
    fl::json r = fl::json::object();
    r.set("success", wifi_get_opmode() == NULL_MODE);  // ok platform specific - ESP8266 bring-up bench measures raw SoC behavior; no core API exposes it
    r.set("wifi_opmode", static_cast<fl::i64>(wifi_get_opmode()));  // ok platform specific - ESP8266 bring-up bench measures raw SoC behavior; no core API exposes it
    return r;
}

// UART1 internal loopback (UCLBE): TX feeds the RX FIFO inside the
// peripheral, so this proves the encoder, 6N1 framing and 3.2 Mbaud timing
// without a jumper. It does not see the GPIO2 pad.
inline fl::json uartWs2812InternalLoopback(fl::u32 baud) {
    namespace u = fl::esp8266_uart;
    u::begin(baud ? baud : FASTLED_ESP8266_UART_BAUD);
    // The loopback taps the line after TX inversion and RX inversion does
    // not apply to it, so drop TX inversion for the self-test: RX then sees
    // ordinary 6N1 frames. The inversion itself is one register bit.
    USC0(u::kUart) &= ~(1u << UCTXI);  // ok platform specific - UART1 internal-loopback registers; no core API
    USC0(u::kUart) |= (1u << UCLBE);  // ok platform specific - UART1 internal-loopback registers; no core API
    USC0(u::kUart) |= (1u << UCRXRST);  // ok platform specific - UART1 internal-loopback registers; no core API
    USC0(u::kUart) &= ~(1u << UCRXRST);  // ok platform specific - UART1 internal-loopback registers; no core API
    const fl::u8 pixel[3] = {0xA5, 0x0F, 0xC3};
    fl::u8 sent[12];
    for (int i = 0; i < 3; ++i) {
        u::encodeByte(pixel[i], &sent[i * 4]);
    }
    u::write(sent, sizeof(sent));
    u::flush();
    fl::delayMicroseconds(20 + 9000000 / (baud ? baud : FASTLED_ESP8266_UART_BAUD));
    const fl::u32 got = (USS(u::kUart) >> USRXC) & 0xFF;  // ok platform specific - UART1 internal-loopback registers; no core API
    fl::u32 matched = 0;
    fl::json rx = fl::json::array();
    for (fl::u32 i = 0; i < got && i < sizeof(sent); ++i) {
        const fl::u8 c = static_cast<fl::u8>(USF(u::kUart) & 0x3F);  // ok platform specific - UART1 internal-loopback registers; no core API
        rx.push_back(static_cast<fl::i64>(c));
        if (c == (sent[i] & 0x3F)) {
            ++matched;
        }
    }
    USC0(u::kUart) &= ~(1u << UCLBE);  // ok platform specific - UART1 internal-loopback registers; no core API
    USC0(u::kUart) |= (1u << UCTXI);  // ok platform specific - UART1 internal-loopback registers; no core API
    fl::json r = fl::json::object();
    r.set("sent", static_cast<fl::i64>(sizeof(sent)));
    r.set("received", static_cast<fl::i64>(got));
    r.set("matched", static_cast<fl::i64>(matched));
    r.set("rx", rx);
    r.set("success", got == sizeof(sent) && matched == sizeof(sent));
    return r;
}

// Drive GPIO2 as a plain GPIO high then low and read rxPin each time, so a
// missing jumper is told apart from a UART1 pin-mux fault. The next UART call
// re-runs esp8266_uart::begin(), which hands GPIO2 back to UART1.
inline fl::json jumperCheck(int rxPin) {
    fl::json r = fl::json::object();
    if (!isUsableRxPin(rxPin)) {
        r.set("success", false);
        r.set("error", "rxPin must be one of 0,4,5,12,13,14,15");
        return r;
    }
    pinMode(rxPin, INPUT);
    pinMode(2, OUTPUT);
    const fl::u32 mask = 1u << rxPin;
    GPOS = 1u << 2;  // ok platform specific - plain-GPIO jumper check on raw GPIO registers
    fl::delayMicroseconds(50);
    const bool seenHigh = (GPI & mask) != 0;  // ok platform specific - plain-GPIO jumper check on raw GPIO registers
    GPOC = 1u << 2;  // ok platform specific - plain-GPIO jumper check on raw GPIO registers
    fl::delayMicroseconds(50);
    const bool seenLow = (GPI & mask) == 0;  // ok platform specific - plain-GPIO jumper check on raw GPIO registers
    r.set("rx_pin", static_cast<fl::i64>(rxPin));
    r.set("rx_high_when_tx_high", seenHigh);
    r.set("rx_low_when_tx_low", seenLow);
    r.set("success", seenHigh && seenLow);
    return r;
}

// Each bind instantiates sizeable temporaries; split the bindings across
// non-inlined functions so their frames do not add up on the 4 KB loop
// stack (one combined frame overflowed it).
FL_NO_INLINE inline void bindEsp8266WatchdogRpcs(fl::Remote& remote) {
    // Watchdog bring-up: read why the chip last reset, or stop feeding the
    // watchdog (while still yielding) so the user timeout must fire.
    remote.bind("resetCause", []() -> fl::json {
        auto& wdt = fl::Watchdog::instance();
        fl::json r = fl::json::object();
        r.set("cause", fl::string(fl::resetCauseName(wdt.lastResetCause())));
        r.set("crash_count", static_cast<fl::i64>(wdt.consecutiveCrashCount()));
        // Loop-stack headroom (4 KB total); the bind/dispatch path runs close.
        r.set("cont_stack_free", static_cast<fl::i64>(cont_get_free_stack(g_pcont)));  // ok platform specific - loop-stack headroom diagnostic
        return r;
    });
    remote.bind("watchdogStall", [](int ms) -> fl::json {
        if (ms < 0) {
            fl::json e = fl::json::object();
            e.set("error", "ms must be >= 0");
            return e;
        }
        const fl::u32 start = fl::millis();
        while (fl::millis() - start < static_cast<fl::u32>(ms)) {
            fl::delay(10);  // yields, so only fl::Watchdog can fire
        }
        fl::json r = fl::json::object();
        r.set("survived_ms", static_cast<fl::i64>(fl::millis() - start));
        return r;
    });
}

FL_NO_INLINE inline void bindEsp8266UartRpcs(fl::Remote& remote) {
    // rxPin < 0 runs the jumper-free internal loopback instead, with nLeds
    // as the rate in kbaud (0 = the driver's 3.2 Mbaud).
    remote.bind("uartWs2812Loopback", [](int nLeds, int rxPin) -> fl::json {
        if (rxPin < 0) {
            // kbaud; 0 = default. USD = ESP8266_CLOCK / baud must stay >= 1.
            if (nLeds < 0 || nLeds > 80000) {
                fl::json e = fl::json::object();
                e.set("error", "kbaud must be in [0, 80000]");
                return e;
            }
            return uartWs2812InternalLoopback(static_cast<fl::u32>(nLeds) * 1000u);
        }
        // nLeds == 0 checks the jumper itself: GPIO2 as plain GPIO.
        if (nLeds == 0) {
            return jumperCheck(rxPin);
        }
        return uartWs2812Loopback(nLeds, rxPin);
    });
    remote.bind("uartWs2812Show", [](int nLeds) -> fl::json {
        return uartWs2812Show(nLeds);
    });
}

FL_NO_INLINE inline void bindEsp8266DriverRpcs(fl::Remote& remote) {
    remote.bind("wifiOff", []() -> fl::json { return wifiOff(); });
    remote.bind("clocklessShow", [](int nLeds, int frames) -> fl::json {
        return clocklessShow(nLeds, frames);
    });
    remote.bind("apa102Show", [](int nLeds, int frames) -> fl::json {
        return apa102Show(nLeds, frames);
    });
}

// I2S TX -> RX loopback (#4943 engine + #4945 capture): encode a known pattern
// with the I2S codec, stream it out of the I2S peripheral, capture it back
// through the I2S RX path, decode, and compare -- all in software on-device.
FL_NO_INLINE inline void bindEsp8266I2sRpcs(fl::Remote& remote) {
    remote.bind("i2sLoopback", [](int nBytes) -> fl::json {
        fl::json r = fl::json::object();
        if (nBytes < 1 || nBytes > 300) {
            r.set("error", "nBytes must be in [1, 300]");
            return r;
        }
        const int n = nBytes;
        static fl::u8 tx[300];
        for (int i = 0; i < n; ++i) {
            tx[i] = static_cast<fl::u8>(i * 53 + 0x1B);
        }
        // Encode as the I2S engine does: two 16-bit samples per LED byte.
        static fl::u16 samples[600];
        for (int i = 0; i < n; ++i) {
            fl::esp8266::encodeByte(tx[i], &samples[i * 2]);
        }
        const int nSamples = n * 2;
        // TX + RX on the one I2S peripheral. Only ONE side may drive the clocks
        // (driving both from the same peripheral conflicts and yields silence).
        (void)i2s_rxtxdrive_begin(true, true, true, false);
        i2s_set_rate(fl::esp8266::kI2sSlotHz);
        i2s_set_bits(16);
        for (int i = 0; i < nSamples; ++i) {
            (void)i2s_write_sample_nb(samples[i]);
        }
        static fl::u16 got[600];
        int cap = 0;
        const fl::u32 deadline = fl::millis() + 20;
        while (cap < nSamples && fl::millis() < deadline) {
            if (i2s_rx_available() > 0) {
                fl::i16 l = 0, rr = 0;
                if (i2s_read_sample(&l, &rr, false)) {
                    // The core packs (right<<16)|left; the engine's 16-bit sample
                    // may land in either half depending on the loopback wiring.
                    fl::u16 s = static_cast<fl::u16>(l);
                    if (s == 0) {
                        s = static_cast<fl::u16>(rr);
                    }
                    got[cap++] = s;
                }
            }
        }
        int firstBad = -1;
        for (int i = 0; i < n; ++i) {
            if (i * 2 + 1 >= cap) { firstBad = i; break; }
            if (fl::esp8266::decodeByte(got[i * 2], got[i * 2 + 1]) != tx[i]) {
                firstBad = i;
                break;
            }
        }
        r.set("success", firstBad < 0);
        r.set("sent", static_cast<fl::i64>(n));
        r.set("captured_samples", static_cast<fl::i64>(cap));
        r.set("first_bad", static_cast<fl::i64>(firstBad));
        r.set("s0", static_cast<fl::i64>(samples[0]));
        r.set("g0", static_cast<fl::i64>(got[0]));
        r.set("g1", static_cast<fl::i64>(got[1]));
        r.set("dec0", static_cast<fl::i64>(fl::esp8266::decodeByte(got[0], got[1])));
        return r;
    });
}

// HSPI (SPI1) master loopback (#4942): drive bytes out of MOSI (GPIO13) and
// read them back from MISO (GPIO12), which the ESP8266 HSPI does through the
// SAME register -- after SPI1CMD/SPIBUSY clears, `SPI1W0 & 0xFF` holds the byte
// clocked in on MISO. Jumper D7 (GPIO13) -> D6 (GPIO12) and every byte comes
// back. This captures the real hardware SPI output byte-exactly.
FL_NO_INLINE inline void bindEsp8266SpiRpcs(fl::Remote& remote) {
    remote.bind("spiLoopback", [](int nBytes) -> fl::json {
        fl::json r = fl::json::object();
        if (nBytes < 1 || nBytes > 64) {
            r.set("error", "nBytes must be in [1, 64]");
            return r;
        }
        const int n = nBytes;
        // Pads + SPI1 config, mirroring fastspi_esp8266_hspi.h (MOSI=13, SCK=14).
        GPC(13) = (GPC(13) & (0xFUL << GPCI));
        GPC(14) = (GPC(14) & (0xFUL << GPCI));
        GPEC = (1UL << 13) | (1UL << 14);
        GPF13 = GPFFS(GPFFS_BUS(13));
        GPF14 = GPFFS(GPFFS_BUS(14));
        while (SPI1CMD & SPIBUSY) {
        }
        SPI1C = 0;
        SPI1C1 = 0;
        GPMUX &= ~(1UL << 9);
        SPI1CLK = fl::esp8266::hspiClockFor(1000000).reg_value;  // 1 MHz
        SPI1U = SPIUMOSI | SPIUDUPLEX | SPIUSSE;
        SPI1U1 = (7UL << SPILMOSI) | (7UL << SPILMISO);

        int firstBad = -1;
        fl::u8 lastGot = 0;
        for (int i = 0; i < n; ++i) {
            const fl::u8 b = static_cast<fl::u8>(i * 37 + 0x1B);
            while (SPI1CMD & SPIBUSY) {
            }
            SPI1W0 = b;
            SPI1CMD |= SPIBUSY;
            while (SPI1CMD & SPIBUSY) {
            }
            lastGot = static_cast<fl::u8>(SPI1W0 & 0xFF);
            if (lastGot != b && firstBad < 0) {
                firstBad = i;
            }
        }
        r.set("success", firstBad < 0);
        r.set("sent", static_cast<fl::i64>(n));
        r.set("first_bad", static_cast<fl::i64>(firstBad));
        r.set("last_got", static_cast<fl::i64>(lastGot));
        return r;
    });
}

inline void bindEsp8266Benches(fl::Remote& remote) {
    bindEsp8266WatchdogRpcs(remote);
    bindEsp8266UartRpcs(remote);
    bindEsp8266DriverRpcs(remote);
    bindEsp8266I2sRpcs(remote);
    bindEsp8266SpiRpcs(remote);
}

}  // namespace esp8266
}  // namespace autoresearch

#endif  // FL_IS_ESP8266
