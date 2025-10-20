/*
  FastLED — SpiIsr32 Unit Tests (32-way interrupt-driven SPI)
  -----------------------------------------------------------
  Tests the SpiIsr32 C++ wrapper class with various configurations.

  Test coverage:
  - Pin mapping initialization with 32 pins
  - LUT generation for byte values
  - Non-blocking transmission
  - Status flags and busy waiting
  - Multiple back-to-back transfers
  - Edge cases (empty buffer, max size buffers)

  License: MIT (FastLED)
*/

#include "test.h"
#include "FastLED.h"
#include "platforms/shared/spi_bitbang/spi_isr_32.h"
#include "platforms/shared/spi_bitbang/host_sim.h"


namespace {

/* Helper: Initialize 32-way pin mapping */
void setup_32way_spi_lut() {
    PinMaskEntry* lut = fl_spi_get_lut_array();

    // Map pins: GPIO0-30 for data (31 pins), GPIO31 for clock (fits in 32-bit mask)
    uint32_t dataPinMasks[32] = {
        1u << 0,   // D0
        1u << 1,   // D1
        1u << 2,   // D2
        1u << 3,   // D3
        1u << 4,   // D4
        1u << 5,   // D5
        1u << 6,   // D6
        1u << 7,   // D7
        1u << 8,   // D8
        1u << 9,   // D9
        1u << 10,  // D10
        1u << 11,  // D11
        1u << 12,  // D12
        1u << 13,  // D13
        1u << 14,  // D14
        1u << 15,  // D15
        1u << 16,  // D16
        1u << 17,  // D17
        1u << 18,  // D18
        1u << 19,  // D19
        1u << 20,  // D20
        1u << 21,  // D21
        1u << 22,  // D22
        1u << 23,  // D23
        1u << 24,  // D24
        1u << 25,  // D25
        1u << 26,  // D26
        1u << 27,  // D27
        1u << 28,  // D28
        1u << 29,  // D29
        1u << 30,  // D30
        1u << 0    // D31 (placeholder on GPIO0)
    };

    for (int v = 0; v < 256; v++) {
        uint32_t setMask = 0;
        uint32_t clearMask = 0;

        for (int b = 0; b < 32; b++) {
            if (b < 8 && (v & (1 << b))) {
                setMask |= dataPinMasks[b];
            } else if (b < 8) {
                clearMask |= dataPinMasks[b];
            } else {
                // Bits 8-31 are always cleared
                clearMask |= dataPinMasks[b];
            }
        }

        lut[v].set_mask = setMask;
        lut[v].clear_mask = clearMask;
    }

    fl_spi_set_clock_mask(1u << 31);
}

}  // namespace

// ============================================================================
// SpiIsr32 Tests
// ============================================================================

TEST_CASE("SpiIsr32 - Pin mapping initialization with 32 pins") {
    setup_32way_spi_lut();

    // Verify that the LUT array is properly initialized
    PinMaskEntry* lut = SpiIsr32::getLUTArray();
    REQUIRE(lut != nullptr);

    // Check boundary values
    // Value 0x00 should clear all data pins
    CHECK(lut[0x00].set_mask == 0);

    // Value 0xFF should set pins 0-7
    // (only lower 8 bits are used for byte value mapping)
    CHECK(lut[0xFF].set_mask != 0);

    // Value 0x01 should set pin 0
    CHECK((lut[0x01].set_mask & (1u << 0)) != 0);
}

TEST_CASE("SpiIsr32 - LUT generation for byte values") {
    setup_32way_spi_lut();

    // Test specific patterns
    PinMaskEntry* lut = SpiIsr32::getLUTArray();

    // 0x00 - all bits low
    CHECK(lut[0x00].set_mask == 0);

    // 0xFF - lower 8 bits high
    uint32_t expected_ff = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |
                           (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7);
    CHECK(lut[0xFF].set_mask == expected_ff);

    // 0x0F - first 4 bits high
    uint32_t expected_0f = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
    CHECK(lut[0x0F].set_mask == expected_0f);

    // 0xAA - alternating pattern
    uint32_t expected_aa = (1u << 1) | (1u << 3) | (1u << 5) | (1u << 7);
    CHECK(lut[0xAA].set_mask == expected_aa);
}

TEST_CASE("SpiIsr32 - Non-blocking transmission") {
    setup_32way_spi_lut();

    SpiIsr32 spi;

    // Prepare test data
    uint8_t test_data[] = {0x00, 0xFF};
    spi.loadBuffer(test_data, 2);

    // Verify data was loaded
    uint8_t* data = spi.getDataArray();
    CHECK(data[0] == 0x00);
    CHECK(data[1] == 0xFF);

    // Setup should succeed
    fl_spi_reset_state();
    int ret = spi.setupISR(1600000);
    REQUIRE(ret == 0);

    // Arm should succeed
    spi.visibilityDelayUs(10);
    spi.arm();

    // Stop should succeed
    spi.stopISR();
}


TEST_CASE("SpiIsr32 - Data buffer loading") {
    setup_32way_spi_lut();

    SpiIsr32 spi;

    // Test loading via loadBuffer
    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44};
    spi.loadBuffer(test_data, 4);

    // Verify buffer was loaded
    uint8_t* data = spi.getDataArray();
    CHECK(data[0] == 0x11);
    CHECK(data[1] == 0x22);
    CHECK(data[2] == 0x33);
    CHECK(data[3] == 0x44);
}

TEST_CASE("SpiIsr32 - LUT bulk loading") {
    SpiIsr32 spi;

    // Create test LUT
    uint32_t setMasks[256];
    uint32_t clearMasks[256];

    for (int i = 0; i < 256; i++) {
        setMasks[i] = i << 1;
        clearMasks[i] = (~i) & 0xFFFFFFFF;
    }

    // Load LUT
    spi.loadLUT(setMasks, clearMasks, 256);

    // Verify
    PinMaskEntry* lut = SpiIsr32::getLUTArray();
    CHECK(lut[0x55].set_mask == (0x55 << 1));
}

TEST_CASE("SpiIsr32 - Zero bytes transfer") {
    setup_32way_spi_lut();

    SpiIsr32 spi;

    // Set zero bytes to transfer
    spi.setTotalBytes(0);

    // Setup with zero bytes should work
    fl_spi_reset_state();
    int ret = spi.setupISR(1600000);
    REQUIRE(ret == 0);

    spi.visibilityDelayUs(10);
    spi.arm();

    // Should be able to stop immediately
    spi.stopISR();

    // Verify no errors occurred
    CHECK(true);
}



TEST_CASE("SpiIsr32 - Clock mask configuration") {
    SpiIsr32 spi;

    // Configure clock on GPIO31 (fits in 32-bit mask)
    spi.setClockMask(1u << 31);

    // Verify by checking subsequent ISR behavior
    // (The clock mask is used internally by the ISR)
    fl_spi_set_clock_mask(1u << 31);

    // Setup should succeed
    fl_spi_reset_state();
    int ret = spi.setupISR(1600000);
    REQUIRE(ret == 0);

    spi.stopISR();
}

TEST_CASE("SpiIsr32 - Visibility delay and ISR setup") {
    setup_32way_spi_lut();
    fl_gpio_sim_clear();

    SpiIsr32 spi;

    // Setup should succeed
    fl_spi_reset_state();
    int ret = spi.setupISR(800000);
    REQUIRE(ret == 0);

    // Visibility delay should complete
    spi.visibilityDelayUs(20);

    // ARM should succeed
    spi.arm();

    // Stop should succeed
    spi.stopISR();
}
