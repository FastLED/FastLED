/// @file    SerialTest.ino
/// @brief   Minimal serial test for LPC845

#include <Arduino.h>

void setup() {
    // Initialize serial with a delay to let USB enumerate
    delay(2000);
    Serial.begin(115200);
    delay(500);
    
    // Send startup message
    Serial.println("\n\n========== LPC845 SERIAL TEST ==========");
    Serial.println("Board: LPC845-BRK");
    Serial.println("Test: Basic serial communication");
    Serial.println("========================================\n");
}

void loop() {
    static uint32_t counter = 0;
    
    Serial.print("Loop #");
    Serial.print(counter++);
    Serial.print(" - millis: ");
    Serial.println(millis());
    
    delay(1000);
}
