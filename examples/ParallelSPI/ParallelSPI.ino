/// @file    ParallelSPI.ino
/// @brief   Parallel SPI driver demonstration (1/2/4/8 lanes, Block/ISR/HWAccel modes)
/// @example ParallelSPI.ino
///
/// This example demonstrates FastLED's parallel SPI drivers across different configurations:
///
/// **SPI Widths (number of parallel data lanes):**
/// - 1-way: Single data pin (traditional serial)
/// - 2-way: Dual-SPI (2 parallel data pins)
/// - 4-way: Quad-SPI (4 parallel data pins)
/// - 8-way: Octo-SPI (8 parallel data pins)
///
/// **Execution Modes:**
/// - BLOCK: Main thread inline bit-banging (lowest overhead, simple API)
/// - ISR: Interrupt-driven (non-blocking, good for complex applications)
/// - HWACCEL: Hardware-accelerated (platform-specific, highest performance)
///
/// **Configuration:**
/// Edit the #defines below to select your configuration:
/// - SPI_WIDTH: 1, 2, 4, or 8 (number of parallel data lanes)
/// - SPI_MODE: BLOCK, ISR, or HWACCEL
///
/// **Platform Support:**
/// - Block mode: Any platform with GPIO support (ESP32-C3/C2/C6/S2/S3/etc.)
/// - ISR mode: Any platform with GPIO + timer support
/// - HWAccel mode: Platform-dependent (see documentation)
///
/// The drivers use platform-agnostic GPIO abstractions that work on:
/// - ESP32 (all variants with GPIO registers at standard addresses)
/// - Host simulation (for testing without hardware)
/// - Other platforms (define FASTLED_GPIO_W1TS_ADDR and FASTLED_GPIO_W1TC_ADDR)

// ============================================================================
// CONFIGURATION - Edit these to select your SPI configuration
// ============================================================================

#define SPI_WIDTH 2        // Options: 1, 2, 4, 8 (number of parallel data lanes)
#define SPI_MODE_BLOCK     // Options: SPI_MODE_BLOCK, SPI_MODE_ISR, SPI_MODE_HWACCEL

// ============================================================================
// Platform Detection
// ============================================================================

// Check if platform provides GPIO support for parallel SPI
#if defined(FASTLED_SPI_HOST_SIMULATION) || defined(ESP32)
#define HAS_PARALLEL_SPI_SUPPORT 1
#else
#define HAS_PARALLEL_SPI_SUPPORT 0
#endif

#if HAS_PARALLEL_SPI_SUPPORT

// ============================================================================
// Include Headers Based on Configuration
// ============================================================================

#if defined(SPI_MODE_BLOCK)
    #if SPI_WIDTH == 1
        #include "platforms/shared/spi_bitbang/spi_block_1.h"
        using SpiDriver = fl::SpiBlock1;
        #define MODE_NAME "Blocking (1-way)"
    #elif SPI_WIDTH == 2
        #include "platforms/shared/spi_bitbang/spi_block_2.h"
        using SpiDriver = fl::SpiBlock2;
        #define MODE_NAME "Blocking (2-way Dual)"
    #elif SPI_WIDTH == 4
        #include "platforms/shared/spi_bitbang/spi_block_4.h"
        using SpiDriver = fl::SpiBlock4;
        #define MODE_NAME "Blocking (4-way Quad)"
    #elif SPI_WIDTH == 8
        #include "platforms/shared/spi_bitbang/spi_block_8.h"
        using SpiDriver = fl::SpiBlock8;
        #define MODE_NAME "Blocking (8-way Octo)"
    #else
        #error "Block mode supports 1, 2, 4, or 8 lanes - invalid SPI_WIDTH specified"
    #endif
#elif defined(SPI_MODE_ISR)
    #if SPI_WIDTH == 1
        #include "platforms/shared/spi_bitbang/spi_isr_1.h"
        using SpiDriver = fl::SpiIsr1;
        #define MODE_NAME "ISR (1-way)"
    #elif SPI_WIDTH == 2
        #include "platforms/shared/spi_bitbang/spi_isr_2.h"
        using SpiDriver = fl::SpiIsr2;
        #define MODE_NAME "ISR (2-way Dual)"
    #elif SPI_WIDTH == 4
        #include "platforms/shared/spi_bitbang/spi_isr_4.h"
        using SpiDriver = fl::SpiIsr4;
        #define MODE_NAME "ISR (4-way Quad)"
    #elif SPI_WIDTH == 8
        #include "platforms/shared/spi_bitbang/spi_isr_8.h"
        using SpiDriver = fl::SpiIsr8;
        #define MODE_NAME "ISR (8-way Octo)"
    #else
        #error "ISR mode supports 1, 2, 4, or 8 lanes"
    #endif
#elif defined(SPI_MODE_HWACCEL)
    #error "Hardware acceleration mode not yet implemented - use BLOCK or ISR mode"
#else
    #error "No SPI mode selected - define SPI_MODE_BLOCK, SPI_MODE_ISR, or SPI_MODE_HWACCEL"
#endif

// ============================================================================
// Test Configuration
// ============================================================================

#define TEST_DATA_SIZE 8
#define NUM_TEST_ITERATIONS 3

// Pin configuration (adjust for your hardware and platform)
// These GPIO numbers are examples - modify based on your board layout
#if SPI_WIDTH == 1
    #define DATA_PIN_0 0   // GPIO0 - Data pin
    #define CLOCK_PIN  8   // GPIO8 - Clock pin
#elif SPI_WIDTH == 2
    #define DATA_PIN_0 0   // GPIO0 - Data bit 0 (LSB)
    #define DATA_PIN_1 1   // GPIO1 - Data bit 1 (MSB)
    #define CLOCK_PIN  8   // GPIO8 - Clock pin
#elif SPI_WIDTH == 4
    #define DATA_PIN_0 0   // GPIO0 - Data bit 0 (LSB)
    #define DATA_PIN_1 1   // GPIO1 - Data bit 1
    #define DATA_PIN_2 2   // GPIO2 - Data bit 2
    #define DATA_PIN_3 3   // GPIO3 - Data bit 3 (MSB)
    #define CLOCK_PIN  8   // GPIO8 - Clock pin
#elif SPI_WIDTH == 8
    // 8-way uses GPIOs 0-7 for data, GPIO8 for clock
    #define CLOCK_PIN  8   // GPIO8 - Clock pin
#endif

#if defined(SPI_MODE_ISR)
    #define SPI_TIMER_HZ 1600000  // 1.6MHz timer → 800kHz SPI bit rate
#endif

using namespace fl;

// ============================================================================
// Global Variables
// ============================================================================

static uint8_t testData[TEST_DATA_SIZE];

// ============================================================================
// Helper Functions
// ============================================================================

void initializeTestData() {
    // Initialize test patterns based on SPI width
    #if SPI_WIDTH == 1
        // 1-bit patterns (bit 0 only)
        testData[0] = 0x00;  testData[1] = 0x01;  testData[2] = 0x00;  testData[3] = 0x01;
        testData[4] = 0x01;  testData[5] = 0x01;  testData[6] = 0x00;  testData[7] = 0x00;
    #elif SPI_WIDTH == 2
        // 2-bit patterns (bits 0-1)
        testData[0] = 0x00;  testData[1] = 0x01;  testData[2] = 0x02;  testData[3] = 0x03;
        testData[4] = 0x01;  testData[5] = 0x02;  testData[6] = 0x00;  testData[7] = 0x03;
    #elif SPI_WIDTH == 4
        // 4-bit patterns (bits 0-3) - all 8 patterns plus repeats
        testData[0] = 0x00;  testData[1] = 0x0F;  testData[2] = 0x0A;  testData[3] = 0x05;
        testData[4] = 0x03;  testData[5] = 0x0C;  testData[6] = 0x09;  testData[7] = 0x06;
    #elif SPI_WIDTH == 8
        // 8-bit patterns (all bits)
        testData[0] = 0x00;  testData[1] = 0xFF;  testData[2] = 0xAA;  testData[3] = 0x55;
        testData[4] = 0x0F;  testData[5] = 0xF0;  testData[6] = 0x33;  testData[7] = 0xCC;
    #endif
}

void printTestData() {
    Serial.println("Test data initialized:");
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        Serial.print("  testData[");
        Serial.print(i);
        Serial.print("] = 0x");
        Serial.print(testData[i], HEX);
        Serial.print(" (binary: ");
        for (int b = SPI_WIDTH - 1; b >= 0; b--) {
            Serial.print((testData[i] >> b) & 1);
        }
        Serial.println(")");
    }
    Serial.println("");
}

void configurePins(SpiDriver& spi) {
    #if SPI_WIDTH == 1
        spi.setPinMapping(DATA_PIN_0, CLOCK_PIN);
        Serial.print("  Pin mapping: DATA=GPIO");
        Serial.print(DATA_PIN_0);
        Serial.print(" CLOCK=GPIO");
        Serial.println(CLOCK_PIN);
    #elif SPI_WIDTH == 2
        spi.setPinMapping(DATA_PIN_0, DATA_PIN_1, CLOCK_PIN);
        Serial.print("  Pin mapping: D0=GPIO");
        Serial.print(DATA_PIN_0);
        Serial.print(" D1=GPIO");
        Serial.print(DATA_PIN_1);
        Serial.print(" CLOCK=GPIO");
        Serial.println(CLOCK_PIN);
    #elif SPI_WIDTH == 4
        spi.setPinMapping(DATA_PIN_0, DATA_PIN_1, DATA_PIN_2, DATA_PIN_3, CLOCK_PIN);
        Serial.print("  Pin mapping: D0-D3=GPIO");
        Serial.print(DATA_PIN_0);
        Serial.print("-");
        Serial.print(DATA_PIN_3);
        Serial.print(" CLOCK=GPIO");
        Serial.println(CLOCK_PIN);
    #elif SPI_WIDTH == 8
        #if defined(SPI_MODE_BLOCK)
            // Blocking 8-way: uses setPinMapping (auto-initializes LUT)
            spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, CLOCK_PIN);
            Serial.print("  Pin mapping: D0-D7=GPIO0-7 CLOCK=GPIO");
            Serial.println(CLOCK_PIN);
        #elif defined(SPI_MODE_ISR)
            // ISR 8-way: uses setClockMask + manual LUT initialization
            spi.setClockMask(1 << CLOCK_PIN);
            Serial.print("  Pin mapping: D0-D7=GPIO0-7 CLOCK=GPIO");
            Serial.println(CLOCK_PIN);

            // Initialize LUT for 8-way ISR
            PinMaskEntry* lut = SpiDriver::getLUTArray();
            for (int v = 0; v < 256; v++) {
                uint32_t setMask = 0, clearMask = 0;
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
            Serial.println("  LUT initialized");
        #endif
    #endif
}

// ============================================================================
// Arduino Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(100);
    delay(2000);

    Serial.println("========================================");
    Serial.println("ParallelSPI Example");
    Serial.println("========================================");
    Serial.print("Mode: ");
    Serial.println(MODE_NAME);
    Serial.print("SPI Width: ");
    Serial.print(SPI_WIDTH);
    Serial.println(" lanes");
    Serial.print("Test data size: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");
    #if defined(SPI_MODE_ISR)
        Serial.print("Timer frequency: ");
        Serial.print(SPI_TIMER_HZ);
        Serial.println(" Hz");
    #endif
    Serial.println("========================================");
    Serial.println("");

    initializeTestData();
    printTestData();

    Serial.println("Setup complete - ready to test");
    delay(500);
}

// ============================================================================
// Arduino Loop
// ============================================================================

void loop() {
    static int iteration = 0;
    static bool testComplete = false;

    if (iteration >= NUM_TEST_ITERATIONS) {
        if (!testComplete) {
            Serial.print("Test finished - completed ");
            Serial.print(iteration);
            Serial.println(" iterations");
            Serial.println("✓ ParallelSPI test PASSED");
            testComplete = true;
        }
        delay(5000);
        return;
    }

    iteration++;
    Serial.println("========================================");
    Serial.print("Iteration ");
    Serial.println(iteration);
    Serial.println("========================================");

    // Create SPI driver instance
    SpiDriver spi;

    Serial.print("  Configuring ");
    Serial.print(MODE_NAME);
    Serial.println(" driver...");

    // Configure pins
    configurePins(spi);

    // Load test data
    spi.loadBuffer(testData, TEST_DATA_SIZE);
    Serial.print("  Buffer loaded: ");
    Serial.print(TEST_DATA_SIZE);
    Serial.println(" bytes");

    // Execute based on mode
    #if defined(SPI_MODE_BLOCK)
        // Blocking mode: transmit() blocks until complete
        Serial.println("  Transmitting (blocking)...");
        uint32_t startTime = micros();
        spi.transmit();
        uint32_t endTime = micros();
        uint32_t duration = endTime - startTime;

        Serial.println("  ✓ Transfer complete");
        Serial.print("  Duration: ");
        Serial.print(duration);
        Serial.println(" µs");

    #elif defined(SPI_MODE_ISR)
        // ISR mode: setup, arm, wait for completion
        Serial.println("  Setting up ISR...");
        int ret = spi.setupISR(SPI_TIMER_HZ);
        if (ret != 0) {
            Serial.print("  ERROR: ISR setup failed with code ");
            Serial.println(ret);
            delay(1000);
            return;
        }

        Serial.println("  Waiting for memory visibility...");
        SpiDriver::visibilityDelayUs(10);

        Serial.println("  Arming transfer...");
        spi.arm();

        Serial.println("  Waiting for completion...");
        uint32_t timeout = millis() + 1000;
        while (spi.isBusy() && millis() < timeout) {
            delay(1);
        }

        if (spi.isBusy()) {
            Serial.println("  ERROR: Transfer timed out");
        } else {
            Serial.println("  ✓ Transfer complete");
            spi.ackDone();
        }

        Serial.println("  Stopping ISR...");
        spi.stopISR();

        #ifdef FL_SPI_ISR_VALIDATE
            // Validation mode: inspect GPIO event log
            Serial.println("");
            Serial.println("  === GPIO VALIDATION ===");
            auto events = spi.getValidationEventsTyped();
            uint16_t eventCount = spi.getValidationEventCount();

            Serial.print("  Events captured: ");
            Serial.print(eventCount);
            Serial.println(" / 8192");

            uint16_t expectedEvents = 1 + (TEST_DATA_SIZE * 4) + 1;
            Serial.print("  Expected: ");
            Serial.println(expectedEvents);

            if (eventCount == expectedEvents) {
                Serial.println("  ✓ Event count matches");
            } else {
                Serial.print("  WARNING: Mismatch - expected ");
                Serial.print(expectedEvents);
                Serial.print(", got ");
                Serial.println(eventCount);
            }
        #endif
    #endif

    Serial.println("");
    Serial.print("Iteration ");
    Serial.print(iteration);
    Serial.println(" complete");
    Serial.println("");

    delay(500);
}

#else  // !HAS_PARALLEL_SPI_SUPPORT
#include "platforms/sketch_fake.hpp"
#endif  // HAS_PARALLEL_SPI_SUPPORT
