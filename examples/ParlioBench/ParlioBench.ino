// @filter
// require:
//   - platform: esp32p4
// @end-filter

// ParlioBench.ino — Issue #2493 reproducer for ESP32-P4 PARLIO 16.7ms block.
// Writes show() timing to a global volatile buffer that survives across the
// chip's reset, so we can read it back via esptool's `dump_mem` against
// internal RAM. Also prints to Serial for hosts where USB-Serial-JTAG works.

#include <FastLED.h>

#ifndef NUM_STRIPS
#define NUM_STRIPS 1
#endif
#ifndef NUM_LEDS
#define NUM_LEDS 100
#endif

CRGB leds[NUM_STRIPS][NUM_LEDS];

// 1 KB of timing scratch in DRAM at a known label so esptool can find it
// by reading the symbol table. Each record = 4 × uint32: magic, frame, total_us, max_iter_us.
struct PtRecord {
    uint32_t magic;
    uint32_t frame;
    uint32_t total_us;
    uint32_t reserved;
};
__attribute__((section(".data"))) volatile PtRecord g_pt_log[64] = {};
__attribute__((section(".data"))) volatile uint32_t g_pt_index = 0;
__attribute__((section(".data"))) volatile uint32_t g_pt_marker = 0xCAFEF00D;

template <int PIN, int IDX>
void addLane() {
    FastLED.addLeds<WS2812B, PIN, GRB>(leds[IDX], NUM_LEDS);
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("[ParlioBench] start");

    addLane<0, 0>();
#if NUM_STRIPS > 1
    addLane<1, 1>();
#endif
#if NUM_STRIPS > 2
    addLane<2, 2>(); addLane<3, 3>();
#endif
#if NUM_STRIPS > 4
    addLane<4, 4>(); addLane<5, 5>(); addLane<6, 6>(); addLane<7, 7>();
#endif
#if NUM_STRIPS > 8
    addLane<8, 8>(); addLane<9, 9>(); addLane<10, 10>(); addLane<11, 11>();
#endif

    FastLED.setBrightness(8);
    for (int s = 0; s < NUM_STRIPS; ++s) {
        for (int i = 0; i < NUM_LEDS; ++i) {
            leds[s][i] = CRGB(1, 0, 0);
        }
    }

    // Stamp marker so esptool can locate the buffer in a RAM dump
    g_pt_marker = 0xCAFEF00D;
    g_pt_index = 0;
    Serial.println("[ParlioBench] setup done");
}

void loop() {
    static uint32_t frame = 0;
    uint32_t t0 = micros();
    FastLED.show();
    uint32_t t1 = micros();
    uint32_t dur = t1 - t0;

    // Record into ring buffer (DRAM, persists between resets if power preserved)
    uint32_t idx = g_pt_index % 64;
    g_pt_log[idx].magic = 0xDEADBEEF;
    g_pt_log[idx].frame = frame;
    g_pt_log[idx].total_us = dur;
    g_pt_log[idx].reserved = 0;
    g_pt_index = (g_pt_index + 1);

    Serial.print("EXT show ");
    Serial.print((int)dur);
    Serial.print(" us frame ");
    Serial.println(frame++);
    Serial.flush();
    delay(200);
}
