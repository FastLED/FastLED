#pragma once
//
// examples/AutoResearch/AutoResearchLowMemory.h
//
// Low-memory AutoResearch bring-up surface for parts whose flash budget
// cannot fit the full ESP32 / Teensy LED-protocol matrix. Originally lived
// as the standalone `examples/AutoResearchLpc/AutoResearchLpc.ino` sketch;
// retired and folded in here per FastLED #3030 once the soft-FP cascade
// from #3022 phase 2 freed up the budget on LPC8xx.
//
// Included from the main `AutoResearch.ino` when
// `FL_PLATFORM_HAS_LARGE_MEMORY == 0` (Low + Tiny tiers per the FastLED
// #3000 memory classification).
//
// Same JSON-RPC contract as the retired LPC sketch:
//   echo: [int] -> int
//   findConnectedPins: [] -> "pairs=..;stuck=..;skip=..;od=.." (LPC)
//   pinToggleRx: [tx_pin, rx_pin, freq_hz, duration_ms] -> CSV
//   ws2812SctTest: [test_case, tx_pin, rx_pin, capture_ms] -> CSV (10 fields)
// See FastLED #3021 for the SCT-RX bring-up flags.
//

#include "platforms/arm/lpc/is_lpc.h"  // ok platform headers - LPC RX driver gate

// Gate FL_DBG to no-op. Without `RELEASE`, fl/log/log.h auto-sets
// FASTLED_FORCE_DBG=1, which expands every FL_DBG call site (inside
// fl::Remote etc.) through the fl::println formatting machinery and
// blows ~4 KB of flash budget. fbuild's nxplpc orchestrator does not
// yet propagate platformio.ini's `build_flags`, so it's defined here.
#ifndef RELEASE
#define RELEASE 1
#endif

// Dedicated #3039 codec-test builds keep the low-memory surface to echo +
// IEEE754 only. The SCT/WS2812 handlers are useful diagnostics, but they are
// unrelated to the codec and push LPC845 past its flash budget when combined.
#if defined(FL_AUTORESEARCH_IEEE754) && FL_AUTORESEARCH_IEEE754
#define FASTLED_AUTORESEARCH_IEEE754_MODE 1
#endif

// ESP8266 has 4 MB of flash but too little DRAM for the full harness, so its
// low-memory build always carries the IEEE754 codec check next to echo.
#include "platforms/esp/is_esp.h"  // ok platform headers // ok platform specific - ESP8266 surface gate
#if defined(FL_IS_ESP8266) && !defined(FASTLED_AUTORESEARCH_IEEE754_MODE)  // ok platform specific - ESP8266 low-DRAM build always carries IEEE754 check
#define FASTLED_AUTORESEARCH_IEEE754_MODE 1
#endif
// The clockless bench reports the driver's frame/retry counters.
#if defined(FL_IS_ESP8266) && !defined(FASTLED_DEBUG_COUNT_FRAME_RETRIES)  // ok platform specific - clockless bench reads ESP8266 retry counters
#define FASTLED_DEBUG_COUNT_FRAME_RETRIES 1
#endif

#if !defined(FASTLED_AUTORESEARCH_LPC_UART_DMA)
#define FASTLED_AUTORESEARCH_LPC_UART_DMA 0
#endif

// Opt in to the SCT input-capture RX backend (FastLED #3021). With this
// flag set, `LpcSctRxChannel::begin()` programs the SCT for hardware
// edge-capture; without it the driver is a no-op stub (host tests still
// work via `injectEdges()`).
#if !defined(FASTLED_AUTORESEARCH_IEEE754_MODE) && \
    !defined(FASTLED_AUTORESEARCH_FAULT_TEST)
#ifndef FASTLED_LPC_RX_SCT
#define FASTLED_LPC_RX_SCT 1
#endif
#endif

// The LPC845-BRK low-memory build fits FastLED + Remote + the WS2812 loopback
// RPC in normal mode. The dedicated IEEE754 mode deliberately omits it. The
// loopback drives the default clockless bus (UART DMA), so a
// FASTLED_LPC_UART_DMA=0 (bit-bang) build leaves it out.
#if defined(FL_IS_ARM_LPC_845) && !defined(FASTLED_AUTORESEARCH_IEEE754_MODE) && \
    !defined(FASTLED_AUTORESEARCH_FAULT_TEST) && !defined(FASTLED_LPC_SPI_DMA) && \
    !defined(FASTLED_LPC_PWM_DMA) && !FASTLED_AUTORESEARCH_LPC_UART_DMA && \
    FASTLED_LPC_UART_DMA  // ok platform specific - bench exists only for the LPC845 UART-DMA clockless TX captured by the SCT
#define FASTLED_AUTORESEARCH_LPC_WS2812 1
#endif

// The WS2812 loopback uses the SCT->DMA RX capture path. The UART clockless
// verifier uses USART RX-DMA instead, so keep SCT-DMA out of that build;
// UM11029 Table 296 fixes USART1_RX/USART1_TX on DMA channels 2/3.
#if defined(FASTLED_AUTORESEARCH_LPC_WS2812) && \
    !defined(FASTLED_LPC_RX_SCT_DMA)
#define FASTLED_LPC_RX_SCT_DMA 1
#endif

#include <Arduino.h>
#include "fl/remote/remote.h"
#include "fl/remote/transport/serial.h"
#include "fl/stl/compiler_control.h"
#include "fl/wdt/watchdog.h"
#include "fl/log/log.h"
#include "fl/stl/cstdio.h"  // fl::serial_begin -- HWCDC-safe Serial.begin wrapper
#include "fl/stl/singleton.h"

#if defined(FASTLED_AUTORESEARCH_IEEE754_MODE)
#include "AutoResearchIeee754.h"
#endif

// The host-stub example DLL build (`tests/shared/example_dll_wrapper_template.cpp`)
// compiles this sketch on Linux to surface ABI breaks. The RX SCT driver
// types are platform-gated behind `FL_IS_ARM_LPC` -- they only exist on
// the real LPC build. So the include + every RX usage below must be gated
// the same way.
#if defined(FL_IS_ARM_LPC) && !defined(FASTLED_AUTORESEARCH_IEEE754_MODE) && \
    !defined(FASTLED_AUTORESEARCH_FAULT_TEST)
#include "fl/channels/rx_sct_capture.h"
#include "fl/stl/strstream.h"
#include "fl/system/pin.h"        // fl::pinMode/digitalWrite/digitalRead
#include "fl/system/pin_probe.h"  // fl::pinProbeSkipReason
#include "platforms/arm/lpc/led_sysdefs_arm_lpc.h"  // ok platform headers - vendor CMSIS SysTick for pinToggleRx edge timing
#endif

#if defined(FASTLED_AUTORESEARCH_LPC_WS2812)
#include "FastLED.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/chipsets/timing_traits.h"
#endif

// FastLED #3468: channels-API SCT+DMA clockless AutoResearch harness.
// Opt-in via `-DFASTLED_LPC_PWM_DMA=1`; the header self-gates on
// `FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA` so unrelated builds are
// unaffected. Note: exclusive with FASTLED_AUTORESEARCH_LPC_WS2812 —
// both target the same SCT peripheral and would collide at runtime.
// The host-side `bash autoresearch --pwm-dma-cl` wrapper enforces
// mutual exclusion with `--dma-spi` and the legacy `ws2812SctTest`.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)
#include "AutoResearchPwmDmaClockless.h"
#endif

// FastLED #3456: LPC845 SPI+DMA async AutoResearch harness (Phase 1 of
// #3453 bench bring-up). Opt-in via `-DFASTLED_LPC_SPI_DMA=1`; the
// header self-gates on `FL_IS_ARM_LPC_845 && FASTLED_LPC_SPI_DMA`.
// LPC804 is NOT supported — silicon has no DMA peripheral. See
// `agents/docs/peripheral-existence.md`. Mutually exclusive with
// FASTLED_LPC_PWM_DMA (both claim DMA0 channels and the LowMemory
// flash budget doesn't fit both). Enforced host-side by
// `bash autoresearch` refusing `--dma-spi --pwm-dma-cl`.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_SPI_DMA) && \
    defined(FASTLED_LPC_PWM_DMA)
#error "FASTLED_LPC_SPI_DMA and FASTLED_LPC_PWM_DMA are mutually exclusive on LPC845: both claim DMA0 channels and the LowMemory flash budget doesn't fit both. Pick one for the AutoResearch build."
#endif
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_SPI_DMA)
#include "AutoResearchSpiDma.h"
#endif

// #3453 follow-up: LPC845 async UART TX harness (DMA + ISR chunk chain) and
// #3611 UART clockless loopback verifier. The UART DMA driver is now the
// LPC845 library default, so the AutoResearch RPC harness has its own flag.
#if defined(FL_IS_ARM_LPC_845) && FASTLED_AUTORESEARCH_LPC_UART_DMA && \
    !FASTLED_LPC_UART_DMA
#error "FASTLED_AUTORESEARCH_LPC_UART_DMA requires FASTLED_LPC_UART_DMA=1."
#endif
#if defined(FL_IS_ARM_LPC_845) && FASTLED_AUTORESEARCH_LPC_UART_DMA && \
    (defined(FASTLED_LPC_SPI_DMA) || defined(FASTLED_LPC_PWM_DMA))
#error "FASTLED_AUTORESEARCH_LPC_UART_DMA is mutually exclusive with the SPI/PWM DMA harnesses on LPC845: the LowMemory flash budget fits one bench at a time. Pick one for the AutoResearch build."
#endif
#if defined(FL_IS_ARM_LPC_845) && FASTLED_AUTORESEARCH_LPC_UART_DMA
#include "AutoResearchUartDma.h"
#endif

// ESP8266 UART WS2812 driver benches (pad self-capture, no jumper needed).
#if defined(FL_IS_ESP8266)  // ok platform specific - ESP8266 bring-up benches
#include "AutoResearchEsp8266.h"
#endif

namespace autoresearch {
namespace low_memory {

struct RemoteState {
    fl::Remote remote;

    RemoteState()
        : remote(fl::createSerialRequestSource(),
                 fl::createSerialResponseSink("REMOTE: ")) {}
};

inline RemoteState& remoteState() {
    return fl::SingletonShared<RemoteState>::instance();
}

#if defined(FL_IS_ARM_LPC) && !defined(FASTLED_AUTORESEARCH_IEEE754_MODE) && \
    !defined(FASTLED_AUTORESEARCH_FAULT_TEST)
struct PinToggleState {
    fl::EdgeTime edges[64];
};

inline PinToggleState& pinToggleState() {
    return fl::SingletonShared<PinToggleState>::instance();
}
#endif

#if defined(FASTLED_AUTORESEARCH_LPC_WS2812)
struct Ws2812State {
    uint8_t decoded[300];
    fl::EdgeTime probe[64];
};

inline Ws2812State& ws2812State() {
    return fl::SingletonShared<Ws2812State>::instance();
}
#endif

}  // namespace low_memory
}  // namespace autoresearch

namespace {

#if defined(FL_IS_ARM_LPC) && !defined(FASTLED_AUTORESEARCH_IEEE754_MODE) && \
    !defined(FASTLED_AUTORESEARCH_FAULT_TEST)
// Period-stat helpers for `pinToggleRx`. Mirrors the FlexIO RX benchmark
// logic in `examples/AutoResearch/AutoResearchRemote.cpp::flexioRxBenchmark`
// but inline so we don't pull in the AutoResearch ObjectFLED bus.
struct LowMemPinTogglePeriodStats {
    fl::u32 periods;
    fl::u32 mean_ns;
    fl::u32 sigma_ns;
    fl::u32 min_ns;
    fl::u32 max_ns;
};

inline LowMemPinTogglePeriodStats computeLowMemPeriodStats(
    const fl::EdgeTime* edges, fl::size edge_count) FL_NO_EXCEPT {
    LowMemPinTogglePeriodStats s{0, 0, 0, 0, 0};
    // Two passes: sum of squared deviations from the mean. The one-pass
    // E[x^2] - floor(mean)^2 form lost ~1.4 us of sigma to the floor at a
    // 1 ms period -- more than the bench's 1 us sigma budget.
    fl::u64 sum    = 0;
    fl::u32 min_ns = 0xFFFFFFFFu;
    fl::u32 max_ns = 0;
    fl::u32 count  = 0;
    for (fl::size i = 0; i + 1 < edge_count; i += 2) {
        const fl::u32 period_ns =
            static_cast<fl::u32>(edges[i].ns) +
            static_cast<fl::u32>(edges[i + 1].ns);
        if (period_ns == 0) continue;
        sum += period_ns;
        if (period_ns < min_ns) min_ns = period_ns;
        if (period_ns > max_ns) max_ns = period_ns;
        ++count;
    }
    if (count == 0) return s;
    const fl::u32 mean = static_cast<fl::u32>(sum / static_cast<fl::u64>(count));
    fl::u64 dev_sq = 0;
    for (fl::size i = 0; i + 1 < edge_count; i += 2) {
        const fl::u32 period_ns =
            static_cast<fl::u32>(edges[i].ns) +
            static_cast<fl::u32>(edges[i + 1].ns);
        if (period_ns == 0) continue;
        const fl::u64 d = (period_ns > mean) ? (period_ns - mean) : (mean - period_ns);
        const fl::u64 sq = d * d;  // d < 2^32
        // Saturate: a few edges clamped to ~2.1 s would wrap the sum and
        // report a tiny sigma.
        dev_sq = (dev_sq > ~fl::u64(0) - sq) ? ~fl::u64(0) : dev_sq + sq;
    }
    const fl::u64 var = dev_sq / static_cast<fl::u64>(count);
    // Bitwise integer sqrt: 32 steps whatever `var` is. A linear search ran
    // for billions of steps on a wide spread and tripped the watchdog.
    fl::u64 rem = var;
    fl::u64 root = 0;
    fl::u64 bit = fl::u64(1) << 62;
    while (bit > rem) bit >>= 2;
    while (bit != 0) {
        if (rem >= root + bit) {
            rem -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    const fl::u32 sig = static_cast<fl::u32>(root);
    s.periods  = count;
    s.mean_ns  = mean;
    s.sigma_ns = sig;
    s.min_ns   = (min_ns == 0xFFFFFFFFu) ? 0u : min_ns;
    s.max_ns   = max_ns;
    return s;
}
// Core-clock tick counter for pinToggleRx edge scheduling, built on the
// SysTick down-counter (reloads every 1 ms). micros() costs ~19 us per call
// on the 24 MHz M0+ (two software divides), which capped the bench near
// 7 kHz and added ~10 us of jitter to every edge. Valid while now() is
// called at least once per SysTick period.
struct LowMemTickClock {
    // millis() starts the core's SysTick (core clock, 1 ms reload) if
    // nothing has yet; a stopped SysTick would never reach a deadline.
    fl::u32 last = (static_cast<void>(millis()), SysTick->VAL);
    fl::u32 elapsed = 0;
    fl::u32 now() FL_NO_EXCEPT {
        const fl::u32 v = SysTick->VAL;
        elapsed += (last >= v) ? (last - v) : (last + SysTick->LOAD + 1u - v);
        last = v;
        return elapsed;
    }
};

// findConnectedPins for the low-memory LPC build: discovers bench jumpers
// through FastLED's own pin backend (fl::pinMode/digitalWrite/digitalRead,
// #5260) -- the LPC8xx Arduino core's pinMode puts pulls on the wrong IOCON
// slot (FastLED/framework-arduino-lpc8xx#40). Every pin is tested against
// every other pin, and every connected direction is reported.
//
// LPC845 PIO0_10/PIO0_11 are true open-drain pads: no pulls, sink only. They
// are probed as drivers (pulled LOW against the other pin's pull-up) but can
// never be the observed end, so a jumper between the two of them is invisible
// without an external pull-up.
inline bool lowMemOpenDrainPin(int pin) FL_NO_EXCEPT {
#if defined(FL_IS_ARM_LPC_845)  // ok platform specific - PIO0_10/PIO0_11 are the LPC845's pull-less open-drain pads; no core API reports pad type
    return pin == 10 || pin == 11;
#else
    (void)pin;
    return false;
#endif
}

inline bool lowMemSettledHigh(int pin) FL_NO_EXCEPT {
    delayMicroseconds(100);  // internal pulls are ~50 kOhm; 100 us settles a jumper
    return fl::digitalRead(pin) == fl::PinValue::High;
}

// tx drives, rx observes. Connected only if rx follows its own pull while tx
// is released AND follows tx against that pull, in both directions.
inline bool lowMemPinPairConnected(int tx, int rx) FL_NO_EXCEPT {
    bool ok = true;
    fl::pinMode(tx, fl::PinMode::Input);
    fl::pinMode(rx, fl::PinMode::InputPullup);
    ok = ok && lowMemSettledHigh(rx);              // control: released -> pull-up
    fl::digitalWrite(tx, fl::PinValue::Low);
    fl::pinMode(tx, fl::PinMode::Output);
    ok = ok && !lowMemSettledHigh(rx);             // tx sinks the pull-up
    if (ok && !lowMemOpenDrainPin(tx)) {
        fl::pinMode(rx, fl::PinMode::InputPulldown);
        fl::digitalWrite(tx, fl::PinValue::High);
        ok = lowMemSettledHigh(rx);                // tx sources the pull-down
        fl::pinMode(tx, fl::PinMode::Input);
        ok = ok && !lowMemSettledHigh(rx);         // control: released -> pull-down
    }
    fl::pinMode(tx, fl::PinMode::Input);
    fl::pinMode(rx, fl::PinMode::Input);
    return ok;
}

// Reply: "pairs=<tx>><rx>[od] ...;stuck=<pin><H|L|X> ...;skip=<pin>:<why> ...;od=<pin> ..."
inline fl::string lowMemFindConnectedPins() FL_NO_EXCEPT {
    constexpr int kPinCount = 32;  // PIO0_0..PIO0_31, the runtime pin grid
    fl::sstream skip;
    fl::sstream stuck;
    fl::sstream od;
    fl::u32 probe = 0;
    for (int p = 0; p < kPinCount; ++p) {
        if (const char* why = fl::pinProbeSkipReason(p)) {
            skip << p << ':' << why << ' ';
            continue;
        }
        probe |= 1u << p;
        fl::pinMode(p, fl::PinMode::Input);  // no pull: a partner's pull wins
    }
    // Per-pin control: with every other pin released, a free pin follows its
    // own pull. One that does not is externally driven or loaded; it would
    // fake connections, so it is reported and left out of the pair sweep.
    fl::u32 usable = 0;
    for (int p = 0; p < kPinCount; ++p) {
        if (!(probe & (1u << p))) continue;
        if (lowMemOpenDrainPin(p)) {
            od << p << ' ';
            usable |= 1u << p;
            continue;
        }
        fl::pinMode(p, fl::PinMode::InputPullup);
        const bool up = lowMemSettledHigh(p);
        fl::pinMode(p, fl::PinMode::InputPulldown);
        const bool down = lowMemSettledHigh(p);
        fl::pinMode(p, fl::PinMode::Input);
        if (up && !down) {
            usable |= 1u << p;
        } else {
            stuck << p << (up ? 'H' : (down ? 'X' : 'L')) << ' ';
        }
    }
    fl::sstream pairs;
    for (int tx = 0; tx < kPinCount; ++tx) {
        if (!(usable & (1u << tx))) continue;
        fl::Watchdog::instance().feed();
        for (int rx = 0; rx < kPinCount; ++rx) {
            if (rx == tx || !(usable & (1u << rx)) || lowMemOpenDrainPin(rx)) continue;
            if (lowMemPinPairConnected(tx, rx)) {
                pairs << tx << '>' << rx << (lowMemOpenDrainPin(tx) ? "od" : "") << ' ';
            }
        }
    }
    // Leave normal pads at their reset state (pull-up), open-drain released.
    for (int p = 0; p < kPinCount; ++p) {
        if (!(probe & (1u << p))) continue;
        fl::pinMode(p, lowMemOpenDrainPin(p) ? fl::PinMode::Input
                                             : fl::PinMode::InputPullup);
    }
    fl::sstream out;
    out << "pairs=" << pairs.str() << ";stuck=" << stuck.str()
        << ";skip=" << skip.str() << ";od=" << od.str();
    return out.str();
}

// Compiled only into the `--pin-toggle-rx` bench build: phases.py passes
// FASTLED_AUTORESEARCH_LPC_CORE_PULL_CHECK there. It tests the vendor core's
// ::pinMode, which FastLED itself never calls (it uses fl::pinMode), so the
// shared LPC845 image does not carry it.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_AUTORESEARCH_LPC_CORE_PULL_CHECK)  // ok platform specific - checks the LPC8xx Arduino core's IOCON pull mapping (framework-arduino-lpc8xx#40)
// corePullCheck: checks that the vendor Arduino core's ::pinMode puts the
// pull on the requested pad (FastLED/framework-arduino-lpc8xx#40). Use
// unconnected pins from findConnectedPins. FastLED's own backend first sets
// the opposite pull, so a core that writes the wrong IOCON slot reads back
// the opposite level. Appends "<pin>:up=<0|1>,down=<0|1> " per pin; a
// correct core gives "up=1,down=0". Pads the core cannot pull (outside its
// pin map, open-drain, probe-skipped) report "<pin>:skip ".
//
// On a core with the #40 bug the probe itself changes some other pad's pull,
// as any ::pinMode call there would. A core without INPUT_PULLDOWN treats
// mode 3 as "no pull", so it reports down=1 too.
inline void lowMemCorePullCheck(int pin, fl::sstream& out) FL_NO_EXCEPT {
#ifdef INPUT_PULLDOWN  // ok platform specific - vendor core constant; the check targets the core itself
    constexpr PinMode kCorePulldown = INPUT_PULLDOWN;  // ok platform specific - vendor core constant under test
#else
    constexpr PinMode kCorePulldown = 0x3;  // older cores lack the define
#endif
    out << pin << ':';
    if (pin < 0 || pin >= static_cast<int>(variant_pin_count) ||  // ok platform specific - vendor core's pin-map size bounds the pins it can pull
        lowMemOpenDrainPin(pin) || fl::pinProbeSkipReason(pin)) {
        out << "skip ";
        return;
    }
    const pin_size_t p = static_cast<pin_size_t>(pin);
    fl::pinMode(pin, fl::PinMode::InputPulldown);
    ::pinMode(p, INPUT_PULLUP);  // ok platform specific - the vendor core call under test
    delayMicroseconds(100);
    const int up = ::digitalRead(p) == HIGH ? 1 : 0;  // ok platform specific - read back through the same vendor core
    fl::pinMode(pin, fl::PinMode::InputPullup);
    ::pinMode(p, kCorePulldown);  // ok platform specific - the vendor core call under test
    delayMicroseconds(100);
    const int down = ::digitalRead(p) == HIGH ? 1 : 0;  // ok platform specific - read back through the same vendor core
    fl::pinMode(pin, fl::PinMode::InputPullup);  // reset-state pull
    out << "up=" << up << ",down=" << down << ' ';
}
#endif  // FL_IS_ARM_LPC_845 && FASTLED_AUTORESEARCH_LPC_CORE_PULL_CHECK

#endif  // FL_IS_ARM_LPC && !FASTLED_AUTORESEARCH_IEEE754_MODE && !FAULT_TEST

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_AUTORESEARCH_FAULT_TEST)
FL_NO_RETURN FL_NO_INLINE void triggerLowMemoryNullFault() FL_NO_EXCEPT {
    volatile fl::u32* const p =
        reinterpret_cast<volatile fl::u32*>(0u);  // ok reinterpret cast - fault test
    *p = 0x3302u;
    while (true) {}
}

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(infinite-recursion)
FL_NO_INLINE int recurseLowMemoryStackBlow(int depth) FL_NO_EXCEPT {
    volatile fl::u8 pad[64] = {0};
    pad[static_cast<fl::u8>(depth) & 0x3Fu] = static_cast<fl::u8>(depth);
    // LPC845/M0+ has no stack-limit trap; unbounded recursion can corrupt the
    // runtime before the HardFault handler can emit. Burn stack frames, then
    // force the HardFault path while the diagnostic surface is still intact.
    if (depth >= 32) {
        triggerLowMemoryNullFault();
    }
    return recurseLowMemoryStackBlow(depth + 1) + pad[depth & 0x3F];
}
FL_DISABLE_WARNING_POP
#endif  // FL_IS_ARM_LPC_845 && FASTLED_AUTORESEARCH_FAULT_TEST

}  // namespace

inline void autoResearchLowMemorySetup() {
    fl::serial_begin(115200);

    // Arm the watchdog so a hung sketch unbricks itself on crash.
    fl::Watchdog::instance().begin(3000);

    // Emit a literal-only warning so the autoresearch harness can verify
    // the FL_WARN log pipeline reaches the host.
    FL_WARN_LIT("FL_WARN: low-memory bring-up OK");

    // The C++ inline-ODR rule guarantees a single shared instance across
    // every translation unit that calls this inline function, which is
    // exactly what we want (one Remote bound to the singleton Serial
    // transport). The included-once-per-sketch model of `.ino` builds
    // also reduces to a single TU in practice.
    fl::Remote& remote = autoresearch::low_memory::remoteState().remote;
    remote.bind("echo", [](int v) -> int {
        FL_WARN_LIT("FL_WARN: echo invoked");
        return v;
    });

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_AUTORESEARCH_FAULT_TEST)
    remote.bind("trigger_oom", []() -> bool {
        // Intentional #3302 fault input: ask the 16 KB SRAM target for
        // far more RAM than exists. If the platform allocator returns null
        // instead of trapping, dereference null to exercise HardFault emit.
        volatile char* const p = new char[16UL * 1024UL * 1024UL];  // ok bare allocation
        if (p == nullptr) {
            triggerLowMemoryNullFault();
        }
        p[0] = 0x33;
        triggerLowMemoryNullFault();
        return false;
    });
    remote.bind("trigger_stackblow", []() -> bool {
        (void)recurseLowMemoryStackBlow(0);
        triggerLowMemoryNullFault();
        return false;
    });
#endif  // FL_IS_ARM_LPC_845 && FASTLED_AUTORESEARCH_FAULT_TEST

#if defined(FL_IS_ESP8266)  // ok platform specific - ESP8266 bring-up benches
    autoresearch::esp8266::bindEsp8266Benches(remote);
#endif

#if defined(FASTLED_AUTORESEARCH_IEEE754_MODE)
    remote.bind("ieee754CodecTest", [](const fl::json& args) -> fl::json {
        (void)args;
        const auto r = autoresearch::ieee754_check::run();
        fl::json response = fl::json::object();
        response.set("success", r.success);
        response.set("tests_run", static_cast<int64_t>(r.tests_run));
        response.set("tests_failed", static_cast<int64_t>(r.tests_failed));
        response.set("first_failure", r.first_failure ? r.first_failure : "");
        response.set("expected_bits", static_cast<int64_t>(r.expected_bits));
        response.set("actual_bits", static_cast<int64_t>(r.actual_bits));
        return response;
    });
#endif

#if defined(FL_IS_ARM_LPC) && !defined(FASTLED_AUTORESEARCH_IEEE754_MODE) && \
    !defined(FASTLED_AUTORESEARCH_FAULT_TEST)
#if defined(FL_IS_ARM_LPC_845)  // ok platform specific - only the LPC845 has a pin-probe skip table
    // LPC845 only: it is the LPC with a pin-probe skip table (SWD, RESET,
    // ISP, console USART). Elsewhere the scan would drive those pads.
    remote.bind("findConnectedPins",
                []() -> fl::string { return lowMemFindConnectedPins(); });
#endif

    // pinToggleRx (FastLED #3021 Phase 1) — bit-bang square wave on tx_pin
    // and capture SCT edges on rx_pin. CSV stats out.
#if !defined(FASTLED_AUTORESEARCH_LPC_DMA_LOOPBACK)
    // Left out of the RX-DMA loopback builds (--ws2812-loopback/--pwm-dma-cl):
    // with the DMA capture linked, the 64 KB LPC845 has no room for this
    // unrelated ~1 KB diagnostic. --pin-toggle-rx builds keep it.
    remote.bind("pinToggleRx",
        [](int tx_pin, int rx_pin, int freq_hz, int duration_ms) -> fl::string {
            if (tx_pin < 0 || rx_pin < 0 || freq_hz <= 0 || duration_ms <= 0) {
                return fl::string("0,0,0,0,0,0,0");
            }
            const fl::u32 expected_edges =
                2u * static_cast<fl::u32>(freq_hz) *
                     static_cast<fl::u32>(duration_ms) / 1000u;
            // Keep both the RX vector and diagnostic copy buffer small on
            // 16 KB SRAM LPC8xx parts. Larger static buffers regressed early
            // init by leaving too little room for stack/heap (FastLED #3200),
            // while stack buffers previously overflowed during this handler
            // (FastLED #3125).
            constexpr fl::u32 kLowMemEdgeBufSize = 64u;
            fl::u32 cap = expected_edges + (expected_edges / 2u);
            if (cap < 2u) cap = 2u;
            if (cap > kLowMemEdgeBufSize) cap = kLowMemEdgeBufSize;

            // FastLED's own pin backend (#5260), not the Arduino core's:
            // this bench is the on-silicon check of fl::pinMode/digitalWrite.
            // TX settles LOW before capture starts, so the first captured
            // edge is the first square-wave edge, not a pull-up -> LOW step.
            fl::digitalWrite(tx_pin, fl::PinValue::Low);
            fl::pinMode(tx_pin, fl::PinMode::Output);

            auto rx = fl::LpcSctRxChannel::create(rx_pin);
            if (!rx) return fl::string("0,0,0,0,0,0,0");

            fl::RxConfig cfg;
            cfg.buffer_size = cap;
            cfg.start_low   = true;
            if (!rx->begin(cfg)) return fl::string("0,0,0,0,0,0,0");

            const fl::u32 half_us = 500000u / static_cast<fl::u32>(freq_hz);
            const fl::u32 cycles  = static_cast<fl::u32>(freq_hz) *
                                     static_cast<fl::u32>(duration_ms) / 1000u;

            // Edges are scheduled on absolute SysTick deadlines. Measured on
            // LPC845-BRK: delayMicroseconds(half_us) after each write and poll
            // added that overhead to every half period (+20% at 1 kHz).
            //
            // The capture is polled once right after every write: the SCT
            // holds one capture per direction, so a half period without a
            // poll loses edges. Near the deadline only the clock is read, so
            // a poll (~3 us) does not delay the next edge.
            const fl::u32 ticks_per_us = F_CPU / 1000000u;
            const fl::u32 half_ticks = half_us * ticks_per_us;
            const fl::i32 poll_guard = static_cast<fl::i32>(6u * ticks_per_us);
            LowMemTickClock clock;
            fl::u32 deadline = clock.now();
            for (fl::u32 i = 0; i < 2u * cycles; ++i) {
                fl::digitalWrite(tx_pin, (i & 1u) ? fl::PinValue::Low
                                                  : fl::PinValue::High);
                rx->pollOnce();
                deadline += half_ticks;
                for (;;) {
                    const fl::i32 left = static_cast<fl::i32>(deadline - clock.now());
                    if (left <= 0) break;
                    if (left > poll_guard) rx->pollOnce();
                }
                if ((i & 0xFFu) == 0u) {
                    fl::Watchdog::instance().feed();
                }
            }
            rx->wait(1);

            // Static storage keeps this off the stack; the small fixed size
            // keeps .bss below the LPC845-BRK early-init failure threshold.
            // Size from the array, not a pointer to it: sizeof(pointer) /
            // sizeof(EdgeTime) is 1, which capped every capture at one edge.
            auto& edges_buf = autoresearch::low_memory::pinToggleState().edges;
            const fl::size n_read = rx->getRawEdgeTimes(
                fl::span<fl::EdgeTime>(edges_buf, sizeof(edges_buf) / sizeof(edges_buf[0])));

            LowMemPinTogglePeriodStats stats = computeLowMemPeriodStats(edges_buf, n_read);

            fl::sstream s;
            const bool success = (stats.periods > 0);
            s << (success ? 1 : 0) << ',' << static_cast<fl::u32>(n_read) << ','
              << stats.periods << ',' << stats.mean_ns << ',' << stats.sigma_ns << ','
              << stats.min_ns << ',' << stats.max_ns;
            return s.str();
        });
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_AUTORESEARCH_LPC_CORE_PULL_CHECK)  // ok platform specific - LPC8xx core pull check, --pin-toggle-rx build only
    // corePullCheck: [pin_a, pin_b, pin_c, pin_d] -> "<pin>:up=..,down=.. ..."
    // (pin < 0 = unused slot); `bash autoresearch lpc845 --pin-toggle-rx`
    // defines FASTLED_AUTORESEARCH_LPC_CORE_PULL_CHECK and runs it on
    // scan-idle pins. Other builds leave it unbound; only --pin-toggle-rx
    // calls it, and there a missing binding fails the run.
    remote.bind("corePullCheck",
        [](int pin_a, int pin_b, int pin_c, int pin_d) -> fl::string {
            fl::sstream out;
            const int pins[] = {pin_a, pin_b, pin_c, pin_d};
            for (int pin : pins) {
                if (pin >= 0) {
                    lowMemCorePullCheck(pin, out);
                }
            }
            return out.str();
        });
#endif
#endif  // !FASTLED_AUTORESEARCH_LPC_DMA_LOOPBACK

#if defined(FASTLED_AUTORESEARCH_LPC_WS2812)
    // ws2812SctTest (FastLED #3021 Phase 2) -- WS2812 byte-match loopback.
    remote.bind("ws2812SctTest",
        [](int test_case, int tx_pin, int rx_pin, int capture_ms) -> fl::string {
            // TX/RX come from the host's findConnectedPins scan. An
            // open-drain pad cannot drive TX: it only sinks, and its rising
            // edge is the ~1.7 us pull-up, longer than any WS2812 HIGH.
            // Reserved pads (SWD, ISP, console) are refused: routing USART
            // TXD or the SCT input onto them cuts the link to the host.
            // A rejected request replies with num_leds=0.
            if (test_case < 0 || test_case > 4 || tx_pin < 0 || tx_pin > 31 ||
                rx_pin < 0 || rx_pin > 31 || tx_pin == rx_pin ||
                lowMemOpenDrainPin(tx_pin) || fl::pinProbeSkipReason(tx_pin) ||
                fl::pinProbeSkipReason(rx_pin) || capture_ms <= 0) {
                return fl::string("0,0,0,0,0,0,0,0,0,0");
            }
            int num_leds = 1;
            switch (test_case) {
                case 1: num_leds = 3; break;
                case 4: num_leds = 100; break;
                default: num_leds = 1; break;
            }
            const int expected_bytes = num_leds * 3;
            // Setup failures keep the case fields, with nothing decoded and
            // tx_done=2 (TX never started; 0 means it started but timed out).
            auto setupFailed = [&]() -> fl::string {
                fl::sstream f;
                f << "0," << test_case << ',' << num_leds << ',' << expected_bytes
                  << ",0,0,0,0,0,2";
                return f.str();
            };

            // Wire bytes in WS2812 GRB order: what addLeds<WS2812, PIN, GRB>
            // would hand the driver for these colors at full brightness.
            fl::vector_psram<u8> grb(static_cast<fl::size>(expected_bytes));
            for (int i = 0; i < num_leds; ++i) {
                u8 r = 0, g = 0, b = 0;
                switch (test_case) {
                    case 0: r = 0xFF; break;
                    case 1: r = (i == 0) ? 0xFF : 0; g = (i == 1) ? 0xFF : 0;
                            b = (i == 2) ? 0xFF : 0; break;
                    case 3: r = g = b = 0xFF; break;
                    case 4: r = (i % 3 == 0) ? 0xFF : 0; g = (i % 3 == 1) ? 0xFF : 0;
                            b = (i % 3 == 2) ? 0xFF : 0; break;
                    default: break;  // case 2: all zeros
                }
                grb[i * 3 + 0] = g;
                grb[i * 3 + 1] = r;
                grb[i * 3 + 2] = b;
            }
            autoresearch::low_memory::Ws2812State& ws_state =
                autoresearch::low_memory::ws2812State();

            // The TX pin is a run-time value (from the jumper scan), which
            // addLeds<WS2812, PIN>() cannot take, and the 64 KB part has no
            // room for the runtime Channel API (+21 KB). So drive the frame
            // the way addLeds<> does on a low-memory part (SlimBridgeController
            // direct show): GRB bytes in a ChannelData for the pin, handed to
            // the platform default clockless driver.
            using TxBus = fl::BusTraits<fl::DefaultBus<fl::ClocklessChipset>::value>;
            fl::IChannelDriver& tx = TxBus::instance();
            auto frame = fl::ChannelData::create(
                fl::ChipsetVariant(fl::makeClockless<fl::TIMING_WS2812_800KHZ>(tx_pin)),
                fl::move(grb));
            if (!frame || !tx.canHandle(frame)) return setupFailed();
            // The engine has no abort, so a frame left over from a timed-out
            // case must drain first, or show() would skip this case's frame.
            constexpr fl::u32 kTxDeadlineMs = 100u;
            if (!tx.waitForReady(kTxDeadlineMs)) return setupFailed();
            const u8* expected_buf = frame->getData().data();

            auto rx = fl::LpcSctRxChannel::create(rx_pin);
            if (!rx) return setupFailed();
            fl::RxConfig cfg;
            const fl::u32 expected_edges = (fl::u32)num_leds * 24u * 2u;
            fl::u32 cap = expected_edges + (expected_edges / 2u);
            // The low-memory sketch cannot afford the old 2048-edge reserve
            // on a 16 KB SRAM LPC845. 192 edges covers the 1- and 3-LED
            // cases; the 100-LED case (4800 edges) is checked on the prefix
            // that fits, reported in the `cap` field.
            constexpr fl::u32 kLowMemWs2812EdgeCapacity = 192u;
            if (cap < 48u) cap = 48u;
            if (cap > kLowMemWs2812EdgeCapacity) cap = kLowMemWs2812EdgeCapacity;
            cfg.buffer_size = cap;
            cfg.start_low   = true;
            if (!rx->begin(cfg)) return setupFailed();

            // TX gets its own deadline: a 100-LED frame is ~3 ms on the
            // wire, whatever the RX window. A frame that does not finish
            // fails the case.
            tx.enqueue(frame);
            tx.show();
            const bool tx_done = tx.waitForReady(kTxDeadlineMs);
            rx->wait(capture_ms);
            // Give the TX pad back to GPIO once the frame is out (re-checked
            // after the RX wait), so a later findConnectedPins in this boot
            // sees the jumper, not a USART-held pin.
            if (tx.poll().state == fl::IChannelDriver::DriverState::READY) {
                fl::lpc::LpcUartDmaRuntime::releaseTxPin();  // ok platform specific - only the LPC845 UART-DMA TX holds the pad on a movable SWM function between frames
            }

            uint8_t* decoded_buf = ws_state.decoded;
            for (int i = 0; i < expected_bytes; ++i) decoded_buf[i] = 0;

            // The second argument is the window half-width, not a reset
            // time (passing 0 demanded exact widths). The UART encoder sends
            // whole 250 ns slots -- a 1 is H750/L500 against the nominal
            // T1H 875/T1L 375 -- so the window must exceed that 125 ns.
            constexpr fl::u32 kWs2812DecodeToleranceNs = 200u;
            fl::ChipsetTiming4Phase timing =
                fl::make4PhaseTiming(
                    fl::to_runtime_timing<fl::WS2812ChipsetTiming>(),
                    kWs2812DecodeToleranceNs);

            auto dec = rx->decode(timing, fl::span<u8>(decoded_buf, expected_bytes));
            const fl::u32 decoded_bytes = dec.ok() ? dec.value() : 0u;

            // mismatched counts decoded bytes that differ; bytes that were
            // never decoded are exp_bytes - dec_bytes.
            fl::u32 matched    = 0;
            fl::u32 mismatched = 0;
            for (fl::u32 i = 0; i < decoded_bytes && i < (fl::u32)expected_bytes; ++i) {
                if (decoded_buf[i] == expected_buf[i]) ++matched;
                else                                    ++mismatched;
            }

            // Count every captured edge, reading through a small .bss window
            // (one read would stop at the window size).
            constexpr fl::size kLowMemWs2812ProbeSize = 64u;
            fl::span<fl::EdgeTime> probe(ws_state.probe, kLowMemWs2812ProbeSize);
            fl::size edges_captured = 0;
            for (fl::size n; (n = rx->getRawEdgeTimes(probe, edges_captured)) > 0;) {
                edges_captured += n;
            }

            // A capture of `cap` edges holds cap / 16 bytes (16 edges per
            // byte), so the 100-LED frame is checked on that prefix; the
            // smaller frames fit whole. Same rule as the host bench.
            fl::u32 need = cap / 16u;
            if (need > (fl::u32)expected_bytes) need = (fl::u32)expected_bytes;
            const bool success =
                tx_done && mismatched == 0 && need > 0 && matched >= need;

            fl::sstream s;
            s << (success ? 1 : 0) << ',' << test_case << ',' << num_leds << ','
              << expected_bytes << ',' << decoded_bytes << ','
              << matched << ',' << mismatched << ','
              << static_cast<fl::u32>(edges_captured) << ',' << cap << ','
              << (tx_done ? 1 : 0);
            return s.str();
        });
#endif  // FASTLED_AUTORESEARCH_LPC_WS2812

    // FastLED #3468: channels-API SCT+DMA clockless AutoResearch
    // harness. Binds pwmDmaClFrameOnce / pwmDmaClFrameBurst /
    // pwmDmaClCaptureSelf. Header self-gates on
    // FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)
    autoresearch::pwm_dma_cl::bind(remote);
#endif

    // FastLED #3456: SPI+DMA async AutoResearch harness. Binds
    // dmaSpiTransferOnce / dmaSpiTransferOverlap / dmaSpiMeasureSck.
    // Header self-gates on FL_IS_ARM_LPC_845 && FASTLED_LPC_SPI_DMA.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_SPI_DMA)
    autoresearch::dma_spi::bind(remote);
#endif

    // #3453/#3611: UART DMA AutoResearch harness. Binds either the raw
    // UART DMA stream bench or the clockless loopback verifier, depending
    // on the build flags injected by `bash autoresearch`.
#if defined(FL_IS_ARM_LPC_845) && FASTLED_AUTORESEARCH_LPC_UART_DMA
    autoresearch::uart_dma::bind(remote);
#endif

#endif  // FL_IS_ARM_LPC && !FASTLED_AUTORESEARCH_IEEE754_MODE
}

inline void autoResearchLowMemoryLoop() {
    fl::Watchdog::instance().feed();
    autoresearch::low_memory::remoteState().remote.update(millis());
}
