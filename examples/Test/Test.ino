/// @file    Test.ino
/// @brief   Runtime tests for FastLED 8-bit math and array operations
/// @example Test.ino



#include <Arduino.h>
#include <FastLED.h>


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


void setup() {
    Serial.begin(115200);  // Fast baud for avr8js
    Serial.println("Test setup starting");
    legacyLibcOnCrgb();


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
