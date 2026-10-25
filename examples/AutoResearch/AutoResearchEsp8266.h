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

#include "platforms/esp/is_esp.h"  // ok platform headers - ESP8266 bench gate

#if defined(FL_IS_ESP8266)

#include "FastLED.h"
#include "fl/stl/json.h"
#include "fl/stl/stdint.h"
#include "platforms/esp/8266/fastled_esp8266_uart.h"  // ok platform headers - driver under test

// IWYU pragma: begin_keep
#include <esp8266_peri.h>     // GPI, ESP8266_CLOCK
#include <user_interface.h>   // system_update_cpu_freq
// IWYU pragma: end_keep

namespace autoresearch {
namespace esp8266 {

constexpr int kMaxLoopbackLeds = 10;  // 120 UART bytes fit the 128-byte FIFO
constexpr int kMaxEdges = kMaxLoopbackLeds * 24 * 2 + 4;

inline fl::u32 cycles() {
    fl::u32 c;
    __asm__ __volatile__("rsr %0,ccount" : "=a"(c));
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

    const fl::u8 prevMhz = system_get_cpu_freq();
    system_update_cpu_freq(160);
    const fl::u32 cyclesPerUs = 160;
    const fl::u32 idleCycles = 20 * cyclesPerUs;  // 20 us of silence ends it

    EdgeCapture& cap = edgeCapture();
    cap.count = 0;
    const fl::u32 mask = 1u << rxPin;

    const fl::u32 savedPs = xt_rsil(15);
    bool level = (GPI & mask) != 0;
    cap.firstLevel = level;
    for (int i = 0; i < nLeds * 12; ++i) {
        USF(fl::esp8266_uart::kUart) = encoded[i];
    }
    fl::u32 lastEdge = cycles();
    while (cap.count < kMaxEdges) {
        const bool now = (GPI & mask) != 0;
        const fl::u32 t = cycles();
        if (now != level) {
            cap.edges[cap.count++] = t;
            level = now;
            lastEdge = t;
        } else if (t - lastEdge > idleCycles) {
            break;
        }
    }
    xt_wsr_ps(savedPs);
    system_update_cpu_freq(prevMhz);

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
    USIE(0) = 0;  // EXPERIMENT
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
    r.set("wifi_opmode", static_cast<fl::i64>(wifi_get_opmode()));
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
    wifi_set_opmode_current(NULL_MODE);
    fl::json r = fl::json::object();
    r.set("success", wifi_get_opmode() == NULL_MODE);
    r.set("wifi_opmode", static_cast<fl::i64>(wifi_get_opmode()));
    return r;
}

inline void bindEsp8266Benches(fl::Remote& remote) {
    remote.bind("wifiOff", []() -> fl::json { return wifiOff(); });
    remote.bind("clocklessShow", [](int nLeds, int frames) -> fl::json {
        return clocklessShow(nLeds, frames);
    });
    remote.bind("apa102Show", [](int nLeds, int frames) -> fl::json {
        return apa102Show(nLeds, frames);
    });
    remote.bind("uartWs2812Loopback", [](int nLeds, int rxPin) -> fl::json {
        return uartWs2812Loopback(nLeds, rxPin);
    });
    remote.bind("uartWs2812Show", [](int nLeds) -> fl::json {
        return uartWs2812Show(nLeds);
    });
}

}  // namespace esp8266
}  // namespace autoresearch

#endif  // FL_IS_ESP8266
