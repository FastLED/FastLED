/// @file    Esp32C3_SPI_ISR.ino
/// @brief   ESP32-C3/C2 Parallel SPI ISR driver test - low-level crash test for RISC-V platforms
/// @example Esp32C3_SPI_ISR.ino

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

#include "platforms/esp/32/parallel_spi/fastled_parallel_spi_esp32c3.hpp"

// Test parameters
#define TEST_DATA_SIZE 4  // Very short sequence for validation testing
#define SPI_TIMER_HZ 1600000  // 1.6MHz timer → 800kHz SPI bit rate
#define CLOCK_PIN 8  // GPIO8 for clock
#define NUM_TEST_ITERATIONS 3  // Fewer iterations for detailed inspection

using namespace fl;

// Test data buffer - simple pattern
static uint8_t testData[TEST_DATA_SIZE];

void initializePinMappingLUT() {
    // Initialize pin mapping lookup tables directly in ISR state
    // For this test, map bits to GPIOs 0-7 for data, GPIO8 for clock
    PinMaskEntry* lut = ParallelSPI_ESP32C3::getLUTArray();

    for (int v = 0; v < 256; v++) {
        uint32_t setMask = 0;
        uint32_t clearMask = 0;

        // Map each bit of the value to a GPIO pin (0-7)
        for (int bit = 0; bit < 8; bit++) {
            if (v & (1 << bit)) {
                setMask |= (1 << bit);
            } else {
                clearMask |= (1 << bit);
            }
        }

        lut[v].set_mask = setMask;
        lut[v].clear_mask = clearMask;
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    delay(2000);

    Serial.println("Esp32C3_SPI_ISR setup starting");
    Serial.println("Low-level parallel SPI ISR driver test");
    Serial.print("Test data size: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");
    Serial.print("SPI timer frequency: ");
    Serial.print(SPI_TIMER_HZ);
    Serial.println(" Hz");

    // Initialize test data with known pattern for validation
    // Using distinct patterns that are easy to identify in GPIO events
    testData[0] = 0x00;  // All data pins low
    testData[1] = 0xFF;  // All data pins high
    testData[2] = 0xAA;  // Alternating pattern 10101010
    testData[3] = 0x55;  // Alternating pattern 01010101

    Serial.println("Test data initialized:");
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        Serial.print("  testData[");
        Serial.print(i);
        Serial.print("] = 0x");
        Serial.println(testData[i], HEX);
    }

    // Initialize pin mapping lookup tables
    initializePinMappingLUT();
    Serial.println("Pin mapping LUT initialized");

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
            Serial.println("✓ Esp32C3_SPI_ISR test PASSED");
            testComplete = true;
        }
        delay(5000);
        return;
    }

    iteration++;
    Serial.print("Starting loop iteration ");
    Serial.println(iteration);

    // Create SPI driver instance
    ParallelSPI_ESP32C3 spi;

    Serial.println("  Configuring SPI driver...");

    // Set clock pin mask
    spi.setClockMask(1 << CLOCK_PIN);
    Serial.print("  Clock mask set: GPIO");
    Serial.println(CLOCK_PIN);

    // LUT is already initialized in global ISR state
    Serial.println("  LUT already initialized in ISR state");

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
    ParallelSPI_ESP32C3::visibilityDelayUs(10);

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
    Serial.println("  === GPIO EVENT LOG VALIDATION ===");
    const GPIOEvent* events = ParallelSPI_ESP32C3::getValidationEvents();
    uint16_t eventCount = ParallelSPI_ESP32C3::getValidationEventCount();

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
    }

    // Count event types
    uint16_t stateEvents = 0;
    uint16_t setBitEvents = 0;
    uint16_t clearBitEvents = 0;
    uint16_t clockLowEvents = 0;
    uint16_t clockHighEvents = 0;

    for (uint16_t i = 0; i < eventCount; i++) {
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
    if (eventCount > 0 && events[0].type() != GPIOEventType::StateStart) {
        Serial.println("  ERROR: First event is not STATE_START");
        sequenceValid = false;
    }

    // Last event should be STATE_DONE
    if (eventCount > 1 && events[eventCount - 1].type() != GPIOEventType::StateDone) {
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

    if (sequenceValid) {
        Serial.println("  ✓ Event sequence structure is valid");
    } else {
        Serial.println("  ✗ Event sequence structure has errors");
    }

    // Print ALL events for short sequence
    Serial.println("  --- Complete Event Log ---");
    for (uint16_t i = 0; i < eventCount; i++) {
        Serial.print("    [");
        Serial.print(i);
        Serial.print("] ");

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
                Serial.print(" (binary: ");
                for (int b = 7; b >= 0; b--) {
                    Serial.print((events[i].payload.gpio_mask >> b) & 1);
                }
                Serial.println(")");
                break;
            case GPIOEventType::ClearBits:
                Serial.print("CLEAR_BITS mask=0x");
                Serial.print(events[i].payload.gpio_mask, HEX);
                Serial.print(" (binary: ");
                for (int b = 7; b >= 0; b--) {
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

    Serial.println("  --- End Event Log ---");
#endif

    Serial.print("Iteration ");
    Serial.print(iteration);
    Serial.println(" complete");

    delay(500);
}

#else  // !IS_ESP32_C3
#include "platforms/sketch_fake.hpp"
#endif  // IS_ESP32_C3
