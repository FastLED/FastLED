/*
  FastLED — Parallel Soft-SPI ISR Unit Tests (4-way Quad-SPI)
  ------------------------------------------------------------
  Tests the ISR engine with host simulation for 4-way Quad-SPI.
  Verifies GPIO event capture and ISR behavior without hardware.

  License: MIT (FastLED)
*/

// Note: FASTLED_SPI_HOST_SIMULATION defined via build system (meson.build)
#include "test.h"
#include "platforms/esp/32/parallel_spi/fl_parallel_spi_isr_rv.h"
#include "platforms/esp/32/parallel_spi/fl_parallel_spi_host_sim.h"

namespace {

/* Helper: Initialize 4-way pin mapping */
void setup_quad_spi_lut() {
    PinMaskEntry* lut = fl_spi_get_lut_array();

    // Map pins: GPIO0-3 for data, GPIO8 for clock
    uint32_t dataPinMasks[4] = {
        1u << 0,  // D0
        1u << 1,  // D1
        1u << 2,  // D2
        1u << 3   // D3
    };

    for (int v = 0; v < 256; v++) {
        uint32_t setMask = 0;
        uint32_t clearMask = 0;

        for (int b = 0; b < 4; b++) {
            if (v & (1 << b)) {
                setMask |= dataPinMasks[b];
            } else {
                clearMask |= dataPinMasks[b];
            }
        }

        lut[v].set_mask = setMask;
        lut[v].clear_mask = clearMask;
    }

    fl_spi_set_clock_mask(1u << 8);
}

/* Helper: Drive ISR until transfer completes */
void drive_isr_until_done(uint32_t max_ticks = 1000) {
    for (uint32_t i = 0; i < max_ticks; i++) {
        fl_spi_host_simulate_tick();

        if (!(fl_spi_status_flags() & FASTLED_STATUS_BUSY)) {
            return;  // Done
        }
    }
    FAIL("ISR did not complete within max_ticks");
}

}  // namespace

TEST_CASE("quad_spi_isr: basic 4-way transmission") {
    setup_quad_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Prepare test data: 0x00, 0x0F
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;  // All data pins low (0000)
    data[1] = 0x0F;  // All data pins high (1111)
    fl_spi_set_total_bytes(2);

    // Start ISR
    int ret = fl_spi_platform_isr_start(1600000);
    REQUIRE(ret == 0);

    // Arm transfer
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    // Drive ISR
    drive_isr_until_done();

    // Verify transfer completed
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    // Inspect ring buffer
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 0);

    // Stop ISR
    fl_spi_platform_isr_stop();
}

TEST_CASE("quad_spi_isr: verify clock toggling") {
    setup_quad_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Single byte transmission
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x05;  // 0101 pattern
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify clock toggles (should see clock mask in both SET and CLEAR events)
    FL_GPIO_Event evt;
    uint32_t clockSetCount = 0;
    uint32_t clockClearCount = 0;
    uint32_t clockMask = 1u << 8;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0 && (evt.gpio_mask & clockMask)) {
            clockSetCount++;
        }
        if (evt.event_type == 1 && (evt.gpio_mask & clockMask)) {
            clockClearCount++;
        }
    }

    CHECK(clockSetCount > 0);
    CHECK(clockClearCount > 0);
    CHECK(clockSetCount == clockClearCount);  // Balanced clock

    fl_spi_platform_isr_stop();
}

TEST_CASE("quad_spi_isr: verify data pattern") {
    setup_quad_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test pattern: 0x0A = 1010 binary
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x0A;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pins match pattern (bits 0 and 2 should be set)
    FL_GPIO_Event evt;
    bool foundDataSet = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if (evt.gpio_mask & 0x0F) {  // Data pins
                CHECK((evt.gpio_mask & 0x0F) == 0x0A);
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

    fl_spi_platform_isr_stop();
}

TEST_CASE("quad_spi_isr: multiple byte sequence") {
    setup_quad_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test sequence: 0x00, 0x0F, 0x0A, 0x05
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0x0F;
    data[2] = 0x0A;
    data[3] = 0x05;
    fl_spi_set_total_bytes(4);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify all bytes transmitted (should have 4 * 2 phases = 8 clock cycles)
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 8);  // At least SET+CLEAR per phase

    fl_spi_platform_isr_stop();
}

TEST_CASE("quad_spi_isr: zero bytes transfer") {
    setup_quad_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // No data to send
    fl_spi_set_total_bytes(0);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    // ISR should immediately complete with no data
    drive_isr_until_done(10);

    // Should be done immediately
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    // No events should be generated
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount == 0);

    fl_spi_platform_isr_stop();
}

TEST_CASE("quad_spi_isr: all ones pattern") {
    setup_quad_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with all ones (0xFF)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xFF;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify all data pins were set (only lower 4 bits matter for quad)
    FL_GPIO_Event evt;
    bool foundAllOnes = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if ((evt.gpio_mask & 0x0F) == 0x0F) {
                foundAllOnes = true;
            }
        }
    }

    CHECK(foundAllOnes);

    fl_spi_platform_isr_stop();
}

TEST_CASE("quad_spi_isr: alternating pattern") {
    setup_quad_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with alternating 0xAA pattern (but only lower 4 bits used)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xAA;  // Lower 4 bits = 1010
    data[1] = 0x55;  // Lower 4 bits = 0101
    fl_spi_set_total_bytes(2);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    fl_spi_platform_isr_stop();
}
