// AutoResearch edge probe implementation (FastLED#3586 tooling).
// See AutoResearchEdgeProbe.h.

#include "fl/system/sketch_macros.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "AutoResearchEdgeProbe.h"

#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include <Arduino.h>
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"
#include "soc/gpio_reg.h"

namespace {

constexpr fl::u32 kMaxStamps = 200;
volatile fl::u32 g_stamps[kMaxStamps];
volatile fl::u32 g_count = 0;
volatile fl::u32 g_selftest_edges = 0;
volatile fl::u32 g_probe_state = 0; // 1=task started 2=triggered 3=sample done
volatile fl::u32 g_rmt_live[6] = {0};
int g_pin = -1;

// Cycle counter. NOTE: plain `rdcycle` is an ILLEGAL INSTRUCTION on
// Espressif RISC-V cores (C3/C6/P4 don't implement the standard cycle
// CSR — they use a custom performance-counter CSR); esp_cpu_get_cycle_count()
// reads the right register on every ESP32 variant.
static inline fl::u32 probeCycles() {
    return static_cast<fl::u32>(esp_cpu_get_cycle_count());
}

void ARDUINO_ISR_ATTR edgeIsr() {
    const fl::u32 i = g_count;
    if (i < kMaxStamps) {
        // Level BEFORE this edge = inverse of the level now.
        const fl::u32 lvl_now = static_cast<fl::u32>(digitalRead(g_pin));
        g_stamps[i] = ((lvl_now ^ 1u) << 31) | (probeCycles() & 0x7FFFFFFFu);
        g_count = i + 1;
    }
}

} // namespace

void edgeProbeArm(int pin) {
    if (g_pin >= 0) {
        detachInterrupt(digitalPinToInterrupt(g_pin));
    }
    g_count = 0;
    g_pin = pin;
    pinMode(pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(pin), edgeIsr, CHANGE);
    // Self-test: flip the pad's pull resistors to generate edges the
    // ISR must record. Proves the interrupt path end-to-end without
    // touching the output driver (jumpered TX pins stay unharmed).
    for (int i = 0; i < 4; ++i) {
        pinMode(pin, INPUT_PULLUP);
        delayMicroseconds(50);
        pinMode(pin, INPUT_PULLDOWN);
        delayMicroseconds(50);
    }
    g_selftest_edges = g_count;
    g_count = 0; // restart clean for the real capture

    // TX drivers reconfigure the probed pad when the test starts,
    // clearing the input-enable and the interrupt binding. Spawn a
    // one-shot task that re-enables the INPUT stage mid-test (pure
    // IO_MUX IE bit — the output routing is untouched) and re-attaches
    // the interrupt, so patterns after the first get recorded.
    xTaskCreate([](void *arg) {
        const int pin = static_cast<int>(reinterpret_cast<intptr_t>(arg));
        g_probe_state = 1;
        vTaskDelay(pdMS_TO_TICKS(150));
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin]);
        // Triggered tight-loop run-length sampler: per-edge interrupts
        // can't resolve sub-µs LED pulses (~4 µs ISR latency), but a
        // spin loop reading GPIO_IN can. TX is DMA/ISR-driven, so
        // starving lower-priority tasks doesn't stop the waveform.
        fl::u32 mask;
        volatile fl::u32 *in_reg;
        if (pin < 32) {
            mask = 1u << pin;
            in_reg = reinterpret_cast<volatile fl::u32 *>(GPIO_IN_REG); // ok reinterpret cast - SoC GPIO input register
        } else {
#ifdef GPIO_IN1_REG
            mask = 1u << (pin - 32);
            in_reg = reinterpret_cast<volatile fl::u32 *>(GPIO_IN1_REG); // ok reinterpret cast - SoC GPIO input register (pins 32+)
#else
            mask = 1u << (pin & 31);
            in_reg = reinterpret_cast<volatile fl::u32 *>(GPIO_IN_REG); // ok reinterpret cast - SoC GPIO input register
#endif
        }
        const fl::u32 idle = *in_reg & mask;
        // Wait up to ~4 s for the line to move. LED frames can be
        // shorter than a tick, so poll in tight ~900 µs bursts with a
        // 1-tick yield between them (a lone vTaskDelay(1) poll misses
        // whole frames and only sees the settled DC level).
        bool triggered = false;
        const fl::u32 burst_cycles = edgeProbeCpuMhz() * 900u;
        for (int t = 0; t < 2000 && !triggered; ++t) {
            // Drivers may clear the pad's input-enable on every
            // pattern re-init — keep re-asserting it.
            PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin]);
            const fl::u32 b0 = probeCycles();
            const fl::u32 lvl0 = *in_reg & mask;
            while (probeCycles() - b0 < burst_cycles) {
                if ((*in_reg & mask) != lvl0) { triggered = true; break; }
            }
            if (!triggered) {
                vTaskDelay(1);
            }
        }
        if (triggered) {
            g_probe_state = 2;
#if defined(CONFIG_IDF_TARGET_ESP32C6)
            // Live RMT-RX registers at the instant the waveform is on
            // the wire (FastLED#3586).
            g_rmt_live[0] = *reinterpret_cast<volatile fl::u32 *>(0x60006038); // ok reinterpret cast - RMT_INT_RAW
            g_rmt_live[1] = *reinterpret_cast<volatile fl::u32 *>(0x60006030); // ok reinterpret cast - CH2STATUS
            g_rmt_live[2] = *reinterpret_cast<volatile fl::u32 *>(0x60006034); // ok reinterpret cast - CH3STATUS
            g_rmt_live[3] = *reinterpret_cast<volatile fl::u32 *>(0x60006024); // ok reinterpret cast - CH2 RX_CONF1
#endif
            fl::u32 idx = 0;
            fl::u32 prev = *in_reg & mask;
            fl::u32 last_edge = probeCycles();
            const fl::u32 start = last_edge;
            const fl::u32 budget = edgeProbeCpuMhz() * 1000u * 8u; // 8 ms
            for (;;) {
                const fl::u32 now = probeCycles();
                if (now - start > budget) break;
                const fl::u32 cur = *in_reg & mask;
                if (cur != prev) {
                    if (idx < kMaxStamps) {
                        g_stamps[idx] = ((prev ? 1u : 0u) << 31) | (now & 0x7FFFFFFFu);
                        ++idx;
                    }
                    last_edge = now;
                    prev = cur;
                }
            }
            g_count = idx;
#if defined(CONFIG_IDF_TARGET_ESP32C6)
            // Second snapshot ~8 ms later (post-frame): did the RMT
            // receiver's interrupt lines fire at all during the frame?
            g_rmt_live[4] = *reinterpret_cast<volatile fl::u32 *>(0x60006038); // ok reinterpret cast - RMT_INT_RAW post
            g_rmt_live[5] = *reinterpret_cast<volatile fl::u32 *>(0x60006030); // ok reinterpret cast - CH2STATUS post
#endif
            g_probe_state = 3;
        }
        vTaskDelete(nullptr);
    }, "edgeprobe_rearm", 3072, reinterpret_cast<void *>(static_cast<intptr_t>(pin)), 20, nullptr);
}

const fl::u32 *edgeProbeStamps(fl::u32 &count) {
    count = g_count;
    return const_cast<const fl::u32 *>(g_stamps);
}

fl::u32 edgeProbeSelfTestEdges() { return g_selftest_edges; }

fl::u32 edgeProbeState() { return g_probe_state; }

const fl::u32 *edgeProbeRmtLive() { return const_cast<const fl::u32 *>(g_rmt_live); }

fl::u32 edgeProbeCpuMhz() {
    return static_cast<fl::u32>(getCpuFrequencyMhz());
}

#else // !FL_IS_ESP32

void edgeProbeArm(int) {}
const fl::u32 *edgeProbeStamps(fl::u32 &count) {
    count = 0;
    return nullptr;
}
fl::u32 edgeProbeCpuMhz() { return 1; }
fl::u32 edgeProbeSelfTestEdges() { return 0; }
fl::u32 edgeProbeState() { return 0; }
const fl::u32 *edgeProbeRmtLive() { static const fl::u32 z[4] = {0}; return z; }

#endif // FL_IS_ESP32

#endif // SKETCH_HAS_LOTS_OF_MEMORY
