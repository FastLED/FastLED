/// @file    Test.ino
/// @brief   Runtime tests for FastLED 8-bit math and array operations
/// @example Test.ino



#include <Arduino.h>
#include <FastLED.h>


// Test configuration


void setup() {
    Serial.begin(115200);  // Fast baud for avr8js
    Serial.println("SETUP COMPLETE");


    delay(100);
}

void loop() {
    EVERY_N_MILLIS(1000) {
        Serial.println("Test loop!");
    }
    delay(1);  // Prevent watchdog timeout on ESP32 qemu tests.
}
