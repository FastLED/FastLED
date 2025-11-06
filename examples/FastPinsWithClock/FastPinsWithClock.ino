/// @file FastPinsWithClock.ino
/// @brief Example demonstrating FastPinsWithClock for 8-data + 1-clock parallel SPI
///
/// This example shows how to use FastPinsWithClock for high-speed parallel communication
/// with 8 data lines and 1 shared clock signal. This is ideal for:
/// - Multi-SPI LED controllers (8 parallel strips with shared clock)
/// - APA102/DotStar parallel output
/// - Custom high-speed parallel protocols
///
/// Performance Modes:
/// 1. Standard Mode (writeWithClockStrobe): ~40ns per write
/// 2. Zero-Delay Mode (writeDataAndClock + NOPs): 60-76ns per bit = 13-17 MHz
///
/// Platform Support:
/// - ESP32/ESP32-S3: All 9 pins must be in same GPIO bank (0-31 or 32-63)
/// - RP2040/RP2350: All pins valid (single GPIO bank)
/// - STM32: All 9 pins must be on same GPIO port (GPIOA, GPIOB, etc.)
/// - Teensy 4.x: All 9 pins must be on same GPIO port (GPIO6-9)
/// - AVR: All 9 pins must be on same PORT (PORTA, PORTB, etc.)

#include <FastLED.h>

// Platform-specific pin configurations
#if defined(ESP32)
    // ESP32: Use GPIO bank 0 (pins 0-31) for all 9 pins
    const uint8_t CLOCK_PIN = 17;
    const uint8_t DATA_PINS[8] = {2, 4, 5, 12, 13, 14, 15, 16};
    const char* PLATFORM_NAME = "ESP32";

#elif defined(__arm__) && (defined(PICO_RP2040) || defined(PICO_RP2350))
    // RP2040/RP2350: All pins on single GPIO bank
    const uint8_t CLOCK_PIN = 10;
    const uint8_t DATA_PINS[8] = {2, 3, 4, 5, 6, 7, 8, 9};
    const char* PLATFORM_NAME = "RP2040/RP2350";

#elif defined(FASTLED_TEENSY4) && defined(__IMXRT1062__)
    // Teensy 4.x: Use GPIO6 (pins 14-27) for all 9 pins
    const uint8_t CLOCK_PIN = 14;
    const uint8_t DATA_PINS[8] = {15, 16, 17, 18, 19, 20, 21, 22};
    const char* PLATFORM_NAME = "Teensy 4.x";

#elif defined(STM32F1) || defined(STM32F2) || defined(STM32F4)
    // STM32: Use GPIOA for all 9 pins (PA0-PA8)
    const uint8_t CLOCK_PIN = PA0;
    const uint8_t DATA_PINS[8] = {PA1, PA2, PA3, PA4, PA5, PA6, PA7, PA8};
    const char* PLATFORM_NAME = "STM32";

#elif defined(__AVR__)
    // AVR: Use PORTD for all 9 pins (digital pins 0-7 + 8)
    // Note: Serial uses pins 0-1, so use pins 2-9 on UNO
    const uint8_t CLOCK_PIN = 2;
    const uint8_t DATA_PINS[8] = {3, 4, 5, 6, 7, 8, 9, 10};
    const char* PLATFORM_NAME = "AVR";

#else
    // Default/fallback
    const uint8_t CLOCK_PIN = 10;
    const uint8_t DATA_PINS[8] = {2, 3, 4, 5, 6, 7, 8, 9};
    const char* PLATFORM_NAME = "Unknown";
    #warning "Using default pin configuration - may not work on all platforms"
#endif

// Create FastPinsWithClock instance
fl::FastPinsWithClock<8> spi;

void setup() {
    // Initialize serial for debug output
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Wait for serial port (or timeout after 3s)
    }

    Serial.println("==================================");
    Serial.println("FastPinsWithClock Example");
    Serial.println("==================================");
    Serial.print("Platform: ");
    Serial.println(PLATFORM_NAME);
    Serial.println();

    // Configure 8 data pins + 1 clock pin
    Serial.println("Configuring FastPinsWithClock...");
    Serial.print("Clock pin: ");
    Serial.println(CLOCK_PIN);
    Serial.print("Data pins: ");
    for (int i = 0; i < 8; i++) {
        Serial.print(DATA_PINS[i]);
        if (i < 7) Serial.print(", ");
    }
    Serial.println();

    // Configure pins (clock pin first, then 8 data pins)
    spi.setPins(CLOCK_PIN,
                DATA_PINS[0], DATA_PINS[1], DATA_PINS[2], DATA_PINS[3],
                DATA_PINS[4], DATA_PINS[5], DATA_PINS[6], DATA_PINS[7]);

    Serial.println("Configuration complete!");
    Serial.println();
    Serial.println("Starting pattern demonstrations...");
    Serial.println("----------------------------------");
}

void loop() {
    // Demo 1: Binary counter pattern
    Serial.println("Demo 1: Binary counter (0-255)");
    for (uint16_t i = 0; i < 256; i++) {
        spi.writeWithClockStrobe(i);  // Standard mode: 40ns per write
        delayMicroseconds(10);         // 10µs between writes for observation
    }
    delay(1000);

    // Demo 2: Walking ones pattern (single bit marching)
    Serial.println("Demo 2: Walking ones pattern");
    for (uint8_t bit = 0; bit < 8; bit++) {
        uint8_t pattern = 1 << bit;
        spi.writeWithClockStrobe(pattern);
        delay(100);
    }
    delay(1000);

    // Demo 3: Alternating pattern (checkerboard)
    Serial.println("Demo 3: Alternating patterns");
    for (int i = 0; i < 10; i++) {
        spi.writeWithClockStrobe(0b10101010);  // Alternate HIGH/LOW
        delay(100);
        spi.writeWithClockStrobe(0b01010101);  // Inverted
        delay(100);
    }
    delay(1000);

    // Demo 4: Manual clock control
    Serial.println("Demo 4: Manual clock control");
    for (int i = 0; i < 5; i++) {
        // Write data without clock strobe
        spi.writeData(0xFF);
        delay(50);

        // Manually strobe clock
        spi.clockHigh();
        delay(10);
        spi.clockLow();
        delay(50);

        // Write different data
        spi.writeData(0x00);
        delay(50);

        // Manually strobe clock again
        spi.clockHigh();
        delay(10);
        spi.clockLow();
        delay(50);
    }
    delay(1000);

    // Demo 5: Zero-delay mode (advanced - 13.2 MHz operation)
    Serial.println("Demo 5: Zero-delay mode (high-speed)");
    Serial.println("Writing 100 bytes at 13.2 MHz...");

    uint32_t start_us = micros();
    for (int i = 0; i < 100; i++) {
        uint8_t byte = i & 0xFF;

        // Zero-delay clock strobing (76ns per bit = 13.2 MHz)
        spi.writeDataAndClock(byte, 0);        // 30ns - data + clock LOW
        __asm__ __volatile__("nop; nop;");     // 8ns - GPIO settle
        spi.writeDataAndClock(byte, 1);        // 30ns - clock HIGH
        __asm__ __volatile__("nop; nop;");     // 8ns - before next cycle
    }
    uint32_t elapsed_us = micros() - start_us;

    Serial.print("Elapsed time: ");
    Serial.print(elapsed_us);
    Serial.println(" µs");
    Serial.print("Throughput: ");
    Serial.print((100.0 * 8) / elapsed_us);  // bits per µs = Mbps
    Serial.println(" Mbps");

    delay(2000);

    Serial.println();
    Serial.println("Loop complete - restarting demos...");
    Serial.println("----------------------------------");
    delay(1000);
}
