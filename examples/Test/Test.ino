/// @file    Test.ino
/// @brief   Runtime tests for FastLED 8-bit math and array operations
/// @example Test.ino



#include <Arduino.h>
#include <FastLED.h>


// Test configuration


void setup() {
    Serial.begin(115200);  // Fast baud for avr8js

    Serial.print("FastLED Runtime Test Suite - begin setup\n");
    Serial.print("===========================\n");



}

void loop() {
    EVERY_N_MILLIS(1000) {
        Serial.println("Test loop!");
    }
}
