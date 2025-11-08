// Multi-lane SPI example for FastLED
// Demonstrates using 4 independent SPI lanes to drive 4 LED strips
// @filter (mem is high)

#include "FastLED.h"
#include "fl/dbg.h"

#define CLOCK_PIN 3

// Use explicit array initialization (AVR compiler requires this)
int pins[] = {0, 1, 2, 5};

fl::Spi spi_device(CLOCK_PIN, pins, fl::SPI_HW);

void setup() {
    Serial.begin(115200);


    if (!spi_device.ok()) {
        FL_WARN("SPI device failed to initialize!" << spi_device.error());
    } else {
        FL_DBG("SPI device initialized with 4 lanes");
    }
}

void loop() {
    if (!spi_device.ok()) {
        FL_WARN("SPI device was not initialized!");
        delay(1000);
        return;
    }

    // Example data for each lane - all lanes must have the same size
    // Using C arrays and manual initialization for AVR compatibility
    static const uint8_t lane0[] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
                                          0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    static const uint8_t lane1[] = {0xAA, 0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lane2[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lane3[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                          0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};


    // Simple API - write all 4 lanes at once (starts transmission asynchronously)
    auto result = spi_device.write(lane0, lane1, lane2, lane3);
    if (!result.ok) {
        FL_WARN("Write failed: " << result.error);
        delay(1000);
        return;
    }

    // Wait for transmission to complete (blocks until done)
    spi_device.wait();

    FL_DBG("Data transmitted to 4 lanes");

    // Delay before next transmission
    delay(100);
}
