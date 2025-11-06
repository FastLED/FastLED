/// @file FastPinsSamePort.ino
/// @brief Example demonstrating FastPinsSamePort for ultra-fast multi-pin GPIO
///
/// This example shows how to use FastPinsSamePort for high-speed parallel GPIO
/// operations. All pins must be on the same GPIO port/bank for optimal performance.
///
/// MEMORY USAGE WARNING:
/// - FastPinsSamePort<8> uses ~2KB RAM (256-entry LUT)
/// - Arduino UNO (2KB total RAM): This example uses 112% of available RAM!
/// - For UNO, use FastPinsSamePort<4> (16-entry LUT = 128 bytes) instead
/// - MEGA/ESP32/RP2040 have enough RAM for FastPinsSamePort<8>

#include <FastLED.h>

// Create FastPinsSamePort instance for 8 pins
// NOTE: On UNO, consider using FastPinsSamePort<4> to reduce RAM usage
fl::FastPinsSamePort<8> multiPins;

void setup() {
    Serial.begin(115200);
    Serial.println("FastPinsSamePort Example");

#if defined(ESP32)
    // ESP32: Configure 8 pins on same bank (all in bank 0: pins 0-31)
    multiPins.setPins(2, 4, 5, 12, 13, 14, 15, 16);
    Serial.println("Configured 8 pins on ESP32 (bank 0)");
#elif defined(ARDUINO_ARCH_RP2040)
    // RP2040: All pins on single bank
    multiPins.setPins(2, 3, 4, 5, 6, 7, 8, 9);
    Serial.println("Configured 8 pins on RP2040");
#elif defined(ARDUINO_ARCH_STM32)
    // STM32: Configure 8 pins on same port (GPIOA)
    multiPins.setPins(PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7);
    Serial.println("Configured 8 pins on STM32 (GPIOA)");
#elif defined(__AVR__)
    // AVR: Configure 8 pins on same PORT (PORTD)
    multiPins.setPins(0, 1, 2, 3, 4, 5, 6, 7);
    Serial.println("Configured 8 pins on AVR (PORTD)");
#else
    // Generic: Try first 8 pins
    multiPins.setPins(0, 1, 2, 3, 4, 5, 6, 7);
    Serial.println("Configured 8 pins (generic)");
#endif

    Serial.print("Pin count: ");
    Serial.println(multiPins.getPinCount());
}

void loop() {
    // Write various patterns to demonstrate fast parallel GPIO

    // All LOW
    multiPins.write(0x00);
    delayMicroseconds(100);

    // All HIGH
    multiPins.write(0xFF);
    delayMicroseconds(100);

    // Alternating pattern 1
    multiPins.write(0b10101010);
    delayMicroseconds(100);

    // Alternating pattern 2
    multiPins.write(0b01010101);
    delayMicroseconds(100);

    // Binary counter pattern (animate 8 bits)
    for (uint8_t i = 0; i < 256; i++) {
        multiPins.write(i);
        delayMicroseconds(10);
    }

    delay(100);
}
