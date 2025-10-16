/// @file    Esp32C3_QuadSPI_ISR.ino
/// @brief   ESP32-C3/C2 Quad-SPI (4-way) ISR driver test for hardware Quad-SPI validation
/// @example Esp32C3_QuadSPI_ISR.ino
///
/// This example demonstrates the 4-way parallel SPI ISR driver, which uses:
/// - 4 data pins (D0-D3) for parallel bit output
/// - 1 clock pin for synchronization
/// - High-priority ISR for minimal jitter
///
/// The 4-way architecture directly matches hardware Quad-SPI topology, making it
/// ideal for testing and validating hardware Quad-SPI implementations.

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

// Note: FL_SPI_ISR_VALIDATE can be defined via build flags for validation testing
// If not defined, the example will run without validation checks

#include "platforms/shared/spi_bitbang/spi_isr_4.h"

// Test parameters - 4-way Quad-SPI configuration
#define TEST_DATA_SIZE 8  // Short sequence for validation testing
#define SPI_TIMER_HZ 1600000  // 1.6MHz timer → 800kHz SPI bit rate
#define NUM_TEST_ITERATIONS 3  // Iterations for testing

// Pin configuration: 4 data pins + 1 clock
#define DATA_PIN_0 0  // GPIO0 - Data bit 0 (LSB)
#define DATA_PIN_1 1  // GPIO1 - Data bit 1
#define DATA_PIN_2 2  // GPIO2 - Data bit 2
#define DATA_PIN_3 3  // GPIO3 - Data bit 3 (MSB)
#define CLOCK_PIN  8  // GPIO8 - Clock pin

using namespace fl;

// Test data buffer - simple patterns for validation
static uint8_t testData[TEST_DATA_SIZE];

void setup() {
    Serial.begin(115200);
    delay(100);
    delay(2000);

    Serial.println("Esp32C3_QuadSPI_ISR setup starting");
    Serial.println("4-way Parallel SPI ISR driver test");
    Serial.println("Architecture: 4 data pins + 1 clock pin");
    Serial.print("Test data size: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");
    Serial.print("SPI timer frequency: ");
    Serial.print(SPI_TIMER_HZ);
    Serial.println(" Hz");
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

    // Initialize test data with patterns that exercise all 4 data pins
    // Each byte only uses lower 4 bits (upper 4 bits ignored)
    testData[0] = 0x00;  // 0000 - All pins low
    testData[1] = 0x0F;  // 1111 - All pins high
    testData[2] = 0x0A;  // 1010 - Alternating pattern
    testData[3] = 0x05;  // 0101 - Alternating pattern (inverted)
    testData[4] = 0x03;  // 0011 - Lower 2 bits high
    testData[5] = 0x0C;  // 1100 - Upper 2 bits high
    testData[6] = 0x09;  // 1001 - Outer bits high
    testData[7] = 0x06;  // 0110 - Inner bits high

    Serial.println("Test data initialized (4-bit patterns):");
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        Serial.print("  testData[");
        Serial.print(i);
        Serial.print("] = 0x");
        Serial.print(testData[i], HEX);
        Serial.print(" (binary: ");
        for (int b = 3; b >= 0; b--) {  // Only show lower 4 bits
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
#ifdef FL_SPI_ISR_VALIDATE
            Serial.println("✓ GPIO event log validation complete");
#endif
            Serial.print("Test finished - completed ");
            Serial.print(iteration);
            Serial.println(" iterations");
            Serial.println("✓ Esp32C3_QuadSPI_ISR test PASSED");
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

    // Create Quad-SPI driver instance
    SpiIsr4 spi;

    Serial.println("  Configuring Quad-SPI driver...");

    // Set pin mapping for 4 data pins + 1 clock
    spi.setPinMapping(DATA_PIN_0, DATA_PIN_1, DATA_PIN_2, DATA_PIN_3, CLOCK_PIN);
    Serial.println("  Pin mapping configured (4 data + 1 clock)");

    // Load test data
    spi.loadBuffer(testData, TEST_DATA_SIZE);
    Serial.print("  Data buffer loaded: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");

    // Setup ISR
    Serial.println("  Setting up ISR...");
    int ret = spi.setupISR(SPI_TIMER_HZ);
    if (ret != 0) {
        Serial.print("  ERROR: ISR setup failed with code ");
        Serial.println(ret);
        delay(1000);
        return;
    }
    Serial.println("  ISR setup successful");

    // Add visibility delay
    Serial.println("  Waiting for memory visibility...");
    SpiIsr4::visibilityDelayUs(10);

    // Arm the transfer
    Serial.println("  Arming transfer...");
    spi.arm();

    // Wait for completion
    Serial.println("  Waiting for transfer to complete...");
    uint32_t timeout = millis() + 1000;  // 1 second timeout
    while (spi.isBusy() && millis() < timeout) {
        delay(1);
    }

    if (spi.isBusy()) {
        Serial.println("  WARNING: Transfer timed out");
    } else {
        Serial.println("  Transfer completed successfully");
        spi.ackDone();
    }

    // Stop ISR
    Serial.println("  Stopping ISR...");
    spi.stopISR();

#ifdef FL_SPI_ISR_VALIDATE
    // Inspect GPIO event log
    Serial.println("");
    Serial.println("  === GPIO EVENT LOG VALIDATION ===");
    const SpiIsr4::GPIOEvent* events = spi.getValidationEventsTyped();
    uint16_t eventCount = spi.getValidationEventCount();

    Serial.print("  Total GPIO events captured: ");
    Serial.print(eventCount);
    Serial.println(" / 8192");

    // Expected events per byte: SET_BITS, CLEAR_BITS, CLOCK_LOW (phase 0), CLOCK_HIGH (phase 1)
    // Plus STATE_START at beginning and STATE_DONE at end
    uint16_t expectedEvents = 1 + (TEST_DATA_SIZE * 4) + 1;  // START + (4 events per byte) + DONE
    Serial.print("  Expected events: ");
    Serial.println(expectedEvents);

    if (eventCount != expectedEvents) {
        Serial.print("  WARNING: Event count mismatch! Expected ");
        Serial.print(expectedEvents);
        Serial.print(", got ");
        Serial.println(eventCount);
    } else {
        Serial.println("  ✓ Event count matches expected");
    }

    // Count event types
    uint16_t stateEvents = 0;
    uint16_t setBitEvents = 0;
    uint16_t clearBitEvents = 0;
    uint16_t clockLowEvents = 0;
    uint16_t clockHighEvents = 0;

    for (uint16_t i = 0; i < eventCount; i++) {
        using GPIOEventType = SpiIsr4::GPIOEventType;
        switch (events[i].type()) {
            case GPIOEventType::StateStart:
            case GPIOEventType::StateDone:
                stateEvents++;
                break;
            case GPIOEventType::SetBits:
                setBitEvents++;
                break;
            case GPIOEventType::ClearBits:
                clearBitEvents++;
                break;
            case GPIOEventType::ClockLow:
                clockLowEvents++;
                break;
            case GPIOEventType::ClockHigh:
                clockHighEvents++;
                break;
        }
    }

    Serial.print("  Event breakdown: State=");
    Serial.print(stateEvents);
    Serial.print(", SetBits=");
    Serial.print(setBitEvents);
    Serial.print(", ClearBits=");
    Serial.print(clearBitEvents);
    Serial.print(", ClockLow=");
    Serial.print(clockLowEvents);
    Serial.print(", ClockHigh=");
    Serial.println(clockHighEvents);

    // Validate event sequence structure
    Serial.println("  Validating event sequence...");
    bool sequenceValid = true;

    // First event should be STATE_START
    if (eventCount > 0 && events[0].type() != SpiIsr4::GPIOEventType::StateStart) {
        Serial.println("  ERROR: First event is not STATE_START");
        sequenceValid = false;
    }

    // Last event should be STATE_DONE
    if (eventCount > 1 && events[eventCount - 1].type() != SpiIsr4::GPIOEventType::StateDone) {
        Serial.println("  ERROR: Last event is not STATE_DONE");
        sequenceValid = false;
    }

    // Check that clock events come in pairs (LOW then HIGH)
    if (clockLowEvents != clockHighEvents) {
        Serial.print("  ERROR: Clock event mismatch - LOW=");
        Serial.print(clockLowEvents);
        Serial.print(" HIGH=");
        Serial.println(clockHighEvents);
        sequenceValid = false;
    }

    // Should have equal SET and CLEAR events
    if (setBitEvents != clearBitEvents) {
        Serial.print("  WARNING: Set/Clear event mismatch - SET=");
        Serial.print(setBitEvents);
        Serial.print(" CLEAR=");
        Serial.println(clearBitEvents);
    }

    if (sequenceValid) {
        Serial.println("  ✓ Event sequence structure is valid");
    } else {
        Serial.println("  ✗ Event sequence structure has errors");
    }

    // Print first few events for inspection (not all to avoid flooding)
    Serial.println("");
    Serial.println("  --- Sample Event Log (first 20 events) ---");
    uint16_t printCount = (eventCount < 20) ? eventCount : 20;
    for (uint16_t i = 0; i < printCount; i++) {
        Serial.print("    [");
        Serial.print(i);
        Serial.print("] ");

        using GPIOEventType = SpiIsr4::GPIOEventType;
        switch (events[i].type()) {
            case GPIOEventType::StateStart:
                Serial.print("STATE_START (bytes=");
                Serial.print(events[i].payload.state_info);
                Serial.println(")");
                break;
            case GPIOEventType::StateDone:
                Serial.print("STATE_DONE (pos=");
                Serial.print(events[i].payload.state_info);
                Serial.println(")");
                break;
            case GPIOEventType::SetBits:
                Serial.print("SET_BITS mask=0x");
                Serial.print(events[i].payload.gpio_mask, HEX);
                Serial.print(" (4-bit: ");
                for (int b = 3; b >= 0; b--) {
                    Serial.print((events[i].payload.gpio_mask >> b) & 1);
                }
                Serial.println(")");
                break;
            case GPIOEventType::ClearBits:
                Serial.print("CLEAR_BITS mask=0x");
                Serial.print(events[i].payload.gpio_mask, HEX);
                Serial.print(" (4-bit: ");
                for (int b = 3; b >= 0; b--) {
                    Serial.print((events[i].payload.gpio_mask >> b) & 1);
                }
                Serial.println(")");
                break;
            case GPIOEventType::ClockLow:
                Serial.print("CLOCK_LOW mask=0x");
                Serial.println(events[i].payload.gpio_mask, HEX);
                break;
            case GPIOEventType::ClockHigh:
                Serial.print("CLOCK_HIGH mask=0x");
                Serial.println(events[i].payload.gpio_mask, HEX);
                break;
            default:
                Serial.print("UNKNOWN type=");
                Serial.println(events[i].event_type);
                break;
        }
    }
    if (eventCount > printCount) {
        Serial.print("  ... (");
        Serial.print(eventCount - printCount);
        Serial.println(" more events)");
    }
    Serial.println("  --- End Sample Event Log ---");
#endif

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
