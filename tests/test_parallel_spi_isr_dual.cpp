/*
  FastLED — Parallel Soft-SPI ISR Unit Tests (2-way Dual-SPI)
  ------------------------------------------------------------
  Tests the ISR engine with host simulation for 2-way Dual-SPI.
  Verifies GPIO event capture and ISR behavior without hardware.

  License: MIT (FastLED)
*/

// Note: FASTLED_SPI_HOST_SIMULATION defined via build system (meson.build)
#include "test.h"
#include "platforms/esp/32/parallel_spi/fl_parallel_spi_isr_rv.h"
#include "platforms/esp/32/parallel_spi/fl_parallel_spi_host_sim.h"

namespace {

/* Helper: Initialize 2-way pin mapping */
void setup_dual_spi_lut() {
    PinMaskEntry* lut = fl_spi_get_lut_array();

    // Map pins: GPIO0-1 for data, GPIO8 for clock
    uint32_t dataPinMasks[2] = {
        1u << 0,  // D0
        1u << 1   // D1
    };

    for (int v = 0; v < 256; v++) {
        uint32_t setMask = 0;
        uint32_t clearMask = 0;

        for (int b = 0; b < 2; b++) {
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

TEST_CASE("dual_spi_isr: basic 2-way transmission") {
    setup_dual_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Prepare test data: 0x00, 0x03
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;  // Both data pins low (00)
    data[1] = 0x03;  // Both data pins high (11)
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

TEST_CASE("dual_spi_isr: verify clock toggling") {
    setup_dual_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Single byte transmission
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x01;  // 01 pattern (D0 high, D1 low)
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

TEST_CASE("dual_spi_isr: verify data pattern 01") {
    setup_dual_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test pattern: 0x01 = 01 binary (D0 high, D1 low)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x01;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pins match pattern (bit 0 should be set)
    FL_GPIO_Event evt;
    bool foundDataSet = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if (evt.gpio_mask & 0x03) {  // Data pins (2 bits)
                CHECK((evt.gpio_mask & 0x03) == 0x01);
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

    fl_spi_platform_isr_stop();
}

TEST_CASE("dual_spi_isr: verify data pattern 10") {
    setup_dual_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test pattern: 0x02 = 10 binary (D0 low, D1 high)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x02;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pins match pattern (bit 1 should be set)
    FL_GPIO_Event evt;
    bool foundDataSet = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if (evt.gpio_mask & 0x03) {  // Data pins (2 bits)
                CHECK((evt.gpio_mask & 0x03) == 0x02);
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

    fl_spi_platform_isr_stop();
}

TEST_CASE("dual_spi_isr: all four patterns") {
    setup_dual_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test all four possible 2-bit patterns: 00, 01, 10, 11
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;  // 00
    data[1] = 0x01;  // 01
    data[2] = 0x02;  // 10
    data[3] = 0x03;  // 11
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

TEST_CASE("dual_spi_isr: zero bytes transfer") {
    setup_dual_spi_lut();
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

TEST_CASE("dual_spi_isr: alternating pattern") {
    setup_dual_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with alternating 01/10 pattern
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x01;  // 01
    data[1] = 0x02;  // 10
    data[2] = 0x01;  // 01
    data[3] = 0x02;  // 10
    fl_spi_set_total_bytes(4);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    fl_spi_platform_isr_stop();
}

TEST_CASE("dual_spi_isr: all ones pattern") {
    setup_dual_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with all ones (0xFF, but only lower 2 bits matter)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xFF;  // Lower 2 bits = 11
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify both data pins were set (lower 2 bits)
    FL_GPIO_Event evt;
    bool foundAllOnes = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if ((evt.gpio_mask & 0x03) == 0x03) {
                foundAllOnes = true;
            }
        }
    }

    CHECK(foundAllOnes);

    fl_spi_platform_isr_stop();
}

TEST_CASE("dual_spi_isr: upper bits ignored") {
    setup_dual_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test that upper 6 bits are ignored
    // 0xFD = 11111101 binary, lower 2 bits = 01
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xFD;  // Should behave same as 0x01
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify only lower 2 bits matter (should match 0x01 pattern)
    FL_GPIO_Event evt;
    bool foundCorrectPattern = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if (evt.gpio_mask & 0x03) {  // Data pins
                CHECK((evt.gpio_mask & 0x03) == 0x01);
                foundCorrectPattern = true;
            }
        }
    }

    CHECK(foundCorrectPattern);

    fl_spi_platform_isr_stop();
}
