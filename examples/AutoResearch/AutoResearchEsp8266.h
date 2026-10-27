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
// apa102ShowMhz: [nLeds, mhz] -> json (20 frames)
//   APA102 on native HSPI (GPIO13/14) at 6/10/20/40/80 MHz. The driver
//   programs SPI1CLK in init() only, so each rate gets its own controller and
//   the bench re-inits before timing. Reports achievable_hz (decoded from the
//   SPI1CLK the driver just wrote), expected_us from it, and the measured
//   min_us; `success` is the frame time landing in
//   [expected_us, expected_us + 400 us]. The 400 us covers the ~250 us
//   per-show cost plus jitter, so this band only rejects a bus running at half
//   the programmed rate where the wire time is large next to that cost (6 MHz
//   here: 762 us of wire time vs a 781 us bound). At 20 MHz and above the wire
//   time shrinks under the slack and the band cannot see a halved bus -- there
//   `achievable_hz` (read back from SPI1CLK) is the evidence that the divider
//   took effect, not `success`.
//
// apa102ShowBitbang: [nLeds, frames] -> json
//   The same chipset on GPIO4/GPIO16, i.e. the generic software SPI fallback,
//   for a same-run comparison against apa102Show. generic_bitbang is false in a
//   FASTLED_ALL_PINS_HARDWARE_SPI build, where that pair resolves to the legacy
//   Arduino-SPI shim and is therefore not a bit-bang baseline.
//
// The benches name the HSPI pair as FL_PIN_SPI_DATA_1 / FL_PIN_SPI_CLOCK_1, the
// same key the native-HSPI specialization uses, so D1/NodeMCU pin orders
// (7/5) select the same GPIO13/14 pads instead of quietly falling back to the
// bit-bang. The sense-pin arguments are always raw GPIO numbers.
//
// apa102HspiLoopback: [khz, mosiPin, sckPin] -> json
//   Pin-level HSPI check at 25 or 50 kHz. GPIO_IN does not see GPIO13/14 while
//   HSPI owns the pads, so jumper D7 (GPIO13, MOSI) to mosiPin and D5 (GPIO14,
//   SCK) to sckPin (12 and 5 are D6 and D1; both arguments are required). The
//   SCK ISR samples MOSI
//   on every rising edge (mode 0) and the 96 decoded bytes must match. Run
//   wifiOff first: the ISR budget is one SCK period, and MAC interrupts eat it.
//
// apa102HspiWatch: [sensePin, nBytes, nField] -> json
//   Starts a 25 kHz HSPI transfer by hand and polls sensePin through GPI for the
//   whole of it, so it counts the edges on the wire without relying on an ISR
//   (the driver's kick() only watches SPIBUSY, which runs for the programmed
//   time whether or not the pad is driven). nField >= 0 re-splits the same
//   total divider across CLKCNT_N/CLKDIV_PRE before the transfer, which is how
//   a zero CLKCNT_L was isolated; -1 runs whatever the driver installed.
//
// apa102HspiJumperCheck: [sensePin, txPin] -> json
//   Drives one HSPI output pad (txPin: 13 = MOSI/D7, 14 = SCK/D5) as a plain
//   GPIO and reports whether sensePin both follows the level and counts the
//   edges, which separates a missing jumper from a dead ISR; run it once per
//   signal before the loopback.
//
// All three accept sense pins only from 0, 5, 12 and 15 (the rest are the
// console, the UART bench, the clockless controller's pad, the flash pads, or
// the HSPI pads pinMode would unmux), and the loopback rejects two equal pins.
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
__attribute__((noinline)) static FL_IRAM void captureEdges(
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

constexpr int kClocklessPin = FL_PIN_CLOCKLESS_1;
// The platform's declared hardware-SPI pair in the active pin order -- the same
// key the native-HSPI specialization uses -- so the bench cannot silently
// measure the generic bit-bang under D1/NodeMCU numbering. The *RealPin
// constants are the fixed GPIO pads behind them, which is what pinMode() and
// GPI take.
constexpr int kApaDataPin = FL_PIN_SPI_DATA_1;
constexpr int kApaClockPin = FL_PIN_SPI_CLOCK_1;
constexpr int kApaDataRealPin = fl::FastPin<FL_PIN_SPI_DATA_1>::kRealPin;
constexpr int kApaClockRealPin = fl::FastPin<FL_PIN_SPI_CLOCK_1>::kRealPin;

struct DriverShowState {
    CRGB leds[64];
    CLEDController* clockless = nullptr;
    CLEDController* apa102 = nullptr;
};

inline DriverShowState& driverShowState() {
    return fl::SingletonShared<DriverShowState>::instance();
}

// Show one controller for `frames` frames and report frame-time spread.
//
// `controller->showLeds()` and not FastLED.show(): each bench times its own
// driver, so registering several controllers on overlapping pads across a
// session (apa102ShowMhz adds one per rate, and the bit-bang baseline shares
// GPIO4 with clocklessShow) cannot make two drivers write the same pin in one
// frame, and FastLED's global refresh-rate clamp never enters these numbers.
inline fl::json timeFrames(CLEDController* controller, int nLeds, int frames,
                           fl::i64 floorUs, fl::i64* minUsOut = nullptr) {
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
    if (minUsOut != nullptr) {
        *minUsOut = static_cast<fl::i64>(minUs);
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

// The chipset's own frame-length formula, so the expectation cannot drift from
// the length the controller actually emits.
inline fl::i64 apa102WireBytes(int nLeds) {
    // APA102Controller is declared at global scope (legacy naming).
    return static_cast<fl::i64>(
        ::APA102Controller<kApaDataPin, kApaClockPin, BGR>::calculateBytes(
            static_cast<fl::size>(nLeds)));
}

// Frame time against the wire time the programmed divider implies. A frame also
// carries FastLED's fixed per-show cost (~250 us on this board, identical on the
// bit-bang path) plus up to ~120 us of scheduling jitter, so the pass band is
// the wire time plus that slack. It is needed because these benches pass
// floorUs = 0: timeFrames can never see a truncated frame here, so its own
// `success` is unconditional.
//
// 400 us still rejects a bus running at half the programmed rate: at 6 MHz that
// would take 762 us of wire time, i.e. 1012 us per frame against a 781 us bound.
// A single divider step (~7% here) is inside this band by construction -- the
// band exists to catch a rate that never took effect, not to fine-tune it.
constexpr fl::i64 kApa102FrameSlackUs = 400;

// The divider the driver actually programmed, decoded from SPI1CLK:
//   f_spi = 80 MHz / ((CLKDIV_PRE + 1) * (CLKCNT_N + 1))
// Reading the register back is deliberate -- mirroring the driver's private
// clock() arithmetic would keep compiling (and keep reporting) after the driver
// changed, and expected_us would then describe a divider never programmed.
inline fl::u32 hspiAppliedHz() {
    const fl::u32 reg = SPI1CLK;  // ok platform specific - reads back the divider ESP8266HSPIOutput::init() just wrote
    if (reg & fl::esp8266::kHspiEquSysClk) {  // ok platform specific - SPI1CLK bit 31; no core API reports the bus clock
        return fl::esp8266::kHspiSourceHz;  // ok platform specific - SPI1's fixed 80 MHz source; no core API exposes it
    }
    const fl::u32 pre = ((reg >> 18) & 0x1FFFUL) + 1;
    const fl::u32 n = ((reg >> 12) & 0x3FUL) + 1;
    return fl::esp8266::kHspiSourceHz / (pre * n);  // ok platform specific - the same fixed-source decode; no core API exposes it
}

inline fl::json apa102RateReport(fl::json r, fl::i64 minUs, int nLeds, int mhz,
                                 fl::u32 hz) {
    const fl::i64 bytes = apa102WireBytes(nLeds);
    const fl::i64 bits = bytes * 8;
    const fl::i64 expected = hz ? (bits * 1000000 + hz - 1) / static_cast<fl::i64>(hz) : -1;
    r.set("success", expected > 0 && minUs >= expected &&
                         minUs <= expected + kApa102FrameSlackUs);
    r.set("mhz", static_cast<fl::i64>(mhz));
    r.set("requested_hz", static_cast<fl::i64>(mhz) * 1000000);
    r.set("achievable_hz", static_cast<fl::i64>(hz));
    r.set("wire_bytes_est", bytes);
    r.set("expected_us", expected);
    return r;
}

// The chipset default: DATA_RATE_MHZ(6) (fl/chipsets/apa102.h).
inline fl::json apa102Show(int nLeds, int frames) {
    DriverShowState& st = driverShowState();
    if (!st.apa102) {
        st.apa102 = &FastLED.addLeds<APA102, kApaDataPin, kApaClockPin, BGR>(st.leds, 64);
    }
    // init() is the only writer of SPI1CLK, and it re-installs the pad mux:
    // another bench may have left the bus at its own divider or taken the pads
    // back as plain GPIO, so re-init rather than trust the addLeds-time setup.
    st.apa102->init();
    const fl::u32 hz = hspiAppliedHz();
    const int n = nLeds < 1 ? 1 : (nLeds > 64 ? 64 : nLeds);
    // Two statements, not one: reading minUs in the same call that fills it in
    // leaves the order unspecified.
    fl::i64 minUs = 0;
    fl::json r = timeFrames(st.apa102, n, frames, 0, &minUs);
    return apa102RateReport(r, minUs, n, 6, hz);
}

inline fl::json hspiPinError(const char* what) {
    fl::json e = fl::json::object();
    e.set("success", false);
    e.set("error", what);
    return e;
}

// The same chipset on a pin pair HSPI cannot reach: the generic software SPI
// bit-bang this platform used for every APA102 before the native path. D2/D0,
// not the loopback sense pins, so a fitted jumper never has two outputs on it.
//
// 4/16 are only GPIO4/GPIO16 in raw pin order, and D1/NodeMCU numbering has no
// FastPin<16> at all, so the baseline is defined for the default order only.
#if defined(FASTLED_ESP8266_RAW_PIN_ORDER)  // ok platform specific - keys 4/16 are GPIO4/16 only in raw order; no API maps a real pad back to a key
constexpr int kBitbangDataPin = 4;
constexpr int kBitbangClockPin = 16;
#endif

inline fl::json apa102ShowBitbang(int nLeds, int frames) {
#if !defined(FASTLED_ESP8266_RAW_PIN_ORDER)  // ok platform specific - those keys exist only in raw order; no core API can express the pair portably
    (void)nLeds;
    (void)frames;
    return hspiPinError("the bit-bang baseline is only defined in raw pin order");
#else
    DriverShowState& st = driverShowState();
    static CLEDController* c = nullptr;
    if (c == nullptr) {
        c = &FastLED.addLeds<APA102, kBitbangDataPin, kBitbangClockPin, BGR>(st.leds, 64);
    }
    // Another bench may have taken these pads as inputs (uartWs2812Loopback's
    // rxPin accepts 4), and addLeds() only initialises once.
    c->init();
    const int n = nLeds < 1 ? 1 : (nLeds > 64 ? 64 : nLeds);
    fl::json r = timeFrames(c, n, frames, 0);
    r.set("data_pin", static_cast<fl::i64>(kBitbangDataPin));
    r.set("clock_pin", static_cast<fl::i64>(kBitbangClockPin));
    r.set("wire_bytes_est", apa102WireBytes(n));
    // This pair is only the generic bit-bang in the default build. Under
    // FASTLED_ALL_PINS_HARDWARE_SPI every non-HSPI pair goes to the legacy
    // Arduino-SPI shim, which drives GPIO13/14 -- so there the timing here is
    // that shim's, not a software bit-bang, and it is not a baseline.
#if defined(FASTLED_ALL_PINS_HARDWARE_SPI)  // ok platform specific - this build resolves the pair to the Arduino-SPI shim; no API reports the backend
    r.set("generic_bitbang", false);
#else
    r.set("generic_bitbang", true);
#endif
    return r;
#endif  // FASTLED_ESP8266_RAW_PIN_ORDER
}

// One APA102 controller per clock rate. SPI1CLK is written by init() alone, so
// a controller added at a different rate would otherwise keep the previous
// divider; the bench re-inits to make each rate real.
template <int MHZ>
inline CLEDController* apa102At(CRGB* leds) {
    static CLEDController* c =
        &FastLED.addLeds<APA102, kApaDataPin, kApaClockPin, BGR, DATA_RATE_MHZ(MHZ)>(leds, 64);
    return c;
}

inline fl::json apa102ShowMhz(int nLeds, int frames, int mhz) {
    DriverShowState& st = driverShowState();
    CLEDController* c = nullptr;
    switch (mhz) {
        case 6: c = apa102At<6>(st.leds); break;
        case 10: c = apa102At<10>(st.leds); break;
        case 20: c = apa102At<20>(st.leds); break;
        case 40: c = apa102At<40>(st.leds); break;
        case 80: c = apa102At<80>(st.leds); break;
        default: {
            fl::json e = fl::json::object();
            e.set("success", false);
            e.set("error", "mhz must be 6, 10, 20, 40 or 80");
            return e;
        }
    }
    const int n = nLeds < 1 ? 1 : (nLeds > 64 ? 64 : nLeds);
    c->init();
    // After init(): the divider under test is the one in the register.
    const fl::u32 hz = hspiAppliedHz();
    fl::i64 minUs = 0;
    fl::json r = timeFrames(c, n, frames, 0, &minUs);
    r = apa102RateReport(r, minUs, n, mhz, hz);
    r.set("cpu_mhz", static_cast<fl::i64>(system_get_cpu_freq()));  // ok platform specific - the bench reports the raw SoC clock; no core API exposes it
    return r;
}

// Pin-level HSPI loopback. GPIO_IN does not see GPIO13/14 once HSPI owns the
// pads, so read MOSI and SCK back through jumpers: SCK (D5, GPIO14) to sckPin
// and MOSI (D7, GPIO13) to mosiPin. The ISR has to
// sample the data, because the driver's flush() blocks on the wire. Sense pins
// are arguments, so a board where those pads are hard to reach can use any two
// of the pins hspiSensePinOk() allows.
constexpr int kHspiLoopBytes = 96;

struct HspiLoopbackState {
    fl::u8 got[kHspiLoopBytes];
    // Staged here rather than on the handler's stack: this file already split
    // its bind functions to stay inside 4 KB of loop stack, and the pattern is
    // fully derivable, so the bytes cost nothing in .bss.
    fl::u8 tx[kHspiLoopBytes];
    int bits;
    int rising;
    int falling;
    fl::u32 mosiMask;
    fl::u32 sckMask;
};

inline HspiLoopbackState& hspiLoopbackState() {
    return fl::SingletonShared<HspiLoopbackState>::instance();
}

// The ISR runs on every SCK edge, so it reads a cached pointer rather than
// paying the singleton lookup, and the accessor must inline so the ISR never
// fetches from flash mid-transfer.
__attribute__((always_inline)) inline HspiLoopbackState*& hspiLoopbackActive() {
    static HspiLoopbackState* s = nullptr;
    return s;
}

inline fl::u8 hspiPattern(int i) { return static_cast<fl::u8>(i * 37 + 11); }

// Samples MOSI on every SCK edge and keeps the rising ones (mode 0), so it
// must stay short: at the bench's clock the ISR gets a few microseconds.
__attribute__((noinline)) static FL_IRAM void hspiSckSample() {
    HspiLoopbackState& s = *hspiLoopbackActive();
    const fl::u32 gpi = GPI;  // ok platform specific - raw pad sampling for the HSPI pin-level loopback
    if (gpi & s.sckMask) {
        ++s.rising;
        if (s.bits < kHspiLoopBytes * 8) {
            if (gpi & s.mosiMask) {
                s.got[s.bits >> 3] |= static_cast<fl::u8>(0x80 >> (s.bits & 7));
            }
            ++s.bits;
        }
    } else {
        ++s.falling;
    }
}

// The ISR has to finish inside one bit period, so the loopback runs far below
// the driver's normal clock: at 40 us per bit even the core's generic ISR
// dispatch has slack.
template <int KHZ>
inline fl::json apa102HspiLoopbackAt(int mosiPin, int sckPin) {
    HspiLoopbackState& s = hspiLoopbackState();
    hspiLoopbackActive() = &s;  // cache for the ISR (constant-initialized static)
    s.mosiMask = 1u << mosiPin;
    s.sckMask = 1u << sckPin;
    s.bits = 0;
    s.rising = 0;
    s.falling = 0;
    for (int i = 0; i < kHspiLoopBytes; ++i) {
        s.got[i] = 0;
        s.tx[i] = hspiPattern(i);
    }

    fl::SPIOutput<kApaDataPin, kApaClockPin, DATA_RATE_KHZ(KHZ)> spi;
    spi.init();
    // Snapshot the pad mux the driver just installed. If GPF14 is not the BUS
    // function, the peripheral shifts the frame internally and nothing reaches
    // the wire -- which the frame time alone cannot tell you, because the shift
    // takes exactly as long either way.
    const fl::u32 gpf13 = GPF13;  // ok platform specific - reads back the pad function just installed; no core API reports it
    const fl::u32 gpf14 = GPF14;  // ok platform specific - reads back the pad function just installed; no core API reports it
    const fl::u32 gpe = GPE;      // ok platform specific - reads back the GPIO output enables; no core API reports them
    pinMode(mosiPin, INPUT);
    pinMode(sckPin, INPUT);
    spi.select();
    const fl::u32 t0 = micros();
    attachInterrupt(digitalPinToInterrupt(sckPin), hspiSckSample, CHANGE);
    spi.writeBytes(s.tx, kHspiLoopBytes);
    const fl::u32 elapsed = micros() - t0;
    // writeBytes blocks in kick() until SPIBUSY clears, but stay attached
    // through endTransaction() so no tail edge can be missed.
    spi.endTransaction();
    const int risingAfter = s.rising;
    const int fallingAfter = s.falling;
    const int bitsAfter = s.bits;

    int badBytes = 0, firstBad = -1;
    fl::u8 firstBadGot = 0;
    for (int i = 0; i < kHspiLoopBytes; ++i) {
        if (s.got[i] != hspiPattern(i)) {
            if (firstBad < 0) {
                firstBad = i;
                // Read now: the probe below drives SCK through this same ISR,
                // and when the transfer was short it keeps filling got[].
                firstBadGot = s.got[i];
            }
            ++badBytes;
        }
    }

    // Was that ISR live for the whole transfer? Drive the SCK pad from the CPU
    // now, with the same ISR still attached: if these edges are counted but the
    // transfer's were not, the attach held and the peripheral never drove the
    // pad -- as opposed to a dead ISR or a missing jumper. Runs after the
    // measurements so its edges cannot leak into bits/got[].
    pinMode(kApaClockRealPin, OUTPUT);
    for (int i = 0; i < 3; ++i) {
        digitalWrite(kApaClockRealPin, LOW);
        fl::delayMicroseconds(50);
        digitalWrite(kApaClockRealPin, HIGH);
        fl::delayMicroseconds(50);
    }
    const int probeEdges = s.rising - risingAfter;
    detachInterrupt(digitalPinToInterrupt(sckPin));
    spi.stop();

    fl::json r = fl::json::object();
    r.set("success", bitsAfter == kHspiLoopBytes * 8 && badBytes == 0);
    r.set("bytes", static_cast<fl::i64>(kHspiLoopBytes));
    r.set("bits", static_cast<fl::i64>(bitsAfter));
    r.set("sck_rising_edges", static_cast<fl::i64>(risingAfter));
    r.set("sck_falling_edges", static_cast<fl::i64>(fallingAfter));
    // The pad mux the driver installed, and whether the ISR was still live when
    // the CPU drove the pad: together these separate "no clock reached the pad"
    // from "no jumper" and from "dead ISR".
    r.set("gpf13", static_cast<fl::i64>(gpf13));
    r.set("gpf14", static_cast<fl::i64>(gpf14));
    r.set("gpe", static_cast<fl::i64>(gpe));
    r.set("probe_edges", static_cast<fl::i64>(probeEdges));
    r.set("isr_live_after_transfer", probeEdges == 3);
    r.set("bad_bytes", static_cast<fl::i64>(badBytes));
    r.set("first_bad", static_cast<fl::i64>(firstBad));
    if (firstBad >= 0) {
        r.set("first_bad_got", static_cast<fl::i64>(firstBadGot));
        r.set("first_bad_want", static_cast<fl::i64>(hspiPattern(firstBad)));
    }
    r.set("khz", static_cast<fl::i64>(KHZ));
    r.set("elapsed_us", static_cast<fl::i64>(elapsed));
    // The ISR budget is one SCK period, and the radio's MAC interrupts eat it:
    // a run with the radio up mis-times edges and reads as a wiring failure.
    r.set("wifi_opmode", static_cast<fl::i64>(wifi_get_opmode()));  // ok platform specific - ESP8266 bring-up bench measures raw SoC behavior; no core API exposes it
    return r;
}

// A sense pin has to be one of the pins that stay usable while the RPC console
// and the bus under test are busy: 1 and 3 are UART0 (the console), 2 is the
// UART bench's TX pad, 4 carries clocklessShow's WS2812 output, 6-11 are the
// flash pads (pinMode on them resets the board), and 13/14 are the HSPI pads the
// transfer is driving -- pinMode there would undo the bus mux. GPIO16 is absent from GPI, so it needs GP16I. `other`
// excludes the pin already used for the other signal, so a self-loop cannot
// pass with no jumper fitted.
inline bool hspiSensePinOk(int pin, int other) {
    const bool usable = pin == 0 || pin == 5 || pin == 12 || pin == 15;  // ok platform specific - the ESP8266 pads left usable on a NodeMCU; no core API reports which real pads the active pin order leaves free
    return usable && pin != other;
}

// Drives an HSPI output pad as a plain GPIO and both counts interrupts and
// samples the level on sensePin, so "no jumper fitted" (level never follows, no
// edges) is told apart from "the HSPI path never drove the pad" (level follows,
// no edges) and from a dead ISR. Only meaningful with that jumper fitted.
//
// txPin is the pad to drive (13 = MOSI/D7, 14 = SCK/D5); sensePin is where the
// jumper takes it (12 = D6, 5 = D1). Checking both signals separately is what
// tells a quiet data line from a missing data jumper.
inline fl::json apa102HspiJumperCheck(int sensePin, int txPin) {
    if (!hspiSensePinOk(sensePin, -1)) {
        return hspiPinError("sensePin must be one of 0, 5, 12, 15");
    }
    if (txPin != kApaDataRealPin && txPin != kApaClockRealPin) {
        return hspiPinError("txPin must be 13 (MOSI/D7) or 14 (SCK/D5)");
    }
    const int sckPin = sensePin;
    const int drivePin = txPin;
    HspiLoopbackState& s = hspiLoopbackState();
    hspiLoopbackActive() = &s;
    s.mosiMask = 0;  // this check samples SCK only; no stale mask from a loopback
    s.sckMask = 1u << sckPin;
    s.bits = 0;
    s.rising = 0;
    s.falling = 0;
    pinMode(sckPin, INPUT);
    pinMode(drivePin, OUTPUT);
    // pinMode(OUTPUT) does not clear the output latch, so start from a known
    // low: a high latch would swallow the first rising edge.
    digitalWrite(drivePin, LOW);
    attachInterrupt(digitalPinToInterrupt(sckPin), hspiSckSample, CHANGE);
    bool seenHigh = false, seenLow = false;
    for (int i = 0; i < 10; ++i) {
        digitalWrite(drivePin, HIGH);
        fl::delayMicroseconds(50);
        if (GPI & s.sckMask) {  // ok platform specific - raw pad level tells a missing jumper from a dead ISR
            seenHigh = true;
        }
        digitalWrite(drivePin, LOW);
        fl::delayMicroseconds(50);
        if ((GPI & s.sckMask) == 0) {  // ok platform specific - raw pad level, as above
            seenLow = true;
        }
    }
    detachInterrupt(digitalPinToInterrupt(sckPin));
    // Give the pad back to HSPI, not to the GPIO output register: a later
    // apa102Show that skipped init() would otherwise clock internally with the
    // pad held low and still report normal timing.
    pinMode(drivePin, SPECIAL);  // ok platform specific - SPECIAL is the Arduino-core spelling of "restore this pad's BUS function", i.e. give the pad back to HSPI; no core API restores a bus function
    const bool levelFollows = seenHigh && seenLow;
    const bool isrCounted = s.rising == 10 && s.falling == 10;
    fl::json r = fl::json::object();
    // A floating pin can both follow nothing and (rarely) produce noise edges,
    // so the level is the authority on whether the jumper is really fitted.
    r.set("success", levelFollows && isrCounted);
    r.set("level_follows_tx", levelFollows);
    r.set("isr_counted_edges", isrCounted);
    r.set("rising", static_cast<fl::i64>(s.rising));
    r.set("falling", static_cast<fl::i64>(s.falling));
    r.set("sense_pin", static_cast<fl::i64>(sensePin));
    r.set("tx_pin", static_cast<fl::i64>(txPin));
    return r;
}

inline fl::json apa102HspiLoopback(int khz, int mosiPin, int sckPin) {
    if (!hspiSensePinOk(mosiPin, sckPin) || !hspiSensePinOk(sckPin, mosiPin)) {
        return hspiPinError(
            "sense pins must be two different pins from 0, 5, 12, 15");
    }
    // Rates the driver resolves cleanly with a divider the ISR can keep up
    // with: at 25 kHz a bit lasts 40 us, so even the core's generic ISR
    // dispatch has room, and the radio must be off (wifiOff) for the rest.
    switch (khz) {
        case 25: return apa102HspiLoopbackAt<25>(mosiPin, sckPin);
        case 50: return apa102HspiLoopbackAt<50>(mosiPin, sckPin);
        default: {
            fl::json e = fl::json::object();
            e.set("success", false);
            e.set("error", "khz must be 25 or 50");
            return e;
        }
    }
}

// Watch the wire from inside the transfer instead of from an ISR.
//
// kick() only ever looks at SPIBUSY, so on its own it cannot tell a transfer
// that drove the pads from one that only shifted internally -- both take the
// same time. This stages and starts the transfer by hand and polls the sense
// pin in the wait loop, so a count of zero here means the pad really was quiet
// (and not merely that no interrupt happened to run).
inline fl::json apa102HspiWatch(int sckPin, int nBytes, int nField) {
    if (!hspiSensePinOk(sckPin, -1)) {
        return hspiPinError("sckPin must be one of 0, 5, 12, 15");
    }
    const int n = nBytes < 1 ? 1 : (nBytes > 64 ? 64 : nBytes);
    fl::SPIOutput<kApaDataPin, kApaClockPin, DATA_RATE_KHZ(25)> spi;
    spi.init();
    // nField >= 0 re-splits the same total divider across CLKCNT_N and
    // CLKDIV_PRE. Before kHspiMinNDivider the driver resolved 25 kHz to
    // CLKDIV_PRE = 3201 with CLKCNT_N = CLKCNT_L = 0, and CLKCNT_L is the
    // clock's high phase, so the pad had nothing to show; re-splitting the same
    // divider with CLKCNT_N = 1 (CLKCNT_L = 1) is how that was confirmed on the
    // wire. -1 runs whatever the driver installed, which is what a fixed build
    // should be measured with.
    const fl::u32 clkBefore = SPI1CLK;  // ok platform specific - reads back the divider the driver installed
    if (nField >= 0) {
        const fl::u32 pre1 = ((clkBefore >> 18) & 0x1FFFUL) + 1;
        const fl::u32 n1 = ((clkBefore >> 12) & 0x3FUL) + 1;
        const fl::u32 divider = pre1 * n1;
        const fl::u32 nNew = (static_cast<fl::u32>(nField) & 0x3FUL) + 1;
        const fl::u32 preNew = divider / nNew == 0 ? 1 : divider / nNew;
        // Same field layout the driver uses, plus a half-period low phase.
        SPI1CLK = ((nNew / 2) & 0x3FUL) | ((nNew - 1) << 12) | ((preNew - 1) << 18);  // ok platform specific - rebuilds SPI1CLK by hand; no core API exposes it
    }
    pinMode(sckPin, INPUT);
    const fl::u32 mask = 1u << sckPin;
    // Same staging as the driver's flush(): length in SPI1U1, bytes packed
    // little-endian into the FIFO words.
    const fl::u32 bits = static_cast<fl::u32>(n) * 8 - 1;
    const fl::u32 lenMask = (static_cast<fl::u32>(SPIMMOSI) << SPILMOSI) |  // ok platform specific - SPI1U field masks/shifts; no core API exposes them
                            (static_cast<fl::u32>(SPIMMISO) << SPILMISO);  // ok platform specific - SPI1U field masks/shifts; no core API exposes them
    while (SPI1CMD & SPIBUSY) {}  // ok platform specific - waits out a previous transfer; no core API polls SPI1CMD
    SPI1U1 = (SPI1U1 & ~lenMask) | (bits << SPILMOSI) | (bits << SPILMISO);  // ok platform specific - sets the SPI1 transfer length, as the driver's flush() does
    volatile fl::u32* fifo = &SPI1W0;  // ok platform specific - SPI1 FIFO staging; no core API exposes it
    for (int i = 0; i < (n + 3) / 4; ++i) {
        const int b = i * 4;
        fl::u32 word = static_cast<fl::u32>(hspiPattern(b));
        if (b + 1 < n) word |= static_cast<fl::u32>(hspiPattern(b + 1)) << 8;
        if (b + 2 < n) word |= static_cast<fl::u32>(hspiPattern(b + 2)) << 16;
        if (b + 3 < n) word |= static_cast<fl::u32>(hspiPattern(b + 3)) << 24;
        fifo[i] = word;
    }
    int transitions = 0, highSamples = 0, samples = 0;
    fl::u32 last = GPI & mask;  // ok platform specific - raw pad level while the bus runs; no core API reports it
    const fl::u32 t0 = micros();
    // The same barrier the driver's kick() issues: the SPI1U1 length and FIFO
    // writes above must be visible before the peripheral is told to start, or
    // the transfer runs with init()'s leftover length and the counts describe
    // the wrong frame.
    __sync_synchronize();
    SPI1CMD |= SPIBUSY;  // ok platform specific - starts the hand-staged transfer; no core API starts a raw SPI1 run
    while (SPI1CMD & SPIBUSY) {  // ok platform specific - polls the bus while sampling the pad; no core API exposes SPI1CMD
        const fl::u32 now = GPI & mask;  // ok platform specific - raw pad level while the bus runs
        ++samples;
        if (now != last) {
            ++transitions;
            last = now;
        }
        if (now != 0) {
            ++highSamples;
        }
    }
    const fl::u32 elapsed = micros() - t0;
    spi.stop();

    fl::json r = fl::json::object();
    r.set("success", transitions > 0);
    r.set("khz", static_cast<fl::i64>(25));
    r.set("bytes", static_cast<fl::i64>(n));
    r.set("bits", static_cast<fl::i64>(n) * 8);
    r.set("sck_pin", static_cast<fl::i64>(sckPin));
    r.set("transitions", static_cast<fl::i64>(transitions));
    r.set("high_samples", static_cast<fl::i64>(highSamples));
    r.set("samples", static_cast<fl::i64>(samples));
    r.set("elapsed_us", static_cast<fl::i64>(elapsed));
    // SPI1CLK: [5:0] CLKCNT_L, [11:6] CLKCNT_H, [17:12] CLKCNT_N, [30:18] PRE.
    // Reported after the override, so these are the fields the transfer ran
    // with rather than a mirror of what init() left behind.
    const fl::u32 clkRan = SPI1CLK;  // ok platform specific - reads back the divider the transfer actually ran with
    r.set("clk_cnt_l", static_cast<fl::i64>(clkRan & 0x3FUL));
    r.set("clk_cnt_h", static_cast<fl::i64>((clkRan >> 6) & 0x3FUL));
    r.set("clk_cnt_n", static_cast<fl::i64>((clkRan >> 12) & 0x3FUL));
    r.set("clk_pre", static_cast<fl::i64>((clkRan >> 18) & 0x1FFFUL));
    r.set("clk_before_pre", static_cast<fl::i64>((clkBefore >> 18) & 0x1FFFUL));
    r.set("n_field", static_cast<fl::i64>(nField));
    return r;
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
__attribute__((noinline)) inline void bindEsp8266WatchdogRpcs(fl::Remote& remote) {
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

__attribute__((noinline)) inline void bindEsp8266UartRpcs(fl::Remote& remote) {
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

__attribute__((noinline)) inline void bindEsp8266DriverRpcs(fl::Remote& remote) {
    remote.bind("wifiOff", []() -> fl::json { return wifiOff(); });
    remote.bind("clocklessShow", [](int nLeds, int frames) -> fl::json {
        return clocklessShow(nLeds, frames);
    });
    remote.bind("apa102Show", [](int nLeds, int frames) -> fl::json {
        return apa102Show(nLeds, frames);
    });
}

// Own frame: every bind's temporaries get a stack slot in the caller, and one
// more bind in bindEsp8266DriverRpcs overflowed the 4 KB loop stack.
__attribute__((noinline)) inline void bindEsp8266HspiRpcs(fl::Remote& remote) {
    remote.bind("apa102ShowMhz", [](int nLeds, int mhz) -> fl::json {
        return apa102ShowMhz(nLeds, 20, mhz);
    });
    remote.bind("apa102HspiLoopback", [](int khz, int mosiPin, int sckPin) -> fl::json {
        return apa102HspiLoopback(khz, mosiPin, sckPin);
    });
    remote.bind("apa102HspiJumperCheck", [](int sensePin, int txPin) -> fl::json {
        return apa102HspiJumperCheck(sensePin, txPin);
    });
    remote.bind("apa102HspiWatch", [](int sckPin, int nBytes, int nField) -> fl::json {
        return apa102HspiWatch(sckPin, nBytes, nField);
    });
}

// A third frame for the same reason: keep every bind's temporaries in a frame
// small enough for the 4 KB loop stack.
__attribute__((noinline)) inline void bindEsp8266HspiBenchRpcs(fl::Remote& remote) {
    remote.bind("apa102ShowBitbang", [](int nLeds, int frames) -> fl::json {
        return apa102ShowBitbang(nLeds, frames);
    });
}

inline void bindEsp8266Benches(fl::Remote& remote) {
    bindEsp8266WatchdogRpcs(remote);
    bindEsp8266UartRpcs(remote);
    bindEsp8266DriverRpcs(remote);
    bindEsp8266HspiRpcs(remote);
    bindEsp8266HspiBenchRpcs(remote);
}

}  // namespace esp8266
}  // namespace autoresearch

#endif  // FL_IS_ESP8266
