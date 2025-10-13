/*
  FastLED — Parallel Soft-SPI ISR Unit Tests (1-way Single-SPI)
  --------------------------------------------------------------
  Tests the ISR engine with host simulation for 1-way Single-SPI.
  Verifies GPIO event capture and ISR behavior without hardware.
  This is the simplest test case and useful for baseline validation.

  License: MIT (FastLED)
*/

// Note: FASTLED_SPI_HOST_SIMULATION defined via build system (meson.build)
#include "test.h"
#include "platforms/esp/32/parallel_spi/fl_parallel_spi_isr_rv.h"
#include "platforms/esp/32/parallel_spi/fl_parallel_spi_host_sim.h"

namespace {

/* Helper: Initialize 1-way pin mapping */
void setup_single_spi_lut() {
    PinMaskEntry* lut = fl_spi_get_lut_array();

    // Map pins: GPIO0 for data, GPIO8 for clock
    uint32_t dataPinMask = 1u << 0;  // D0

    for (int v = 0; v < 256; v++) {
        // Only process bit 0 (upper 7 bits ignored)
        if (v & 1) {
            // Bit 0 is set - set data pin high
            lut[v].set_mask = dataPinMask;
            lut[v].clear_mask = 0;
        } else {
            // Bit 0 is clear - clear data pin low
            lut[v].set_mask = 0;
            lut[v].clear_mask = dataPinMask;
        }
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

TEST_CASE("single_spi_isr: basic 1-way transmission") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Prepare test data: 0x00, 0x01
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;  // Data pin low (0)
    data[1] = 0x01;  // Data pin high (1)
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

TEST_CASE("single_spi_isr: verify clock toggling") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Single byte transmission
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x01;  // 1 pattern (data high)
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

TEST_CASE("single_spi_isr: verify data pattern 0") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test pattern: 0x00 = 0 binary (data low)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pin is cleared (should see CLEAR event with data pin mask)
    FL_GPIO_Event evt;
    bool foundDataClear = false;
    uint32_t dataPinMask = 1u << 0;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 1) {  // CLEAR event
            if (evt.gpio_mask & dataPinMask) {
                foundDataClear = true;
            }
        }
    }

    CHECK(foundDataClear);

    fl_spi_platform_isr_stop();
}

TEST_CASE("single_spi_isr: verify data pattern 1") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test pattern: 0x01 = 1 binary (data high)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x01;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pin is set (should see SET event with data pin mask)
    FL_GPIO_Event evt;
    bool foundDataSet = false;
    uint32_t dataPinMask = 1u << 0;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if (evt.gpio_mask & dataPinMask) {
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

    fl_spi_platform_isr_stop();
}

TEST_CASE("single_spi_isr: alternating pattern") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with alternating 0/1 pattern
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;  // 0
    data[1] = 0x01;  // 1
    data[2] = 0x00;  // 0
    data[3] = 0x01;  // 1
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

TEST_CASE("single_spi_isr: zero bytes transfer") {
    setup_single_spi_lut();
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

TEST_CASE("single_spi_isr: all ones byte") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with 0xFF (only bit 0 matters, so same as 0x01)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xFF;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pin was set
    FL_GPIO_Event evt;
    bool foundDataSet = false;
    uint32_t dataPinMask = 1u << 0;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if (evt.gpio_mask & dataPinMask) {
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

    fl_spi_platform_isr_stop();
}

TEST_CASE("single_spi_isr: upper bits ignored") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test that upper 7 bits are ignored
    // 0xFE = 11111110 binary, bit 0 = 0
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xFE;  // Should behave same as 0x00
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pin was cleared (not set)
    FL_GPIO_Event evt;
    bool foundDataClear = false;
    bool foundDataSet = false;
    uint32_t dataPinMask = 1u << 0;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0 && (evt.gpio_mask & dataPinMask)) {
            foundDataSet = true;
        }
        if (evt.event_type == 1 && (evt.gpio_mask & dataPinMask)) {
            foundDataClear = true;
        }
    }

    CHECK(foundDataClear);
    CHECK_FALSE(foundDataSet);  // Should NOT set data pin for 0xFE

    fl_spi_platform_isr_stop();
}

TEST_CASE("single_spi_isr: long sequence") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with a longer sequence (10 bytes)
    uint8_t* data = fl_spi_get_data_array();
    for (int i = 0; i < 10; i++) {
        data[i] = (i % 2);  // Alternating 0, 1, 0, 1, ...
    }
    fl_spi_set_total_bytes(10);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    // Should have many events (10 bytes * 2 phases * 2+ events per phase)
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount >= 20);

    fl_spi_platform_isr_stop();
}

TEST_CASE("single_spi_isr: max bytes transfer") {
    setup_single_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with maximum bytes (256)
    uint8_t* data = fl_spi_get_data_array();
    for (int i = 0; i < 256; i++) {
        data[i] = (i % 2);  // Alternating pattern
    }
    fl_spi_set_total_bytes(256);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done(2000);  // Need more ticks for 256 bytes

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    fl_spi_platform_isr_stop();
}
