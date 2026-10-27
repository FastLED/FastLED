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
//   APA102 on native HSPI (GPIO13/14) at 6/10/20/40/80 MHz; min_us against
//   wire_bytes_est * 8 / mhz shows how close the ping-pong buffer gets.
//
// apa102HspiLoopback: [] -> json
//   Pin-level HSPI check; needs jumpers D7->D6 and D5->D1 (see the function).
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

// One APA102 controller per clock rate: HSPI reprograms its clock on every
// frame, so several strips on GPIO13/14 at different rates coexist.
template <int MHZ>
inline CLEDController* apa102At(CRGB* leds) {
    static CLEDController* c = &FastLED.addLeds<APA102, kApaDataPin, kApaClockPin, BGR, DATA_RATE_MHZ(MHZ)>(leds, 64);
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
            fl::json r = fl::json::object();
            r.set("success", false);
            r.set("error", "mhz must be 6, 10, 20, 40 or 80");
            return r;
        }
    }
    const int n = nLeds < 1 ? 1 : (nLeds > 64 ? 64 : nLeds);
    fl::json r = timeFrames(c, n, frames, 0);
    // Start frame + 4 bytes per LED + APA102Controller::endBoundary's
    // (n / 32 + 1) end dwords. showLeds() returns once the tail chunk is on
    // the hardware, but each frame's select() waits for the previous tail,
    // so steady-state min_us is throughput, not one frame's latency.
    const fl::i64 bytes = 4 + 4 * n + 4 * (n / 32 + 1);
    r.set("mhz", static_cast<fl::i64>(mhz));
    // ok platform specific - ESP8266 bench reports the raw SoC clock; no core API exposes it
    r.set("cpu_mhz", static_cast<fl::i64>(system_get_cpu_freq()));
    r.set("wire_bytes_est", bytes);
    return r;
}

// apa102HspiLoopback: [] -> json
//   Drives HSPI at 250 kHz and reads MOSI and SCK back through jumpers
//   D7->D6 (GPIO13->12) and D5->D1 (GPIO14->5) via GPIO_IN while the
//   driver streams 96 bytes through its ping-pong halves (W0-7 / W8-15) in
//   4, 8, 16, 32, 32 and 4-byte chunks, then decodes MOSI on every SCK
//   rising edge (mode 0). The bench feeds bytes from the sampling loop only
//   when doing so would not block, so the capture sees every half switch.
constexpr int kHspiLoopBytes = 96;
// GPIO_IN does not see GPIO13/14 once HSPI owns the pads (measured: zero SCK
// edges while frames went out), so read them through jumpers instead.
constexpr int kHspiMosiSense = 12;  // jumper D7 (GPIO13) -> D6 (GPIO12)
constexpr int kHspiSckSense = 5;    // jumper D5 (GPIO14) -> D1 (GPIO5)

struct HspiCapture {
    fl::u8 got[kHspiLoopBytes];
    int bits;
    int sckEdges;
    bool sckIdleLow;
};

inline fl::u8 hspiPattern(int i) { return static_cast<fl::u8>(i * 37 + 11); }

__attribute__((noinline)) static FL_IRAM void captureHspi(HspiCapture& cap, fl::u32 timeoutCycles) {
    using fl::esp8266_hspi::State;
    // ok platform specific - the bench feeds the HSPI staging buffer directly to
    // overlap the sampler with the wire; SPIOutput only exposes whole frames
    State& s = fl::esp8266_hspi::state();
    const fl::u32 mosi = 1u << kHspiMosiSense, sck = 1u << kHspiSckSense;
    fl::u32 prev = GPI;  // ok platform specific - raw pad sampling for the HSPI pin-level loopback
    cap.sckIdleLow = (prev & sck) == 0;
    int fed = 0;
    const fl::u32 t0 = cycles();
    while (cycles() - t0 < timeoutCycles) {
        const fl::u32 now = GPI;  // ok platform specific - raw pad sampling for the HSPI pin-level loopback
        if ((now & sck) && !(prev & sck)) {
            ++cap.sckEdges;
            if (cap.bits < kHspiLoopBytes * 8) {
                if (now & mosi) {
                    cap.got[cap.bits >> 3] |= static_cast<fl::u8>(0x80 >> (cap.bits & 7));
                }
                ++cap.bits;
            }
        }
        prev = now;
        const bool busy = (SPI1CMD & SPIBUSY) != 0;  // ok platform specific - HSPI busy flag, so the feed never blocks the sampler
        // ok platform specific - byte-at-a-time feeding is what lets the sampler
        // stay in the loop across a half switch; SPIOutput would block per frame
        if (fed < kHspiLoopBytes && !(busy && s.ptr + 1 == s.end)) {
            fl::esp8266_hspi::push(hspiPattern(fed++));
        } else if (fed == kHspiLoopBytes && !busy) {
            if (s.ptr != s.stage.bytes) {
                fl::esp8266_hspi::flush();  // the tail chunk; does not wait
            } else if (cap.bits >= kHspiLoopBytes * 8) {
                break;
            }
        }
    }
}

inline fl::json apa102HspiLoopback() {
    fl::SPIOutput<kApaDataPin, kApaClockPin, DATA_RATE_KHZ(250)> spi;
    spi.init();
    pinMode(kHspiMosiSense, INPUT);
    pinMode(kHspiSckSense, INPUT);
    spi.select();
    HspiCapture cap = {};
    const fl::u32 savedPs = xt_rsil(15);  // ok platform specific - GPI polling must not miss a 2 us half-bit
    captureHspi(cap, system_get_cpu_freq() * 20000u);  // 20 ms; ok platform specific - cycles from the raw SoC clock
    xt_wsr_ps(savedPs);  // ok platform specific - restores IRQ level from xt_rsil
    spi.endTransaction();
    int badBytes = 0, firstBad = -1;
    for (int i = 0; i < kHspiLoopBytes; ++i) {
        if (cap.got[i] != hspiPattern(i)) {
            if (firstBad < 0) firstBad = i;
            ++badBytes;
        }
    }
    fl::json r = fl::json::object();
    r.set("success", cap.bits == kHspiLoopBytes * 8 && badBytes == 0 && cap.sckIdleLow);
    r.set("bits", static_cast<fl::i64>(cap.bits));
    r.set("sck_rising_edges", static_cast<fl::i64>(cap.sckEdges));
    r.set("sck_idle_low", cap.sckIdleLow);
    r.set("bad_bytes", static_cast<fl::i64>(badBytes));
    r.set("first_bad", static_cast<fl::i64>(firstBad));
    if (firstBad >= 0) {
        r.set("first_bad_got", static_cast<fl::i64>(cap.got[firstBad]));
        r.set("first_bad_want", static_cast<fl::i64>(hspiPattern(firstBad)));
    }
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

// Own frame: every bind's temporaries get a stack slot in the caller, and
// one more bind in bindEsp8266DriverRpcs overflowed the 4 KB loop stack.
__attribute__((noinline)) inline void bindEsp8266HspiRpcs(fl::Remote& remote) {
    remote.bind("apa102ShowMhz", [](int nLeds, int mhz) -> fl::json {
        return apa102ShowMhz(nLeds, 20, mhz);
    });
    remote.bind("apa102HspiLoopback", []() -> fl::json { return apa102HspiLoopback(); });
}

inline void bindEsp8266Benches(fl::Remote& remote) {
    bindEsp8266WatchdogRpcs(remote);
    bindEsp8266UartRpcs(remote);
    bindEsp8266DriverRpcs(remote);
    bindEsp8266HspiRpcs(remote);
}

}  // namespace esp8266
}  // namespace autoresearch

#endif  // FL_IS_ESP8266
