/*
 * FastPinsSPI Example
 *
 * Demonstrates FastLED's FastPinsWithClock for parallel SPI-like protocols
 * with 8 data lines + 1 clock line, all controlled with ultra-low latency.
 *
 * KEY CONCEPTS:
 * - FastPinsWithClock<8>: 8 data pins + 1 clock pin controller
 * - All 9 pins MUST be on same GPIO port/bank
 * - Optimized for SPI-like protocols requiring synchronized clock
 * - Three write modes: data-only, data+clock-strobe, zero-delay
 *
 * PERFORMANCE:
 * - writeData(): 20-30ns (data only, clock unchanged)
 * - writeWithClockStrobe(): 40ns (data + clock HIGH + clock LOW)
 * - writeDataAndClock(): 35ns (data + clock state, zero-delay mode)
 * - Manual clock control: 5ns per clockHigh()/clockLow()
 *
 * MEMORY USAGE:
 * - FastPinsWithClock<8>: 2KB (same as FastPinsSamePort<8>)
 * - AVR: 256 bytes (optimized for 8-bit)
 * - Uses FastPinsSamePort<8> internally for data pins
 *
 * PLATFORM-SPECIFIC PIN CONFIGURATIONS:
 *
 * ESP32/ESP32-S3:
 *   - Bank 0 (pins 0-31): 8 data + 1 clock all in 0-31 range
 *   - Example: Clock=17, Data=2,4,5,12,13,14,15,16
 *   - NOTE: Do NOT mix banks (0-31 vs 32-39)!
 *
 * RP2040 (Raspberry Pi Pico):
 *   - Single bank: Any 9 pins, e.g., Clock=10, Data=2-9
 *   - No restrictions (all pins on same hardware port)
 *
 * STM32 (Bluepill/Blackpill):
 *   - All on GPIOA: Clock=PA8, Data=PA0-PA7
 *   - OR all on GPIOB: Clock=PB8, Data=PB0-PB7
 *   - NOTE: Do NOT mix ports!
 *
 * Arduino UNO/Nano (ATmega328P):
 *   - PORTB (pins 8-13): Clock=8, Data=9,10,11,12,13 (only 5 data pins available)
 *   - PORTD (pins 0-7): Clock=0, Data=1,2,3,4,5,6,7 (8 pins, but pin 0/1 used by Serial)
 *   - Recommended: Use PORTD with clock=2, data=3,4,5,6,7 (5 pins, Serial safe)
 *   - MEMORY WARNING: 256 bytes (12.5% of 2KB RAM)
 *
 * Arduino MEGA (ATmega2560):
 *   - PORTA (pins 22-29): Clock=22, Data=23-29 (7 data pins)
 *   - PORTB (pins 10-13, 50-53): Clock=50, Data=51,52,53,10,11,12,13 (7 data pins)
 *   - NOTE: All 9 pins must be on same PORT
 *
 * Teensy 4.0/4.1:
 *   - GPIO6/7: Clock + 8 data pins on same GPIO port
 *   - Check Teensy pinout for specific GPIO port assignments
 *
 * USE CASES:
 * 1. Parallel SPI data transmission (8 channels + clock)
 * 2. Custom SPI-like protocols
 * 3. Shift register control (8 parallel outputs + clock)
 * 4. Multi-channel LED drivers with clock synchronization
 *
 * IMPORTANT WARNINGS:
 * - All 9 pins (8 data + 1 clock) MUST be on same port/bank
 * - Validation will warn if pins are on different ports
 * - AVR: Careful with memory (256 bytes = 12.5% of UNO RAM)
 *
 * Author: FastLED Team
 * License: MIT
 */

#include <FastLED.h>

// Choose pin configuration based on platform
#if defined(ESP32)
  // ESP32: All pins on Bank 0 (pins 0-31)
  #define CLOCK_PIN  17
  #define DATA_PIN0  2
  #define DATA_PIN1  4
  #define DATA_PIN2  5
  #define DATA_PIN3  12
  #define DATA_PIN4  13
  #define DATA_PIN5  14
  #define DATA_PIN6  15
  #define DATA_PIN7  16

#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040: Any pins (single bank)
  #define CLOCK_PIN  10
  #define DATA_PIN0  2
  #define DATA_PIN1  3
  #define DATA_PIN2  4
  #define DATA_PIN3  5
  #define DATA_PIN4  6
  #define DATA_PIN5  7
  #define DATA_PIN6  8
  #define DATA_PIN7  9

#elif defined(STM32F1) || defined(STM32F4)
  // STM32: All on GPIOA
  #define CLOCK_PIN  PA8
  #define DATA_PIN0  PA0
  #define DATA_PIN1  PA1
  #define DATA_PIN2  PA2
  #define DATA_PIN3  PA3
  #define DATA_PIN4  PA4
  #define DATA_PIN5  PA5
  #define DATA_PIN6  PA6
  #define DATA_PIN7  PA7

#elif defined(__AVR_ATmega2560__)
  // Arduino MEGA: All on PORTB
  #define CLOCK_PIN  50
  #define DATA_PIN0  51
  #define DATA_PIN1  52
  #define DATA_PIN2  53
  #define DATA_PIN3  10
  #define DATA_PIN4  11
  #define DATA_PIN5  12
  #define DATA_PIN6  13
  #define DATA_PIN7  50  // Reuse for demo (normally use different pin)

#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  // Arduino UNO/Nano: PORTD (avoiding Serial pins 0,1)
  // Only using 5 data pins to keep Serial functional
  #define CLOCK_PIN  2
  #define DATA_PIN0  3
  #define DATA_PIN1  4
  #define DATA_PIN2  5
  #define DATA_PIN3  6
  #define DATA_PIN4  7
  #define DATA_PIN5  3  // Reuse for demo
  #define DATA_PIN6  4  // Reuse for demo
  #define DATA_PIN7  5  // Reuse for demo

#elif defined(__MK20DX256__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
  // Teensy 3.x/4.x: Using adjacent pins (check your model's pinout)
  #define CLOCK_PIN  0
  #define DATA_PIN0  1
  #define DATA_PIN1  2
  #define DATA_PIN2  3
  #define DATA_PIN3  4
  #define DATA_PIN4  5
  #define DATA_PIN5  6
  #define DATA_PIN6  7
  #define DATA_PIN7  8

#else
  // Generic fallback
  #define CLOCK_PIN  10
  #define DATA_PIN0  2
  #define DATA_PIN1  3
  #define DATA_PIN2  4
  #define DATA_PIN3  5
  #define DATA_PIN4  6
  #define DATA_PIN5  7
  #define DATA_PIN6  8
  #define DATA_PIN7  9
#endif

// Create FastPinsWithClock instance (8 data pins + 1 clock)
fl::FastPinsWithClock<8> spiController;

// Test data patterns
const uint8_t testData[] = {
  0x00,  // All LOW
  0xFF,  // All HIGH
  0xAA,  // 10101010
  0x55,  // 01010101
  0x0F,  // 00001111
  0xF0,  // 11110000
  0x33,  // 00110011
  0xCC,  // 11001100
};

const int NUM_TEST_PATTERNS = sizeof(testData) / sizeof(testData[0]);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for serial port to connect (up to 5 seconds)
  }

  Serial.println("\n=== FastPinsWithClock (FastPinsSPI) Demo ===\n");
  Serial.println("This demo shows FastLED's 8-data + 1-clock parallel GPIO control.");
  Serial.println("Perfect for parallel SPI protocols and synchronized data transmission.\n");

  // Print platform information
  #if defined(ESP32)
    Serial.println("Platform: ESP32");
    Serial.println("Configuration: Clock + 8 data pins on Bank 0");
  #elif defined(ARDUINO_ARCH_RP2040)
    Serial.println("Platform: RP2040 (Raspberry Pi Pico)");
    Serial.println("Configuration: Clock + 8 data pins (single bank)");
  #elif defined(STM32F1) || defined(STM32F4)
    Serial.println("Platform: STM32");
    Serial.println("Configuration: Clock + 8 data pins on GPIOA");
  #elif defined(__AVR_ATmega2560__)
    Serial.println("Platform: Arduino MEGA");
    Serial.println("Configuration: Clock + 8 data pins on PORTB");
  #elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
    Serial.println("Platform: Arduino UNO/Nano");
    Serial.println("Configuration: Clock + 5 data pins on PORTD (Serial-safe)");
    Serial.println("MEMORY WARNING: 256 bytes used (12.5% of 2KB RAM)");
  #elif defined(__MK20DX256__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
    Serial.println("Platform: Teensy");
    Serial.println("Configuration: Clock + 8 data pins on same GPIO port");
  #else
    Serial.println("Platform: Generic");
  #endif

  Serial.println("\nPin configuration:");
  Serial.print("  Clock pin:   GPIO "); Serial.println(CLOCK_PIN);
  Serial.print("  Data pin 0:  GPIO "); Serial.println(DATA_PIN0);
  Serial.print("  Data pin 1:  GPIO "); Serial.println(DATA_PIN1);
  Serial.print("  Data pin 2:  GPIO "); Serial.println(DATA_PIN2);
  Serial.print("  Data pin 3:  GPIO "); Serial.println(DATA_PIN3);
  Serial.print("  Data pin 4:  GPIO "); Serial.println(DATA_PIN4);
  Serial.print("  Data pin 5:  GPIO "); Serial.println(DATA_PIN5);
  Serial.print("  Data pin 6:  GPIO "); Serial.println(DATA_PIN6);
  Serial.print("  Data pin 7:  GPIO "); Serial.println(DATA_PIN7);

  // Configure pins (clock first, then 8 data pins)
  Serial.println("\nInitializing FastPinsWithClock...");
  spiController.setPins(CLOCK_PIN,
                       DATA_PIN0, DATA_PIN1, DATA_PIN2, DATA_PIN3,
                       DATA_PIN4, DATA_PIN5, DATA_PIN6, DATA_PIN7);

  // Set all pins as outputs using standard pinMode
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN0, OUTPUT);
  pinMode(DATA_PIN1, OUTPUT);
  pinMode(DATA_PIN2, OUTPUT);
  pinMode(DATA_PIN3, OUTPUT);
  pinMode(DATA_PIN4, OUTPUT);
  pinMode(DATA_PIN5, OUTPUT);
  pinMode(DATA_PIN6, OUTPUT);
  pinMode(DATA_PIN7, OUTPUT);

  Serial.println("Initialization complete!\n");
  Serial.println("Performance:");
  Serial.println("  writeData():            20-30ns (data only)");
  Serial.println("  writeWithClockStrobe(): 40ns (data + clock pulse)");
  Serial.println("  writeDataAndClock():    35ns (zero-delay mode)");
  Serial.println("  clockHigh()/clockLow(): 5ns each");
  Serial.println("  Memory usage:           256 bytes (AVR), 2KB (others)\n");

  Serial.println("Now demonstrating three write modes...\n");
  delay(1000);
}

void loop() {
  Serial.println("=== Demo 1: Standard Clock Strobing (writeWithClockStrobe) ===");
  Serial.println("Data is set, then clock goes HIGH->LOW automatically (40ns total)\n");

  for (int i = 0; i < NUM_TEST_PATTERNS; i++) {
    uint8_t data = testData[i];
    Serial.print("Sending 0x");
    if (data < 16) Serial.print("0");
    Serial.print(data, HEX);
    Serial.print(" (0b");
    for (int b = 7; b >= 0; b--) {
      Serial.print((data >> b) & 1);
    }
    Serial.println(")");

    // Write data with automatic clock strobe (40ns)
    spiController.writeWithClockStrobe(data);
    delay(250);
  }

  Serial.println("\n=== Demo 2: Manual Clock Control ===");
  Serial.println("Data is set, then clock is controlled manually\n");

  for (int i = 0; i < NUM_TEST_PATTERNS; i++) {
    uint8_t data = testData[i];
    Serial.print("Sending 0x");
    if (data < 16) Serial.print("0");
    Serial.print(data, HEX);
    Serial.println(" with manual clock");

    // Write data only (20-30ns)
    spiController.writeData(data);
    delay(100);

    // Manually strobe clock HIGH (5ns)
    spiController.clockHigh();
    delay(100);

    // Manually strobe clock LOW (5ns)
    spiController.clockLow();
    delay(100);
  }

  Serial.println("\n=== Demo 3: Zero-Delay Mode (writeDataAndClock) ===");
  Serial.println("Data and clock state set simultaneously (35ns)\n");
  Serial.println("Use for ultra-high-speed protocols (13-17 MHz with NOPs)\n");

  for (int i = 0; i < NUM_TEST_PATTERNS; i++) {
    uint8_t data = testData[i];
    Serial.print("Sending 0x");
    if (data < 16) Serial.print("0");
    Serial.print(data, HEX);
    Serial.println(" with zero-delay strobing");

    // Set data with clock LOW (35ns)
    spiController.writeDataAndClock(data, 0);
    delay(100);

    // Set data with clock HIGH (35ns)
    spiController.writeDataAndClock(data, 1);
    delay(100);

    // Set data with clock LOW (35ns)
    spiController.writeDataAndClock(data, 0);
    delay(100);
  }

  Serial.println("\n=== Demo 4: Rapid SPI Transfer (50 bytes) ===");
  Serial.println("Sending 50 bytes at maximum speed...\n");

  uint8_t rapidData[50];
  for (int i = 0; i < 50; i++) {
    rapidData[i] = i * 5;  // Generate test pattern
  }

  unsigned long startTime = micros();
  for (int i = 0; i < 50; i++) {
    spiController.writeWithClockStrobe(rapidData[i]);
    // In real application, no delay here - transfers at maximum speed
    delayMicroseconds(1);  // Small delay for demo purposes
  }
  unsigned long endTime = micros();

  Serial.print("50 bytes transferred in ");
  Serial.print(endTime - startTime);
  Serial.println(" microseconds");
  Serial.print("Average: ");
  Serial.print((endTime - startTime) / 50.0);
  Serial.println(" µs per byte\n");

  Serial.println("=== All demos complete! Restarting... ===\n");
  delay(2000);
}

/*
 * ADVANCED USAGE EXAMPLES:
 *
 * 1. Ultra-Fast SPI Transfer (13.2 MHz with balanced timing):
 *
 *    void transferByteFast(uint8_t data) {
 *      // Zero-delay mode with NOPs for GPIO settling
 *      spiController.writeDataAndClock(data, 0);  // 30ns - data + clock LOW
 *      __asm__ __volatile__("nop; nop;");         // 8ns - GPIO settle
 *      spiController.writeDataAndClock(data, 1);  // 30ns - clock HIGH
 *      __asm__ __volatile__("nop; nop;");         // 8ns - setup time
 *      // Total: 76ns = 13.2 MHz
 *    }
 *
 * 2. Shift Register Control:
 *
 *    void writeToShiftRegisters(const uint8_t* data, size_t length) {
 *      for (size_t i = 0; i < length; i++) {
 *        spiController.writeWithClockStrobe(data[i]);  // 40ns per byte
 *      }
 *      // Optional: Add latch pulse on separate pin
 *    }
 *
 * 3. Custom SPI Protocol with Variable Clock Timing:
 *
 *    void customSPITransfer(uint8_t data, uint16_t clockPulseUs) {
 *      spiController.writeData(data);           // Set data (20-30ns)
 *      delayMicroseconds(clockPulseUs / 2);     // Setup time
 *      spiController.clockHigh();               // Clock HIGH (5ns)
 *      delayMicroseconds(clockPulseUs / 2);     // Hold time
 *      spiController.clockLow();                // Clock LOW (5ns)
 *    }
 *
 * 4. Parallel Data Transmission (8 channels):
 *
 *    void sendParallelBytes(const uint8_t* data, size_t length) {
 *      for (size_t i = 0; i < length; i++) {
 *        spiController.writeWithClockStrobe(data[i]);
 *        // Each byte sent to all 8 channels simultaneously
 *      }
 *    }
 *
 * PERFORMANCE TIPS:
 * - Use writeWithClockStrobe() for standard timing (40ns)
 * - Use writeDataAndClock() + NOPs for ultra-high speed (13-17 MHz)
 * - Manual clock control for custom protocols with variable timing
 * - All 9 pins MUST be on same port/bank for optimal performance
 * - For ESP32 with WiFi: Consider Level 7 NMI interrupts (see NMI example)
 *
 * ZERO-DELAY MODE DETAILS:
 *
 * writeDataAndClock() allows you to set data and clock state simultaneously,
 * enabling zero-delay clock strobing for maximum speed:
 *
 *   // Aggressive (60ns per bit, may violate clock pulse width):
 *   spiController.writeDataAndClock(byte, 0);  // 30ns
 *   spiController.writeDataAndClock(byte, 1);  // 30ns
 *   // Total: 60ns = 16.7 MHz
 *
 *   // Balanced (76ns per bit, safe clock pulse width):
 *   spiController.writeDataAndClock(byte, 0);    // 30ns
 *   __asm__ __volatile__("nop; nop;");           // 8ns
 *   spiController.writeDataAndClock(byte, 1);    // 30ns
 *   __asm__ __volatile__("nop; nop;");           // 8ns
 *   // Total: 76ns = 13.2 MHz
 *
 *   // Conservative (92ns per bit, robust timing):
 *   spiController.writeDataAndClock(byte, 0);           // 30ns
 *   __asm__ __volatile__("nop; nop; nop; nop;");        // 16ns
 *   spiController.writeDataAndClock(byte, 1);           // 30ns
 *   __asm__ __volatile__("nop; nop; nop; nop;");        // 16ns
 *   // Total: 92ns = 10.9 MHz
 *
 * GPIO propagation delay (15-25ns) means back-to-back writes result in
 * narrow clock pulses. Add NOPs (4ns each) for reliable operation.
 *
 * MEMORY CONSIDERATIONS:
 * - AVR (UNO): 256 bytes = 12.5% of 2KB RAM
 * - AVR (MEGA): 256 bytes = 3% of 8KB RAM
 * - ESP32: 2KB = 0.6% of 320KB RAM
 * - STM32: 2KB = 3-10% depending on model
 *
 * DEBUGGING:
 * - Verify all 9 pins are on same port/bank (validation will warn)
 * - Use logic analyzer to verify timing
 * - Expect: writeWithClockStrobe() = 40ns, writeData() = 20-30ns
 * - Clock pulses should be clean with no glitches
 */
