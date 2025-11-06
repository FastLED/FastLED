/*
 * FastPinsMultiSPI Example
 *
 * Demonstrates FastLED's multi-SPI parallel GPIO control for driving
 * multiple LED strips or SPI data lines simultaneously.
 *
 * KEY CONCEPTS:
 * - FastPinsSamePort<8>: Ultra-fast same-port/bank GPIO control
 * - 8 parallel data lines written simultaneously (20-30ns per write)
 * - Perfect for bit-banging WS2812 LEDs or multi-SPI protocols
 * - All pins MUST be on the same GPIO port/bank for optimal performance
 *
 * PERFORMANCE:
 * - Same-port write: 20-30ns (ARM/Xtensa), 15-20ns (AVR)
 * - 50-100× faster than digitalWrite loops
 * - Atomic writes (no race conditions)
 * - Zero CPU overhead during write
 *
 * MEMORY USAGE:
 * - FastPinsSamePort<8>: 2KB LUT (256 entries × 8 bytes)
 * - FastPinsSamePort<8>: 256 bytes on AVR (optimized for 8-bit)
 *
 * PLATFORM-SPECIFIC PIN CONFIGURATIONS:
 *
 * ESP32/ESP32-S3:
 *   - Bank 0 (pins 0-31): GPIO 2, 4, 5, 12, 13, 14, 15, 16
 *   - Bank 1 (pins 32-39): GPIO 32, 33, 34, 35, 36, 37, 38, 39
 *   - NOTE: Do NOT mix banks! All pins must be in same bank.
 *
 * RP2040 (Raspberry Pi Pico):
 *   - Single bank (pins 0-29): Any 8 pins, e.g., GPIO 2-9
 *   - No bank restrictions (all pins on same hardware port)
 *
 * STM32 (Bluepill/Blackpill):
 *   - GPIOA: PA0-PA15
 *   - GPIOB: PB0-PB15
 *   - GPIOC: PC0-PC15
 *   - NOTE: Do NOT mix ports! All pins must be on same port (PA, PB, or PC).
 *
 * Arduino UNO/Nano (ATmega328P):
 *   - PORTB (pins 8-13): Arduino pins 8, 9, 10, 11, 12, 13
 *   - PORTC (pins A0-A5): Arduino pins A0, A1, A2, A3, A4, A5
 *   - PORTD (pins 0-7): Arduino pins 0, 1, 2, 3, 4, 5, 6, 7
 *   - NOTE: Do NOT mix ports! All pins must be on same PORT.
 *   - MEMORY WARNING: FastPinsSamePort<8> uses 256 bytes (12.5% of 2KB RAM)
 *
 * Arduino MEGA (ATmega2560):
 *   - PORTA (pins 22-29): Arduino pins 22, 23, 24, 25, 26, 27, 28, 29
 *   - PORTB (pins 10-13, 50-53): Arduino pins 10, 11, 12, 13, 50, 51, 52, 53
 *   - PORTC (pins 30-37): Arduino pins 30, 31, 32, 33, 34, 35, 36, 37
 *   - NOTE: Do NOT mix ports! All pins must be on same PORT.
 *
 * Teensy 4.0/4.1:
 *   - GPIO6: Many pins (varies by Teensy model)
 *   - GPIO7: Many pins (varies by Teensy model)
 *   - NOTE: Do NOT mix GPIO ports! All pins must be on same GPIO port.
 *
 * USE CASES:
 * 1. WS2812 LED strips (8 parallel strips at 800 kHz)
 * 2. Multi-SPI data transmission (8 channels simultaneously)
 * 3. Parallel communication protocols
 * 4. High-speed GPIO bit-banging
 *
 * IMPORTANT WARNINGS:
 * - All pins MUST be on same port/bank (validation in code will warn)
 * - Cross-port/bank pins will fall back to slower multi-port mode
 * - AVR: Be careful with memory usage (256 bytes on UNO = 12.5% of RAM)
 *
 * Author: FastLED Team
 * License: MIT
 */

#include <FastLED.h>

// Choose pin configuration based on platform
#if defined(ESP32)
  // ESP32: All pins on Bank 0 (pins 0-31)
  // NOTE: Do NOT use pins 32-39 here (different bank)
  #define PIN0  2
  #define PIN1  4
  #define PIN2  5
  #define PIN3  12
  #define PIN4  13
  #define PIN5  14
  #define PIN6  15
  #define PIN7  16

#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040: Any pins (single bank)
  #define PIN0  2
  #define PIN1  3
  #define PIN2  4
  #define PIN3  5
  #define PIN4  6
  #define PIN5  7
  #define PIN6  8
  #define PIN7  9

#elif defined(STM32F1) || defined(STM32F4)
  // STM32: All on GPIOA
  #define PIN0  PA0
  #define PIN1  PA1
  #define PIN2  PA2
  #define PIN3  PA3
  #define PIN4  PA4
  #define PIN5  PA5
  #define PIN6  PA6
  #define PIN7  PA7

#elif defined(__AVR_ATmega2560__)
  // Arduino MEGA: All on PORTB (pins 10-13, 50-53)
  #define PIN0  50
  #define PIN1  51
  #define PIN2  52
  #define PIN3  53
  #define PIN4  10
  #define PIN5  11
  #define PIN6  12
  #define PIN7  13

#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  // Arduino UNO/Nano: All on PORTB (pins 8-13)
  // Only 6 pins available on PORTB, so using 6 pins instead of 8
  #define PIN0  8
  #define PIN1  9
  #define PIN2  10
  #define PIN3  11
  #define PIN4  12
  #define PIN5  13
  #define PIN6  8   // Reuse pin 8 for demo purposes
  #define PIN7  9   // Reuse pin 9 for demo purposes

#elif defined(__MK20DX256__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
  // Teensy 3.x/4.x: Using pins that are likely on same GPIO port
  // NOTE: Check your specific Teensy model's pinout
  #define PIN0  0
  #define PIN1  1
  #define PIN2  2
  #define PIN3  3
  #define PIN4  4
  #define PIN5  5
  #define PIN6  6
  #define PIN7  7

#else
  // Generic fallback
  #define PIN0  2
  #define PIN1  3
  #define PIN2  4
  #define PIN3  5
  #define PIN4  6
  #define PIN5  7
  #define PIN6  8
  #define PIN7  9
#endif

// Create FastPinsSamePort instance with 8 pins
fl::FastPinsSamePort<8> multiSPI;

// Demo patterns
const uint8_t patterns[] = {
  0b00000000,  // All LOW
  0b11111111,  // All HIGH
  0b10101010,  // Alternating (even HIGH, odd LOW)
  0b01010101,  // Alternating (even LOW, odd HIGH)
  0b00001111,  // Lower 4 HIGH, upper 4 LOW
  0b11110000,  // Lower 4 LOW, upper 4 HIGH
  0b11000011,  // Outer HIGH, inner LOW
  0b00111100,  // Outer LOW, inner HIGH
};

const int NUM_PATTERNS = sizeof(patterns) / sizeof(patterns[0]);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    ; // Wait for serial port to connect (up to 5 seconds)
  }

  Serial.println("\n=== FastPins Multi-SPI Demo ===\n");
  Serial.println("This demo shows FastLED's ultra-fast parallel GPIO control.");
  Serial.println("Perfect for driving multiple LED strips or SPI data lines.\n");

  // Print platform information
  #if defined(ESP32)
    Serial.println("Platform: ESP32");
    Serial.println("Configuration: 8 pins on Bank 0 (pins 0-31)");
  #elif defined(ARDUINO_ARCH_RP2040)
    Serial.println("Platform: RP2040 (Raspberry Pi Pico)");
    Serial.println("Configuration: 8 pins (single bank)");
  #elif defined(STM32F1) || defined(STM32F4)
    Serial.println("Platform: STM32");
    Serial.println("Configuration: 8 pins on GPIOA");
  #elif defined(__AVR_ATmega2560__)
    Serial.println("Platform: Arduino MEGA");
    Serial.println("Configuration: 8 pins on PORTB");
  #elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
    Serial.println("Platform: Arduino UNO/Nano");
    Serial.println("Configuration: 6 pins on PORTB (demonstrating with 8-entry LUT)");
    Serial.println("MEMORY WARNING: 256 bytes used (12.5% of 2KB RAM)");
  #elif defined(__MK20DX256__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
    Serial.println("Platform: Teensy");
    Serial.println("Configuration: 8 pins on same GPIO port");
  #else
    Serial.println("Platform: Generic");
  #endif

  Serial.println("\nPins configured:");
  Serial.print("  Pin 0 (LSB): GPIO "); Serial.println(PIN0);
  Serial.print("  Pin 1:       GPIO "); Serial.println(PIN1);
  Serial.print("  Pin 2:       GPIO "); Serial.println(PIN2);
  Serial.print("  Pin 3:       GPIO "); Serial.println(PIN3);
  Serial.print("  Pin 4:       GPIO "); Serial.println(PIN4);
  Serial.print("  Pin 5:       GPIO "); Serial.println(PIN5);
  Serial.print("  Pin 6:       GPIO "); Serial.println(PIN6);
  Serial.print("  Pin 7 (MSB): GPIO "); Serial.println(PIN7);

  // Configure pins (all must be on same port/bank)
  Serial.println("\nInitializing FastPinsSamePort...");
  multiSPI.setPins(PIN0, PIN1, PIN2, PIN3, PIN4, PIN5, PIN6, PIN7);

  // Set all pins as outputs using standard pinMode
  // (FastPins only controls OUTPUT state, not pin direction)
  pinMode(PIN0, OUTPUT);
  pinMode(PIN1, OUTPUT);
  pinMode(PIN2, OUTPUT);
  pinMode(PIN3, OUTPUT);
  pinMode(PIN4, OUTPUT);
  pinMode(PIN5, OUTPUT);
  pinMode(PIN6, OUTPUT);
  pinMode(PIN7, OUTPUT);

  Serial.println("Initialization complete!\n");
  Serial.println("Performance:");
  Serial.println("  Write time: 20-30ns (ARM/Xtensa), 15-20ns (AVR)");
  Serial.println("  Speed vs digitalWrite: 50-100x faster");
  Serial.println("  Memory usage: 256 bytes (AVR), 2KB (others)\n");

  Serial.println("Now cycling through test patterns...");
  Serial.println("Observe GPIO outputs with logic analyzer or oscilloscope.\n");

  delay(1000);
}

void loop() {
  // Cycle through test patterns
  for (int i = 0; i < NUM_PATTERNS; i++) {
    uint8_t pattern = patterns[i];

    // Print pattern information
    Serial.print("Pattern ");
    Serial.print(i);
    Serial.print(": 0b");
    for (int b = 7; b >= 0; b--) {
      Serial.print((pattern >> b) & 1);
    }
    Serial.print(" (0x");
    if (pattern < 16) Serial.print("0");
    Serial.print(pattern, HEX);
    Serial.print(") - ");

    // Describe pattern
    switch (pattern) {
      case 0b00000000: Serial.println("All pins LOW"); break;
      case 0b11111111: Serial.println("All pins HIGH"); break;
      case 0b10101010: Serial.println("Alternating: even HIGH, odd LOW"); break;
      case 0b01010101: Serial.println("Alternating: even LOW, odd HIGH"); break;
      case 0b00001111: Serial.println("Lower 4 HIGH, upper 4 LOW"); break;
      case 0b11110000: Serial.println("Lower 4 LOW, upper 4 HIGH"); break;
      case 0b11000011: Serial.println("Outer pins HIGH, inner LOW"); break;
      case 0b00111100: Serial.println("Outer pins LOW, inner HIGH"); break;
      default: Serial.println("");
    }

    // Write pattern to all 8 pins simultaneously (20-30ns!)
    multiSPI.write(pattern);

    // Hold pattern for observation
    delay(500);
  }

  Serial.println("\n--- Rapid Pattern Sequence (50µs per pattern) ---");
  Serial.println("Cycling through all 256 patterns at high speed...");

  // Rapid sequence: All 256 patterns in 12.8ms
  for (uint16_t pattern = 0; pattern < 256; pattern++) {
    multiSPI.write(pattern);
    delayMicroseconds(50);  // 50µs per pattern = 20kHz pattern rate
  }

  Serial.println("Sequence complete! Restarting...\n");
  delay(1000);
}

/*
 * ADVANCED USAGE EXAMPLES:
 *
 * 1. WS2812 LED Bit-Banging (8 parallel strips):
 *
 *    void sendWS2812Byte(uint8_t data) {
 *      for (int bit = 7; bit >= 0; bit--) {
 *        uint8_t pattern = (data >> bit) & 0x01;
 *
 *        // WS2812 timing: T0H=400ns, T0L=850ns, T1H=800ns, T1L=450ns
 *        multiSPI.write(pattern ? 0xFF : 0x00);  // 30ns - set data
 *        delayNanoseconds(pattern ? 800 : 400);  // High time
 *        multiSPI.write(0x00);                    // 30ns - clear all
 *        delayNanoseconds(pattern ? 450 : 850);  // Low time
 *      }
 *    }
 *
 * 2. Multi-SPI Data Transmission:
 *
 *    void sendSPIByte(uint8_t data) {
 *      multiSPI.write(data);  // Write to all 8 SPI data lines
 *      // Clock pulse would be handled separately
 *      // See FastPinsWithClock example for clock integration
 *    }
 *
 * 3. Parallel Communication Protocol:
 *
 *    void sendParallelData(const uint8_t* data, size_t length) {
 *      for (size_t i = 0; i < length; i++) {
 *        multiSPI.write(data[i]);  // 30ns per byte
 *        // Add protocol-specific timing or handshaking
 *        delayMicroseconds(1);
 *      }
 *    }
 *
 * PERFORMANCE TIPS:
 * - Keep all pins on same port/bank for 20-30ns writes
 * - Mixed ports degrade to 60-120ns (still fast, but not optimal)
 * - AVR has FASTEST same-port mode: 15-20ns!
 * - Use FastPinsWithClock<8> if you need clock synchronization
 * - For ESP32 with WiFi: Consider Level 7 NMI interrupts (see NMI example)
 *
 * MEMORY CONSIDERATIONS:
 * - AVR (UNO): 256 bytes = 12.5% of 2KB RAM (acceptable)
 * - AVR (MEGA): 256 bytes = 3% of 8KB RAM (excellent)
 * - ESP32: 2KB = 0.6% of 320KB RAM (negligible)
 * - STM32: 2KB = 3-10% depending on model (acceptable)
 *
 * DEBUGGING:
 * - If performance is slower than expected, check pin port/bank assignment
 * - FastPinsSamePort will warn if pins are on different ports
 * - Use logic analyzer to verify timing (expect 20-30ns write pulses)
 * - For AVR, monitor RAM usage (should be 256 bytes for FastPinsSamePort<8>)
 */
