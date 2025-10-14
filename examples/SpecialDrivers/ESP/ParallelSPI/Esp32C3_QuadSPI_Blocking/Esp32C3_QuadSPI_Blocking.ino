/// @file    Esp32C3_QuadSPI_Blocking.ino
/// @brief   ESP32-C3/C2 Quad-SPI (4-way) Blocking driver test for main thread bit-banging
/// @example Esp32C3_QuadSPI_Blocking.ino
///
/// This example demonstrates the 4-way parallel SPI blocking driver, which uses:
/// - 4 data pins (D0-D3) for parallel bit output
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
/// The 4-way architecture directly matches hardware Quad-SPI topology.

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

#include "platforms/esp/32/parallel_spi/parallel_spi_blocking_quad.hpp"

// Test parameters - 4-way Quad-SPI configuration
#define TEST_DATA_SIZE 16  // 16 bytes to exercise all 16 possible 4-bit patterns
#define NUM_TEST_ITERATIONS 3  // Iterations for testing

// Pin configuration: 4 data pins + 1 clock
#define DATA_PIN_0 0  // GPIO0 - Data bit 0 (LSB)
#define DATA_PIN_1 1  // GPIO1 - Data bit 1
#define DATA_PIN_2 2  // GPIO2 - Data bit 2
#define DATA_PIN_3 3  // GPIO3 - Data bit 3 (MSB)
#define CLOCK_PIN  8  // GPIO8 - Clock pin

using namespace fl;

// Test data buffer - all 16 possible 4-bit patterns for validation
static uint8_t testData[TEST_DATA_SIZE];

void setup() {
    Serial.begin(115200);
    delay(100);
    delay(2000);

    Serial.println("Esp32C3_QuadSPI_Blocking setup starting");
    Serial.println("4-way Parallel SPI Blocking driver test");
    Serial.println("Architecture: 4 data pins + 1 clock pin (inline bit-banging)");
    Serial.print("Test data size: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");
    Serial.println("");

    Serial.println("Pin configuration:");
    Serial.print("  Data pin 0 (LSB): GPIO");
    Serial.println(DATA_PIN_0);
    Serial.print("  Data pin 1:       GPIO");
    Serial.println(DATA_PIN_1);
    Serial.print("  Data pin 2:       GPIO");
    Serial.println(DATA_PIN_2);
    Serial.print("  Data pin 3 (MSB): GPIO");
    Serial.println(DATA_PIN_3);
    Serial.print("  Clock pin:        GPIO");
    Serial.println(CLOCK_PIN);
    Serial.println("");

    // Initialize test data with all 16 possible 4-bit patterns
    // Each byte only uses lower 4 bits (upper 4 bits ignored)
    testData[0]  = 0x00;  // 0000 - All pins low
    testData[1]  = 0x01;  // 0001 - D0 high
    testData[2]  = 0x02;  // 0010 - D1 high
    testData[3]  = 0x03;  // 0011 - D0+D1 high
    testData[4]  = 0x04;  // 0100 - D2 high
    testData[5]  = 0x05;  // 0101 - D0+D2 high
    testData[6]  = 0x06;  // 0110 - D1+D2 high
    testData[7]  = 0x07;  // 0111 - D0+D1+D2 high
    testData[8]  = 0x08;  // 1000 - D3 high
    testData[9]  = 0x09;  // 1001 - D0+D3 high
    testData[10] = 0x0A;  // 1010 - D1+D3 high
    testData[11] = 0x0B;  // 1011 - D0+D1+D3 high
    testData[12] = 0x0C;  // 1100 - D2+D3 high
    testData[13] = 0x0D;  // 1101 - D0+D2+D3 high
    testData[14] = 0x0E;  // 1110 - D1+D2+D3 high
    testData[15] = 0x0F;  // 1111 - All pins high

    Serial.println("Test data initialized (4-bit patterns):");
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        Serial.print("  testData[");
        Serial.print(i);
        Serial.print("] = 0x");
        Serial.print(testData[i], HEX);
        Serial.print(" (binary: ");
        for (int b = 3; b >= 0; b--) {  // Show all 4 bits
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
            Serial.println("✓ Esp32C3_QuadSPI_Blocking test PASSED");
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

    // Create Quad-SPI blocking driver instance
    QuadSPI_Blocking_ESP32 spi;

    Serial.println("  Configuring Quad-SPI blocking driver...");

    // Set pin mapping for 4 data pins + 1 clock
    spi.setPinMapping(DATA_PIN_0, DATA_PIN_1, DATA_PIN_2, DATA_PIN_3, CLOCK_PIN);
    Serial.println("  Pin mapping configured (4 data + 1 clock)");

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
    // For 4-way SPI: 4 bits transmitted per clock cycle
    // Total bits = TEST_DATA_SIZE bytes * 4 bits/byte
    uint32_t totalBits = TEST_DATA_SIZE * 4;
    float bitRateKHz = (float)totalBits / (float)duration * 1000.0f;
    Serial.print("  Effective bit rate: ");
    Serial.print(bitRateKHz, 2);
    Serial.println(" kHz (per lane)");

    // Calculate total throughput
    float throughputKHz = bitRateKHz * 4;  // 4 lanes
    Serial.print("  Total throughput: ");
    Serial.print(throughputKHz, 2);
    Serial.println(" kHz (all lanes combined)");

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
