// @filter
// require:
//   - platform: esp32p4
// @end-filter

// ParlioBench.ino — Issue #2493 reproducer.
// Writes show() timing to flash so we can read it back via esptool — bypasses
// the unreliable USB-Serial-JTAG path on some Windows hosts.
//
// After running for ~5s:
//   esptool --port COMxx --chip esp32p4 read-flash 0x3F0000 1024 dump.bin
// Decode: magic 0xDEADBEEF at offset 0, then 16-byte records of
//   {uint32 magic, uint32 frame, uint32 total_us, uint32 wfc_iters}

#include <FastLED.h>
#include <esp_partition.h>

#ifndef NUM_STRIPS
#define NUM_STRIPS 1
#endif
#ifndef NUM_LEDS
#define NUM_LEDS 100
#endif

CRGB leds[NUM_STRIPS][NUM_LEDS];

// Fixed flash log address — well into unused space, before NVS partitions
static const uint32_t LOG_FLASH_OFFSET = 0x3F0000;
static const size_t LOG_RECORD_SIZE = 16;
static const size_t LOG_MAX_RECORDS = 60;

struct PtRecord {
    uint32_t magic;       // 0xDEADBEEF
    uint32_t frame;
    uint32_t total_us;
    uint32_t reserved;
};

static uint8_t g_log_buffer[LOG_RECORD_SIZE * LOG_MAX_RECORDS];
static uint32_t g_frame = 0;
static bool g_flushed = false;

template <int PIN, int IDX>
void addLane() {
    FastLED.addLeds<WS2812B, PIN, GRB>(leds[IDX], NUM_LEDS);
}

void flushToFlash() {
    if (g_flushed) return;
    g_flushed = true;
    // Find a data partition we can write to. Use the larger "spiffs" partition
    // or any data partition with enough space.
    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (!part) return;
    esp_partition_erase_range(part, 0, 4096);
    esp_partition_write(part, 0, g_log_buffer, sizeof(g_log_buffer));
    Serial.println("[ParlioBench] flushed to flash");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
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
    FastLED.setMaxRefreshRate(60);  // Force 60Hz gate to test if that's user's bug
    for (int s = 0; s < NUM_STRIPS; ++s) {
        for (int i = 0; i < NUM_LEDS; ++i) {
            leds[s][i] = CRGB(1, 0, 0);
        }
    }
    Serial.println("[ParlioBench] setup done");
}

void loop() {
    uint32_t t0 = micros();
    FastLED.show();
    uint32_t t1 = micros();
    uint32_t dur = t1 - t0;

    if (g_frame < LOG_MAX_RECORDS) {
        PtRecord* rec = (PtRecord*)(g_log_buffer + (g_frame * LOG_RECORD_SIZE));
        rec->magic = 0xDEADBEEF;
        rec->frame = g_frame;
        rec->total_us = dur;
        rec->reserved = 0;
    }

    Serial.print("EXT show ");
    Serial.print((int)dur);
    Serial.print(" us frame ");
    Serial.println(g_frame);
    Serial.flush();

    g_frame++;

    if (g_frame == LOG_MAX_RECORDS) {
        flushToFlash();
        Serial.println("[ParlioBench] HALTED — read with esptool");
    }
    if (g_frame >= LOG_MAX_RECORDS) {
        // Pulse activity so we know we're alive
        delay(1000);
        return;
    }
    // NO DELAY — back-to-back show() to force waitForReady() to spin on prior DMA
}
