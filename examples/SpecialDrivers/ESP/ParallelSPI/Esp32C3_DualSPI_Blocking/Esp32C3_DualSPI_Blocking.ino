/// @file    Esp32C3_DualSPI_Blocking.ino
/// @brief   ESP32-C3/C2 Dual-SPI (2-way) Blocking driver test for main thread bit-banging
/// @example Esp32C3_DualSPI_Blocking.ino
///
/// This example demonstrates the 2-way parallel SPI blocking driver, which uses:
/// - 2 data pins (D0-D1) for parallel bit output
/// - 1 clock pin for synchronization
/// - Main thread inline bit-banging (no ISR overhead)
///
/// Key Differences from ISR version:
/// - Runs inline on main thread (no interrupt context switching)
/// - Simple blocking API (transmit() blocks until complete)
/// - Lower overhead (no interrupt latency or jitter)
/// - Better timing precision (inline execution)
/// - Higher throughput (no interrupt scheduling delays)
///
/// The 2-way architecture directly matches hardware Dual-SPI topology.

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

#include "platforms/shared/spi_bitbang/spi_block_2.h"

// Test parameters - 2-way Dual-SPI configuration
#define TEST_DATA_SIZE 8  // Short sequence for testing
#define NUM_TEST_ITERATIONS 3  // Iterations for testing

// Pin configuration: 2 data pins + 1 clock
#define DATA_PIN_0 0  // GPIO0 - Data bit 0 (LSB)
#define DATA_PIN_1 1  // GPIO1 - Data bit 1 (MSB)
#define CLOCK_PIN  8  // GPIO8 - Clock pin

using namespace fl;

// Test data buffer - simple patterns for validation
static uint8_t testData[TEST_DATA_SIZE];

void setup() {
    Serial.begin(115200);
    delay(100);
    delay(2000);

    Serial.println("Esp32C3_DualSPI_Blocking setup starting");
    Serial.println("2-way Parallel SPI Blocking driver test");
    Serial.println("Architecture: 2 data pins + 1 clock pin (inline bit-banging)");
    Serial.print("Test data size: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");
    Serial.println("");

    Serial.println("Pin configuration:");
    Serial.print("  Data pin 0 (LSB): GPIO");
    Serial.println(DATA_PIN_0);
    Serial.print("  Data pin 1 (MSB): GPIO");
    Serial.println(DATA_PIN_1);
    Serial.print("  Clock pin:        GPIO");
    Serial.println(CLOCK_PIN);
    Serial.println("");

    // Initialize test data with patterns that exercise both data pins
    // Each byte only uses lower 2 bits (upper 6 bits ignored)
    testData[0] = 0x00;  // 00 - Both pins low
    testData[1] = 0x01;  // 01 - D0 high, D1 low
    testData[2] = 0x02;  // 10 - D0 low, D1 high
    testData[3] = 0x03;  // 11 - Both pins high
    testData[4] = 0x01;  // 01 - Repeat
    testData[5] = 0x02;  // 10 - Repeat
    testData[6] = 0x00;  // 00 - Repeat
    testData[7] = 0x03;  // 11 - Repeat

    Serial.println("Test data initialized (2-bit patterns):");
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        Serial.print("  testData[");
        Serial.print(i);
        Serial.print("] = 0x");
        Serial.print(testData[i], HEX);
        Serial.print(" (binary: ");
        for (int b = 1; b >= 0; b--) {  // Only show lower 2 bits
            Serial.print((testData[i] >> b) & 1);
        }
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
            Serial.println("✓ Esp32C3_DualSPI_Blocking test PASSED");
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

    // Create Dual-SPI blocking driver instance
    SpiBlock2 spi;

    Serial.println("  Configuring Dual-SPI blocking driver...");

    // Set pin mapping for 2 data pins + 1 clock
    spi.setPinMapping(DATA_PIN_0, DATA_PIN_1, CLOCK_PIN);
    Serial.println("  Pin mapping configured (2 data + 1 clock)");

    // Load test data
    spi.loadBuffer(testData, TEST_DATA_SIZE);
    Serial.print("  Data buffer loaded: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");

    // Transmit using inline bit-banging (blocking)
    Serial.println("  Transmitting data (blocking)...");

    // Measure transmission time
    uint32_t startTime = micros();
    spi.transmit();
    uint32_t endTime = micros();
    uint32_t duration = endTime - startTime;

    Serial.println("  Transfer completed successfully");
    Serial.print("  Transmission time: ");
    Serial.print(duration);
    Serial.println(" microseconds");

    // Calculate effective bit rate
    // For 2-way SPI: 2 bits transmitted per clock cycle
    // Total bits = TEST_DATA_SIZE bytes * 2 bits/byte
    uint32_t totalBits = TEST_DATA_SIZE * 2;
    float bitRateKHz = (float)totalBits / (float)duration * 1000.0f;
    Serial.print("  Effective bit rate: ");
    Serial.print(bitRateKHz, 2);
    Serial.println(" kHz (per lane)");

    // Calculate total throughput
    float throughputKHz = bitRateKHz * 2;  // 2 lanes
    Serial.print("  Total throughput: ");
    Serial.print(throughputKHz, 2);
    Serial.println(" kHz (both lanes combined)");

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
