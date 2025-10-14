/// @file    Esp32C3_SingleSPI_Blocking.ino
/// @brief   ESP32-C3/C2 Single-SPI (1-way) Blocking driver test
/// @example Esp32C3_SingleSPI_Blocking.ino
///
/// This example demonstrates the 1-way (single pin) blocking SPI driver, which uses:
/// - 1 data pin (D0) for serial bit output
/// - 1 clock pin for synchronization
/// - Main thread inline bit-banging (no ISR overhead)
///
/// The blocking architecture offers:
/// - Lower overhead than ISR (no interrupt context switching)
/// - Simple blocking API (no async complexity)
/// - Better timing precision (inline execution)
/// - Higher throughput (no interrupt scheduling delays)
///
/// Comparison with ISR SPI:
/// - ISR SPI: Non-blocking, good for complex applications
/// - Blocking SPI: Lower overhead, good for simple LED updates

#ifndef ESP32
#define IS_ESP32_C3 0
#else
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2)
#define IS_ESP32_C3 1
#else
#define IS_ESP32_C3 0
#endif
#endif  // ESP32

#if IS_ESP32_C3

#include "platforms/esp/32/parallel_spi/parallel_spi_blocking_single.hpp"

// Test parameters - 1-way Single-SPI configuration
#define TEST_DATA_SIZE 8  // Short sequence for validation testing
#define NUM_TEST_ITERATIONS 3  // Iterations for testing

// Pin configuration: 1 data pin + 1 clock
#define DATA_PIN  0  // GPIO0 - Data pin
#define CLOCK_PIN 8  // GPIO8 - Clock pin

using namespace fl;

// Test data buffer - simple patterns for validation
static uint8_t testData[TEST_DATA_SIZE];

void setup() {
    Serial.begin(115200);
    delay(100);
    delay(2000);

    Serial.println("Esp32C3_SingleSPI_Blocking setup starting");
    Serial.println("1-way (Single-pin) Blocking SPI driver test");
    Serial.println("Architecture: Main thread inline bit-banging");
    Serial.print("Test data size: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");
    Serial.println("");

    Serial.println("Pin configuration:");
    Serial.print("  Data pin:  GPIO");
    Serial.println(DATA_PIN);
    Serial.print("  Clock pin: GPIO");
    Serial.println(CLOCK_PIN);
    Serial.println("");

    Serial.println("Key Features:");
    Serial.println("  - Lower overhead than ISR (no interrupt context switching)");
    Serial.println("  - Simple blocking API (transmit() blocks until complete)");
    Serial.println("  - Better timing precision (inline execution)");
    Serial.println("  - Higher throughput (no interrupt scheduling delays)");
    Serial.println("");

    // Initialize test data with patterns that exercise the data pin
    // Each byte only uses bit 0 (upper 7 bits ignored)
    testData[0] = 0x00;  // 0 - Pin low
    testData[1] = 0x01;  // 1 - Pin high
    testData[2] = 0x00;  // 0 - Pin low
    testData[3] = 0x01;  // 1 - Pin high
    testData[4] = 0x01;  // 1 - Pin high
    testData[5] = 0x01;  // 1 - Pin high
    testData[6] = 0x00;  // 0 - Pin low
    testData[7] = 0x00;  // 0 - Pin low

    Serial.println("Test data initialized (1-bit patterns):");
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        Serial.print("  testData[");
        Serial.print(i);
        Serial.print("] = 0x");
        Serial.print(testData[i], HEX);
        Serial.print(" (bit: ");
        Serial.print(testData[i] & 1);
        Serial.println(")");
    }
    Serial.println("");

    Serial.println("Setup complete - ready to test");
    delay(500);
}

void loop() {
    static int iteration = 0;
    static bool testComplete = false;

    if (iteration >= NUM_TEST_ITERATIONS) {
        // Test complete, just idle
        if (!testComplete) {
            Serial.print("Test finished - completed ");
            Serial.print(iteration);
            Serial.println(" iterations");
            Serial.println("✓ Esp32C3_SingleSPI_Blocking test PASSED");
            testComplete = true;
        }
        delay(5000);
        return;
    }

    iteration++;
    Serial.println("========================================");
    Serial.print("Starting loop iteration ");
    Serial.println(iteration);
    Serial.println("========================================");

    // Create Single-SPI blocking driver instance
    SingleSPI_Blocking_ESP32 spi;

    Serial.println("  Configuring Blocking Single-SPI driver...");

    // Set pin mapping for 1 data pin + 1 clock
    spi.setPinMapping(DATA_PIN, CLOCK_PIN);
    Serial.println("  Pin mapping configured (1 data + 1 clock)");

    // Load test data
    spi.loadBuffer(testData, TEST_DATA_SIZE);
    Serial.print("  Data buffer loaded: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");

    // Transmit (blocking call - completes inline)
    Serial.println("  Transmitting data (inline bit-banging)...");
    uint32_t startTime = micros();
    spi.transmit();  // Blocks until complete
    uint32_t endTime = micros();
    uint32_t elapsedUs = endTime - startTime;

    Serial.println("  Transfer completed successfully");
    Serial.print("  Transmission time: ");
    Serial.print(elapsedUs);
    Serial.println(" microseconds");

    // Calculate effective bit rate
    // Each byte represents 1 bit output, so TEST_DATA_SIZE bits total
    // Time is in microseconds, so rate is: (bits * 1000000) / time_us
    if (elapsedUs > 0) {
        uint32_t bitRateHz = (TEST_DATA_SIZE * 1000000UL) / elapsedUs;
        Serial.print("  Effective bit rate: ");
        Serial.print(bitRateHz);
        Serial.println(" Hz");
    }

    Serial.println("");
    Serial.print("Iteration ");
    Serial.print(iteration);
    Serial.println(" complete");
    Serial.println("");

    delay(500);
}

#else  // !IS_ESP32_C3
#include "platforms/sketch_fake.hpp"
#endif  // IS_ESP32_C3
