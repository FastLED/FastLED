/// @file    Test.ino
/// @brief   Runtime tests for FastLED 8-bit math and array operations
/// @example Test.ino



#include <Arduino.h>
#include <FastLED.h>

// drawPolygon AVR-vs-host hash check (#5324): 16-bit int on AVR only. Both
// polygon kernels add ~24 KB, so run it on the Uno (ATmega328P, the avr8js
// target: 27 KB of 32 KB) and on 48 KB+ parts; a Leonardo's USB stack would
// not fit.
#if defined(FL_IS_AVR) && defined(FLASHEND) && \
    (defined(__AVR_ATmega328P__) || (FLASHEND >= 0xBFFF))
#define TEST_POLYGON_HASHES 1
#include "PolygonHashes.h"
#else
#define TEST_POLYGON_HASHES 0
#endif


// Test configuration

// Compile-time regression for FastLED#4827: legacy sketches call libc memory
// functions unqualified on CRGB*. These must stay unambiguous on every board.
static void legacyLibcOnCrgb() {
    CRGB src[4] = {CRGB(1, 2, 3), CRGB(4, 5, 6), CRGB(7, 8, 9), CRGB(10, 11, 12)};
    CRGB* dst = static_cast<CRGB*>(malloc(sizeof(CRGB) * 4));  // ok bare allocation
    if (!dst) {
        return;
    }
    memset(dst, 0, sizeof(CRGB) * 4);
    memcpy(dst, src, sizeof(CRGB) * 4);
    memmove(dst + 1, dst, sizeof(CRGB) * 3);
    if (memcmp(dst, src, sizeof(CRGB)) != 0) {
        Serial.println("legacy libc copy mismatch");
    }
    free(dst);  // ok bare allocation
}

#if TEST_POLYGON_HASHES
// Prints every scene's hash; on any difference from the host golden table
// prints "polygon hash mismatch", which the avr8js runner treats as failure.
static void checkPolygonHashes() {
    CRGB buf[polygon_hashes::kWidth * polygon_hashes::kHeight];
    bool ok = true;
    for (int scene = 0; scene < polygon_hashes::kSceneCount; ++scene) {
        for (int k = 0; k < 2; ++k) {
            const bool abuffer = k == 1;
            const fl::u32 got = polygon_hashes::renderHash(scene, abuffer, buf);
            const fl::u32 want = polygon_hashes::goldenHash(scene, abuffer);
            Serial.print("polygon scene ");
            Serial.print(scene);
            Serial.print(abuffer ? " abuffer 0x" : " exact 0x");
            Serial.print(static_cast<unsigned long>(got), HEX);
            if (got != want) {
                Serial.print(" polygon hash mismatch, host 0x");
                Serial.print(static_cast<unsigned long>(want), HEX);
                ok = false;
            }
            Serial.println();
        }
    }
    if (ok) {
        Serial.println("polygon hashes match host");
    }
}
#endif

void setup() {
    Serial.begin(115200);  // Fast baud for avr8js
    Serial.println("Test setup starting");
    legacyLibcOnCrgb();
#if TEST_POLYGON_HASHES
    checkPolygonHashes();
#endif


    delay(100);
}

void loop() {
    EVERY_N_MILLIS(1000) {
        Serial.println("Test loop!");
    }
#if defined(FASTLED_ESP32_IS_QEMU)
    // Busy-wait under Espressif QEMU. delay() sleeps in vTaskDelay until a
    // timer-alarm tick wakes the task; on a loaded host the emulator
    // sometimes never delivers that wake-up and loop() stalls for good
    // (FastLED#4845: ~1 s into the run, every local repro on esp32c3). The
    // EVERY_N_MILLIS above still exercises millis().
    delayMicroseconds(1000);
#else
    delay(1);  // Yield to the idle task (feeds the task watchdog).
#endif
}
