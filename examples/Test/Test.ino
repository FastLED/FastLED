/// @file    Test.ino
/// @brief   Runtime tests for FastLED 8-bit math and array operations
/// @example Test.ino



#include <Arduino.h>
#include <FastLED.h>


// Test configuration

// Compile-time regression for FastLED#4827: legacy sketches call libc memory
// functions unqualified on CRGB*. These must stay unambiguous on every board.
static void legacyLibcOnCrgb() {
    CRGB src[4];
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
    delay(1);  // Prevent watchdog timeout on ESP32 qemu tests.
}
