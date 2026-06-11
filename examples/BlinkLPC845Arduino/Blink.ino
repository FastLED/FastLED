/// @file    Blink.ino
/// @brief   Blink the first LED of an LED strip
/// @example Blink.ino

#include <Arduino.h>

void setup() {
    // Test Serial initialization
    Serial.begin(115200);
    Serial.println("\n==============================================");
    Serial.println("   BLINK.INO - Simple LED Blink Example");
    Serial.println("==============================================\n");
    Serial.flush();
}

void loop() {
    static uint32_t count = 0;
    count++;

    // Test Serial.print (uses Print::write virtual function)
    Serial.print("Loop: ");
    Serial.println(count);

    // Test delay (requires SysTick)
    delay(1000);
}
