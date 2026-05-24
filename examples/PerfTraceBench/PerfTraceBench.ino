// @filter
// require:
//   - platform: esp32p4
// @end-filter

// PerfTraceBench.ino — fl::PerfTrace sweep for ESP32-P4 PARLIO (issue #2493 follow-up).
//
// Why this exists:
//   Same dev-env constraint as PR #2494: the USB-Serial-JTAG port on COM25 stops
//   producing host-visible bytes once the Arduino app boots, so RPC-driven
//   autoresearch can't actually read frame timings back. This sketch borrows the
//   ParlioBench workaround — write the perf log to a flash data partition, then
//   `esptool read-flash` it from the host. Once issue #2507 is resolved we can
//   delete this and drive the same data via the AutoResearch sketch.
//
// What it does:
//   1. Wires up NUM_STRIPS WS2812B lanes (default 12 × 256 LEDs each).
//   2. Flips fl::PerfTrace::set_enabled(true).
//   3. Calls FastLED.show() N times back-to-back (no inter-frame delay).
//   4. For each frame, records (frame_idx, show_us, event_count, dropped) plus
//      every captured perf event (depth, dur_us, start_us, name_idx, line).
//   5. Once the buffer fills, writes it to a flash data partition and halts.
//
// Read it back (host side):
//   esptool --chip esp32p4 --port COM25 read-flash <part_offset> <size> dump.bin
//   uv run ci/perf_trace_decode.py dump.bin
//
// Required compile flag:
//   -D FASTLED_PERF_TRACE=1 (already set in platformio.ini esp32p4 env)

#include <Arduino.h>
#include <FastLED.h>
#include <esp_partition.h>

#include "fl/system/perf_trace.h"

#ifndef NUM_STRIPS
#define NUM_STRIPS 12
#endif
#ifndef NUM_LEDS
#define NUM_LEDS 256
#endif
#ifndef NUM_FRAMES
#define NUM_FRAMES 16
#endif

// On the ESP32-P4-EV, GPIOs 0..15 are safe for PARLIO lanes.
// We allocate strips on pins 0..NUM_STRIPS-1 in order.
static CRGB g_leds[NUM_STRIPS][NUM_LEDS];

// ---------------------------------------------------------------------------
// On-flash record layout (little-endian throughout).
//
//   header (16B): magic=0xFA571DED, version=1, num_frames, total_event_bytes
//   frame_record (16B): magic=0xF8AAE000, frame_idx, show_us, event_count,
//                       dropped (1B), pad (3B)
//   event_record (16B): magic=0xE0E07E07, depth (i16), line (i16), dur_us,
//                       start_us, name_offset (into name_table)
//   name_table: u32 name_count, then [u16 len, u8 bytes[len]] repeated
// ---------------------------------------------------------------------------
struct __attribute__((packed)) Header {
    uint32_t magic;          // 0xFA571DED
    uint16_t version;        // 1
    uint16_t num_frames;
    uint32_t total_bytes;    // total bytes following the header (frames + events + name_table)
    uint32_t reserved;
};

struct __attribute__((packed)) FrameRec {
    uint32_t magic;          // 0xF8AAE000
    uint32_t frame_idx;
    uint32_t show_us;
    uint16_t event_count;
    uint8_t  dropped;
    uint8_t  pad;
};

struct __attribute__((packed)) EventRec {
    uint32_t magic;          // 0xE0E07E07
    int16_t  depth;
    int16_t  line;
    uint32_t dur_us;
    uint32_t start_us;
    uint32_t name_offset;
};

// ---------------------------------------------------------------------------
// In-RAM staging buffer
// ---------------------------------------------------------------------------
static constexpr size_t kBufferBytes = 96 * 1024;
static uint8_t g_buf[kBufferBytes];
static size_t  g_used = 0;

// Tiny string interning for event names — names are __FILE__ + name literals
// that recur. We collect them once, then emit a single name table.
static constexpr size_t kMaxNames = 256;
static const char* g_name_ptrs[kMaxNames];
static size_t      g_name_count = 0;
static uint32_t    g_name_offsets[kMaxNames];

static uint32_t intern_name(const char* s) {
    if (!s) return 0xFFFFFFFFu;
    for (size_t i = 0; i < g_name_count; ++i) {
        if (g_name_ptrs[i] == s) return static_cast<uint32_t>(i);
    }
    if (g_name_count >= kMaxNames) return 0xFFFFFFFFu;
    g_name_ptrs[g_name_count] = s;
    return static_cast<uint32_t>(g_name_count++);
}

static bool append(const void* src, size_t n) {
    if (g_used + n > kBufferBytes) return false;
    memcpy(g_buf + g_used, src, n);
    g_used += n;
    return true;
}

// ---------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------
template <int PIN, int IDX> struct AddLane {
    static void run() { FastLED.addLeds<WS2812B, PIN, GRB>(g_leds[IDX], NUM_LEDS); }
};

static void addAllLanes() {
    AddLane<0, 0>::run();
#if NUM_STRIPS > 1
    AddLane<1, 1>::run();
#endif
#if NUM_STRIPS > 2
    AddLane<2, 2>::run();
    AddLane<3, 3>::run();
#endif
#if NUM_STRIPS > 4
    AddLane<4, 4>::run();
    AddLane<5, 5>::run();
    AddLane<6, 6>::run();
    AddLane<7, 7>::run();
#endif
#if NUM_STRIPS > 8
    AddLane<8, 8>::run();
    AddLane<9, 9>::run();
    AddLane<10, 10>::run();
    AddLane<11, 11>::run();
#endif
#if NUM_STRIPS > 12
#error "Extend addAllLanes() for >12 strips"
#endif
}

// ---------------------------------------------------------------------------
// Flash flush
// ---------------------------------------------------------------------------
static bool flushed = false;
static uint32_t flush_part_offset = 0;
static uint32_t flush_part_size = 0;

static void flushToFlash() {
    if (flushed) return;

    // Append the name table at the end of g_buf.
    uint32_t name_count_u32 = static_cast<uint32_t>(g_name_count);
    append(&name_count_u32, sizeof(name_count_u32));
    for (size_t i = 0; i < g_name_count; ++i) {
        const char* s = g_name_ptrs[i];
        uint16_t len = static_cast<uint16_t>(strlen(s));
        if (len > 256) len = 256;
        g_name_offsets[i] = static_cast<uint32_t>(g_used);
        append(&len, sizeof(len));
        append(s, len);
    }

    // Find a writable data partition.
    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (!part) {
        flushed = true;
        return;
    }

    flush_part_offset = part->address;
    flush_part_size   = static_cast<uint32_t>(g_used);

    // Erase to a 4KB-aligned multiple covering g_used.
    uint32_t erase_size = (static_cast<uint32_t>(g_used) + 4095) & ~4095u;
    if (erase_size > part->size) erase_size = part->size;
    esp_partition_erase_range(part, 0, erase_size);
    esp_partition_write(part, 0, g_buf, g_used);

    flushed = true;
}

// ---------------------------------------------------------------------------
// Arduino lifecycle
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    // Long delay so USB-Serial-JTAG CDC has time to enumerate AND for the
    // host's `pio device monitor`/Python `serial.Serial` to (re)open the
    // port. Without this, the first ~1-2s of Serial output is lost on
    // ESP32-P4-EV's USB-Serial-JTAG bridge. See PR #2494 / #2507.
    delay(5000);
    Serial.println("[PerfTraceBench] start");
    Serial.flush();

    addAllLanes();
    FastLED.setBrightness(8);
    for (int s = 0; s < NUM_STRIPS; ++s) {
        for (int i = 0; i < NUM_LEDS; ++i) {
            g_leds[s][i] = CRGB(1, 0, 0);
        }
    }

    // Reserve room for header — written last with final byte count.
    Header h{};
    h.magic = 0xFA571DEDu;
    h.version = 1;
    h.num_frames = 0;
    h.total_bytes = 0;
    h.reserved = 0;
    append(&h, sizeof(h));

    Serial.println("[PerfTraceBench] setup done");
}

static uint32_t g_frame = 0;

void loop() {
    if (flushed) {
        delay(1000);
        return;
    }
    if (g_frame >= NUM_FRAMES) {
        // Patch header (num_frames, total_bytes) before flush.
        Header h{};
        h.magic = 0xFA571DEDu;
        h.version = 1;
        h.num_frames = static_cast<uint16_t>(NUM_FRAMES);
        h.total_bytes = static_cast<uint32_t>(g_used - sizeof(Header));
        memcpy(g_buf, &h, sizeof(h));

        flushToFlash();
        Serial.print("[PerfTraceBench] flushed ");
        Serial.print((int)g_used);
        Serial.print(" bytes to partition at 0x");
        Serial.println((unsigned int)flush_part_offset, HEX);
        return;
    }

    // Clear the trace log and enable for this frame only.
    fl::PerfTrace::clear();
    fl::PerfTrace::set_enabled(true);

    const uint32_t t0 = micros();
    FastLED.show();
    const uint32_t t1 = micros();
    const uint32_t show_us = t1 - t0;

    fl::PerfTrace::set_enabled(false);

    const fl::size n = fl::PerfTrace::event_count();
    const fl::PerfEvent* evs = fl::PerfTrace::events();
    const bool dropped = fl::PerfTrace::dropped();

    // Frame record
    FrameRec fr{};
    fr.magic = 0xF8AAE000u;
    fr.frame_idx = g_frame;
    fr.show_us = show_us;
    fr.event_count = static_cast<uint16_t>(n);
    fr.dropped = dropped ? 1 : 0;
    fr.pad = 0;
    append(&fr, sizeof(fr));

    // Events
    for (fl::size i = 0; i < n; ++i) {
        EventRec er{};
        er.magic    = 0xE0E07E07u;
        er.depth    = static_cast<int16_t>(evs[i].depth);
        er.line     = static_cast<int16_t>(evs[i].line);
        er.dur_us   = evs[i].end_us - evs[i].start_us;
        er.start_us = evs[i].start_us;
        er.name_offset = intern_name(evs[i].name);
        append(&er, sizeof(er));
    }

    Serial.print("[PerfTraceBench] frame ");
    Serial.print((int)g_frame);
    Serial.print(" show_us=");
    Serial.print((int)show_us);
    Serial.print(" events=");
    Serial.print((int)n);
    Serial.print(" dropped=");
    Serial.println((int)dropped);
    Serial.flush();

    g_frame++;
}
